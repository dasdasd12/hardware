#include "pc_link.h"

#include <stdio.h>
#include <string.h>

#include "aik_profile_format.h"
#include "approval_mailbox.h"
#include "ch585_link.h"
#include "profile_activate.h"
#include "profile_runtime.h"
#include "profile_store.h"
#include "profile_sync.h"

#ifndef V3F_ENABLE_USBFS_CDC
#define V3F_ENABLE_USBFS_CDC 0
#endif

#define PC_LINK_LINE_MAX     320U
#define PC_LINK_PACKS_PER_POLL 4U
#define PC_APPROVAL_SHOW_MAX_LINE_CHARS \
    ((sizeof("AK APPROVAL SHOW ") - 1u) + 8u + 1u + 1u + 1u + \
     (AIK_APPROVAL_TOOL_MAX * 2u) + 1u + \
     (AIK_APPROVAL_SUMMARY_MAX * 2u))

typedef char pc_approval_show_fits_cdc_line[
    (PC_APPROVAL_SHOW_MAX_LINE_CHARS < PC_LINK_LINE_MAX) ? 1 : -1];

#define PC_ERR_UNKNOWN   1
#define PC_ERR_ARGS      2
#define PC_ERR_STATE     3
#define PC_ERR_RANGE     4
#define PC_ERR_FLASH     5
#define PC_ERR_CRC       6
#define PC_ERR_PACKAGE   7

typedef struct
{
    uint8_t upload_active;
    uint8_t slot_id;
    uint32_t total_len;
    uint32_t expected_crc;
    uint32_t offset;
} pc_upload_t;

static pc_upload_t s_upload;
static uint8_t s_data_buf[128];
static void (*s_writer)(const char *line);

void v3f_pc_link_set_writer(void (*writer)(const char *line))
{
    s_writer = writer;
}

static void pc_reply(const char *line)
{
    if(s_writer != 0)
    {
        s_writer(line);
    }
}

static void pc_reply_err(int code, const char *detail)
{
    char buf[64];

    (void)snprintf(buf, sizeof(buf), "ERR %d %s\r\n", code, detail);
    pc_reply(buf);
}

static int hex_nibble(char c)
{
    if((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }
    if((c >= 'a') && (c <= 'f'))
    {
        return (c - 'a') + 10;
    }
    if((c >= 'A') && (c <= 'F'))
    {
        return (c - 'A') + 10;
    }
    return -1;
}

static int parse_hex_u32(const char *s, uint32_t *out)
{
    uint32_t value = 0U;
    uint8_t digits = 0U;

    while((*s != '\0') && (*s != ' '))
    {
        int nibble = hex_nibble(*s);

        if((nibble < 0) || (digits >= 8U))
        {
            return -1;
        }
        value = (value << 4) | (uint32_t)nibble;
        digits++;
        s++;
    }
    if(digits == 0U)
    {
        return -1;
    }
    *out = value;
    return 0;
}

typedef struct
{
    const char *data;
    uint16_t length;
} pc_token_t;

static uint8_t pc_token_equals(const pc_token_t *token, const char *text)
{
    uint16_t length = (uint16_t)strlen(text);

    return (uint8_t)((token->length == length) &&
                     (memcmp(token->data, text, length) == 0));
}

static int pc_take_token(const char **cursor, pc_token_t *token)
{
    const char *start;
    const char *end;

    if((cursor == 0) || (*cursor == 0) || (token == 0))
    {
        return -1;
    }

    start = *cursor;
    while(*start == ' ')
    {
        start++;
    }
    if(*start == '\0')
    {
        return -1;
    }

    end = start;
    while((*end != '\0') && (*end != ' '))
    {
        end++;
    }
    token->data = start;
    token->length = (uint16_t)(end - start);

    while(*end == ' ')
    {
        end++;
    }
    *cursor = end;
    return 0;
}

static uint8_t pc_tokens_finished(const char *cursor)
{
    if(cursor == 0)
    {
        return 1u;
    }
    while(*cursor == ' ')
    {
        cursor++;
    }
    return (uint8_t)(*cursor == '\0');
}

static int pc_parse_fixed_hex(const pc_token_t *token,
                              uint8_t digits,
                              uint32_t *value_out)
{
    uint32_t value = 0u;
    uint8_t index;

    if((token == 0) || (value_out == 0) ||
       (token->length != digits))
    {
        return -1;
    }
    for(index = 0u; index < digits; index++)
    {
        int nibble = hex_nibble(token->data[index]);

        if(nibble < 0)
        {
            return -1;
        }
        value = (value << 4) | (uint32_t)nibble;
    }
    *value_out = value;
    return 0;
}

#define PC_DECODE_ASCII_OK         0
#define PC_DECODE_ASCII_ERR_HEX   (-1)
#define PC_DECODE_ASCII_ERR_RANGE (-2)
#define PC_DECODE_ASCII_ERR_TEXT  (-3)

static int pc_decode_ascii_hex(const pc_token_t *token,
                               char *output,
                               uint16_t capacity,
                               uint16_t *length_out)
{
    uint16_t byte_count;
    uint16_t index;

    if((token == 0) || (output == 0) || (length_out == 0))
    {
        return PC_DECODE_ASCII_ERR_HEX;
    }
    if((token->length == 1u) && (token->data[0] == '-'))
    {
        *length_out = 0u;
        return PC_DECODE_ASCII_OK;
    }
    if((token->length == 0u) || ((token->length & 1u) != 0u))
    {
        return PC_DECODE_ASCII_ERR_HEX;
    }

    byte_count = (uint16_t)(token->length / 2u);
    if(byte_count > capacity)
    {
        return PC_DECODE_ASCII_ERR_RANGE;
    }

    for(index = 0u; index < byte_count; index++)
    {
        int high = hex_nibble(token->data[index * 2u]);
        int low = hex_nibble(token->data[(index * 2u) + 1u]);
        uint8_t value;

        if((high < 0) || (low < 0))
        {
            return PC_DECODE_ASCII_ERR_HEX;
        }
        value = (uint8_t)((high << 4) | low);
        if((value < 0x20u) || (value > 0x7eu))
        {
            return PC_DECODE_ASCII_ERR_TEXT;
        }
        output[index] = (char)value;
    }

    *length_out = byte_count;
    return PC_DECODE_ASCII_OK;
}

static const char *next_token(const char *s)
{
    while((*s != '\0') && (*s != ' '))
    {
        s++;
    }
    while(*s == ' ')
    {
        s++;
    }
    return (*s != '\0') ? s : 0;
}

static uint8_t slot_is_usable(uint8_t slot_id)
{
    const uint8_t *slot = v3f_profile_store_slot_ptr(slot_id);
    uint32_t index;

    if(slot == 0)
    {
        return 1U;
    }
    /*
     * An erased user slot is logically backed by its built-in preset until
     * the PC writes a real package into it, so report it as usable.
     */
    for(index = 0U; index < AIK_PKG_HEADER_SIZE; index++)
    {
        if(slot[index] != 0xFFU)
        {
            break;
        }
    }
    if(index == AIK_PKG_HEADER_SIZE)
    {
        return 1U;
    }

    /* INFO is a foreground PC request, so validate the stored package
     * fully instead of advertising a header-valid but CRC-bad slot. */
    return (uint8_t)(v3f_profile_package_validate(
                         slot, V3F_PROFILE_SLOT_SIZE, 0) ==
                     V3F_PROFILE_OK);
}

static void pc_cmd_info(void)
{
    const v3f_profile_runtime_t *rt = v3f_profile_runtime_get();
    char buf[128];

    (void)snprintf(buf, sizeof(buf),
                   "OK INFO active=%u id16=%04x gen=%u slots=%u%u%u "
                   "stored=%u%u%u sync=%u%u\r\n",
                   (unsigned int)rt->active_slot,
                   (unsigned int)rt->profile_id16,
                   (unsigned int)rt->generation16,
                   (unsigned int)slot_is_usable(1U),
                   (unsigned int)slot_is_usable(2U),
                   (unsigned int)slot_is_usable(3U),
                   (unsigned int)v3f_profile_store_slot_present(1U),
                   (unsigned int)v3f_profile_store_slot_present(2U),
                   (unsigned int)v3f_profile_store_slot_present(3U),
                   (unsigned int)v3f_profile_sync_half_synced(
                       AIK_HALF_ID_LEFT),
                   (unsigned int)v3f_profile_sync_half_synced(
                       AIK_HALF_ID_RIGHT));
    pc_reply(buf);
}

static void pc_cmd_begin(const char *args)
{
    const char *len_arg;
    const char *crc_arg;
    uint32_t slot;
    uint32_t total_len;
    uint32_t crc;

    len_arg = (args != 0) ? next_token(args) : 0;
    crc_arg = (len_arg != 0) ? next_token(len_arg) : 0;
    if((args == 0) || (len_arg == 0) || (crc_arg == 0) ||
       (parse_hex_u32(args, &slot) != 0) ||
       (parse_hex_u32(len_arg, &total_len) != 0) ||
       (parse_hex_u32(crc_arg, &crc) != 0))
    {
        pc_reply_err(PC_ERR_ARGS, "begin");
        return;
    }
    if((slot < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot >= AIK_PROFILE_SLOT_COUNT_TOTAL) ||
       (total_len < AIK_PKG_HEADER_SIZE) ||
       (total_len > V3F_PROFILE_SLOT_SIZE))
    {
        pc_reply_err(PC_ERR_RANGE, "begin");
        return;
    }

    if(v3f_profile_store_staging_begin(total_len) != V3F_PROFILE_STORE_OK)
    {
        pc_reply_err(PC_ERR_FLASH, "staging");
        return;
    }

    s_upload.upload_active = 1U;
    s_upload.slot_id = (uint8_t)slot;
    s_upload.total_len = total_len;
    s_upload.expected_crc = crc;
    s_upload.offset = 0U;
    pc_reply("OK BEGIN\r\n");
}

static void pc_cmd_data(const char *args)
{
    const char *hex;
    uint32_t offset;
    uint32_t count = 0U;
    char buf[32];

    hex = (args != 0) ? next_token(args) : 0;
    if((args == 0) || (hex == 0) || (parse_hex_u32(args, &offset) != 0))
    {
        pc_reply_err(PC_ERR_ARGS, "data");
        return;
    }
    if(s_upload.upload_active == 0U)
    {
        pc_reply_err(PC_ERR_STATE, "no-begin");
        return;
    }
    if(offset != s_upload.offset)
    {
        pc_reply_err(PC_ERR_RANGE, "offset");
        return;
    }

    while((hex[0] != '\0') && (hex[0] != ' '))
    {
        int high = hex_nibble(hex[0]);
        int low = (hex[1] != '\0') ? hex_nibble(hex[1]) : -1;

        if((high < 0) || (low < 0) || (count >= sizeof(s_data_buf)))
        {
            pc_reply_err(PC_ERR_ARGS, "hex");
            return;
        }
        s_data_buf[count++] = (uint8_t)((high << 4) | low);
        hex += 2;
    }
    if((count == 0U) || ((s_upload.offset + count) > s_upload.total_len))
    {
        pc_reply_err(PC_ERR_RANGE, "len");
        return;
    }

    if(v3f_profile_store_staging_write(s_upload.offset, s_data_buf,
                                       count) != V3F_PROFILE_STORE_OK)
    {
        s_upload.upload_active = 0U;
        pc_reply_err(PC_ERR_FLASH, "write");
        return;
    }

    s_upload.offset += count;
    (void)snprintf(buf, sizeof(buf), "OK DATA %lx\r\n",
                   (unsigned long)s_upload.offset);
    pc_reply(buf);
}

static void pc_cmd_commit(void)
{
    const uint8_t *staging = v3f_profile_store_staging_ptr();
    char buf[32];

    if((s_upload.upload_active == 0U) ||
       (s_upload.offset != s_upload.total_len))
    {
        pc_reply_err(PC_ERR_STATE, "incomplete");
        return;
    }
    if(v3f_profile_store_staging_finish() != V3F_PROFILE_STORE_OK)
    {
        s_upload.upload_active = 0U;
        pc_reply_err(PC_ERR_FLASH, "finish");
        return;
    }
    if(aik_crc32c(0U, staging, s_upload.total_len) != s_upload.expected_crc)
    {
        s_upload.upload_active = 0U;
        pc_reply_err(PC_ERR_CRC, "crc32c");
        return;
    }
    if(v3f_profile_package_validate(staging, s_upload.total_len,
                                    0) != V3F_PROFILE_OK)
    {
        s_upload.upload_active = 0U;
        pc_reply_err(PC_ERR_PACKAGE, "akpk");
        return;
    }
    if(v3f_profile_store_commit_staging_to_slot(
           s_upload.slot_id, s_upload.total_len) != V3F_PROFILE_STORE_OK)
    {
        s_upload.upload_active = 0U;
        pc_reply_err(PC_ERR_FLASH, "commit");
        return;
    }

    (void)snprintf(buf, sizeof(buf), "OK COMMIT %u\r\n",
                   (unsigned int)s_upload.slot_id);
    s_upload.upload_active = 0U;
    pc_reply(buf);
}

static void pc_cmd_activate(const char *args)
{
    uint32_t slot;
    int status;
    char buf[64];

    if((args == 0) || (parse_hex_u32(args, &slot) != 0) ||
       (slot >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        pc_reply_err(PC_ERR_ARGS, "activate");
        return;
    }

    status = v3f_profile_activate_slot((uint8_t)slot);
    if(status == V3F_PROFILE_ACTIVATE_ERR_META)
    {
        pc_reply_err(PC_ERR_FLASH, "meta");
        return;
    }
    if(status != V3F_PROFILE_ACTIVATE_OK)
    {
        pc_reply_err(PC_ERR_PACKAGE, "install");
        return;
    }

    (void)snprintf(buf, sizeof(buf), "OK ACTIVATE %u id16=%04x gen=%u\r\n",
                   (unsigned int)slot,
                   (unsigned int)v3f_profile_runtime_get()->profile_id16,
                   (unsigned int)v3f_profile_runtime_get()->generation16);
    pc_reply(buf);
}

static int pc_parse_half_id(const char *args, uint8_t *half_id)
{
    uint32_t value;

    if((args == 0) || (half_id == 0) || (next_token(args) != 0) ||
       (parse_hex_u32(args, &value) != 0) ||
       (value > AIK_HALF_ID_RIGHT))
    {
        return -1;
    }
    *half_id = (uint8_t)value;
    return 0;
}

static void pc_cmd_sync(const char *args)
{
    v3f_profile_sync_diag_t diag;
    uint8_t half_id;
    char buf[64];

    if(pc_parse_half_id(args, &half_id) != 0)
    {
        pc_reply_err(PC_ERR_ARGS, "sync");
        return;
    }
    memset(&diag, 0, sizeof(diag));
    if(v3f_profile_sync_get_diag(half_id, &diag) == 0U)
    {
        pc_reply_err(PC_ERR_STATE, "sync");
        return;
    }

    (void)snprintf(buf, sizeof(buf),
                   "OK SYNC %u s=%u r=%u o=%u n=%u w=%u b=%u\r\n",
                   (unsigned int)half_id,
                   (unsigned int)diag.state,
                   (unsigned int)diag.retries,
                   (unsigned int)diag.offset,
                   (unsigned int)diag.patch_len,
                   (unsigned int)diag.commit_waits,
                   (unsigned int)diag.backoff);
    pc_reply(buf);
}

static void pc_cmd_link(const char *args)
{
    v3f_ch585_link_stats_t stats;
    uint8_t half_id;
    char buf[192];

    if(pc_parse_half_id(args, &half_id) != 0)
    {
        pc_reply_err(PC_ERR_ARGS, "link");
        return;
    }
    memset(&stats, 0, sizeof(stats));
    v3f_ch585_link_stats(half_id, &stats);

    (void)snprintf(buf, sizeof(buf),
                   "OK LINK %u e=%lu i=%lu c=%lu p=%lu q=%lu a=%u s=%u o=%u d=%u m=%02x t=%02x\r\n",
                   (unsigned int)half_id,
                   (unsigned long)stats.link_errors,
                   (unsigned long)stats.invalid_frames,
                   (unsigned long)stats.command_phase_frames,
                   (unsigned long)stats.profile_status_ok,
                   (unsigned long)stats.profile_status_invalid,
                   (unsigned int)stats.last_profile_response.xfer.ack_seq,
                   (unsigned int)stats.last_profile_response.xfer.state,
                   (unsigned int)stats.last_profile_response.xfer.received_len,
                   (unsigned int)stats.last_profile_response.xfer.detail,
                   (unsigned int)stats.last_magic,
                   (unsigned int)stats.last_type);
    pc_reply(buf);
}

static void pc_cmd_delete(const char *args)
{
    uint32_t slot;
    uint8_t was_active;
    int status;
    char buf[32];

    if((args == 0) || (next_token(args) != 0) ||
       (parse_hex_u32(args, &slot) != 0) ||
       (slot < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        pc_reply_err(PC_ERR_ARGS, "delete");
        return;
    }

    was_active = (uint8_t)(v3f_profile_runtime_get()->active_slot ==
                           (uint8_t)slot);
    status = v3f_profile_store_delete_slot((uint8_t)slot);
    if(status != V3F_PROFILE_STORE_OK)
    {
        pc_reply_err(PC_ERR_FLASH, "delete");
        return;
    }

    if((was_active != 0U) &&
       (v3f_profile_activate_slot(AIK_PROFILE_SLOT_FACTORY) !=
        V3F_PROFILE_ACTIVATE_OK))
    {
        pc_reply_err(PC_ERR_PACKAGE, "delete-install");
        return;
    }

    (void)snprintf(buf, sizeof(buf), "OK DELETE %u\r\n",
                   (unsigned int)slot);
    pc_reply(buf);
}

static void pc_cmd_approval_show(const char *args)
{
    pc_token_t tag_token;
    pc_token_t risk_token;
    pc_token_t tool_token;
    pc_token_t summary_token;
    uint32_t tag;
    uint32_t risk;
    char tool[AIK_APPROVAL_TOOL_MAX];
    char summary[AIK_APPROVAL_SUMMARY_MAX];
    uint16_t tool_len;
    uint16_t summary_len;
    int tool_status;
    int summary_status;
    char reply[96];

    if((pc_take_token(&args, &tag_token) != 0) ||
       (pc_take_token(&args, &risk_token) != 0) ||
       (pc_take_token(&args, &tool_token) != 0) ||
       (pc_take_token(&args, &summary_token) != 0) ||
       (pc_tokens_finished(args) == 0u) ||
       (pc_parse_fixed_hex(&tag_token, 8u, &tag) != 0) ||
       (pc_parse_fixed_hex(&risk_token, 1u, &risk) != 0))
    {
        pc_reply_err(PC_ERR_ARGS, "approval-show");
        return;
    }
    if(risk > AIK_APPROVAL_RISK_MAX)
    {
        pc_reply_err(PC_ERR_RANGE, "approval-risk");
        return;
    }

    tool_status = pc_decode_ascii_hex(&tool_token,
                                      tool,
                                      sizeof(tool),
                                      &tool_len);
    summary_status = pc_decode_ascii_hex(&summary_token,
                                         summary,
                                         sizeof(summary),
                                         &summary_len);
    if((tool_status == PC_DECODE_ASCII_ERR_RANGE) ||
       (summary_status == PC_DECODE_ASCII_ERR_RANGE))
    {
        pc_reply_err(PC_ERR_RANGE, "approval-text");
        return;
    }
    if((tool_status != PC_DECODE_ASCII_OK) ||
       (summary_status != PC_DECODE_ASCII_OK))
    {
        pc_reply_err(PC_ERR_ARGS, "approval-text");
        return;
    }

    if(v3f_approval_mailbox_show(tag,
                                 (uint8_t)risk,
                                 tool,
                                 tool_len,
                                 summary,
                                 summary_len) !=
       V3F_APPROVAL_MAILBOX_OK)
    {
        pc_reply_err(PC_ERR_RANGE, "approval-mailbox");
        return;
    }

    (void)snprintf(reply, sizeof(reply),
                   "OK APPROVAL SHOW %08lx risk=%lx tool=%u summary=%u\r\n",
                   (unsigned long)tag,
                   (unsigned long)risk,
                   (unsigned int)tool_len,
                   (unsigned int)summary_len);
    pc_reply(reply);
}

static void pc_cmd_approval_clear(const char *args)
{
    pc_token_t tag_token;
    uint32_t tag;
    int status;
    char reply[48];

    if((pc_take_token(&args, &tag_token) != 0) ||
       (pc_tokens_finished(args) == 0u) ||
       (pc_parse_fixed_hex(&tag_token, 8u, &tag) != 0))
    {
        pc_reply_err(PC_ERR_ARGS, "approval-clear");
        return;
    }

    status = v3f_approval_mailbox_clear(tag);
    if(status == V3F_APPROVAL_MAILBOX_ERR_STATE)
    {
        pc_reply_err(PC_ERR_STATE, "approval-inactive");
        return;
    }
    if(status == V3F_APPROVAL_MAILBOX_ERR_TAG)
    {
        pc_reply_err(PC_ERR_STATE, "approval-tag");
        return;
    }
    if(status != V3F_APPROVAL_MAILBOX_OK)
    {
        pc_reply_err(PC_ERR_RANGE, "approval-mailbox");
        return;
    }

    (void)snprintf(reply, sizeof(reply),
                   "OK APPROVAL CLEAR %08lx\r\n",
                   (unsigned long)tag);
    pc_reply(reply);
}

static void pc_cmd_approval(const char *args)
{
    pc_token_t action;

    if(pc_take_token(&args, &action) != 0)
    {
        pc_reply_err(PC_ERR_ARGS, "approval");
        return;
    }
    if(pc_token_equals(&action, "SHOW") != 0u)
    {
        pc_cmd_approval_show(args);
    }
    else if(pc_token_equals(&action, "CLEAR") != 0u)
    {
        pc_cmd_approval_clear(args);
    }
    else
    {
        pc_reply_err(PC_ERR_UNKNOWN, "approval");
    }
}

static void pc_cmd_claude_state(const char *args)
{
    pc_token_t state_token;
    uint8_t state;
    const char *state_name;
    char reply[40];

    if((pc_take_token(&args, &state_token) != 0) ||
       (pc_tokens_finished(args) == 0u))
    {
        pc_reply_err(PC_ERR_ARGS, "claude-state");
        return;
    }

    if(pc_token_equals(&state_token, "OFF") != 0u)
    {
        state = AIK_CLAUDE_STATE_OFF;
        state_name = "OFF";
    }
    else if(pc_token_equals(&state_token, "RUNNING") != 0u)
    {
        state = AIK_CLAUDE_STATE_RUNNING;
        state_name = "RUNNING";
    }
    else if(pc_token_equals(&state_token, "DONE") != 0u)
    {
        state = AIK_CLAUDE_STATE_DONE;
        state_name = "DONE";
    }
    else
    {
        pc_reply_err(PC_ERR_RANGE, "claude-state");
        return;
    }

    if(v3f_approval_mailbox_set_claude_state(state) !=
       V3F_APPROVAL_MAILBOX_OK)
    {
        pc_reply_err(PC_ERR_RANGE, "claude-state");
        return;
    }

    (void)snprintf(reply, sizeof(reply),
                   "OK CLAUDE STATE %s\r\n", state_name);
    pc_reply(reply);
}

static void pc_cmd_claude(const char *args)
{
    pc_token_t action;

    if(pc_take_token(&args, &action) != 0)
    {
        pc_reply_err(PC_ERR_ARGS, "claude");
        return;
    }
    if(pc_token_equals(&action, "STATE") != 0u)
    {
        pc_cmd_claude_state(args);
    }
    else
    {
        pc_reply_err(PC_ERR_UNKNOWN, "claude");
    }
}

void v3f_pc_link_handle_line(const char *line)
{
    const char *args;

    if(strncmp(line, "AK ", 3U) != 0)
    {
        return; /* not for us: tolerate stray terminal input */
    }
    line += 3;

    if(strncmp(line, "PING", 4U) == 0)
    {
        pc_reply("OK PONG 1\r\n");
    }
    else if(strncmp(line, "INFO", 4U) == 0)
    {
        pc_cmd_info();
    }
    else if((strncmp(line, "SYNC", 4U) == 0) &&
            ((line[4] == '\0') || (line[4] == ' ')))
    {
        args = next_token(line);
        pc_cmd_sync(args);
    }
    else if((strncmp(line, "LINK", 4U) == 0) &&
            ((line[4] == '\0') || (line[4] == ' ')))
    {
        args = next_token(line);
        pc_cmd_link(args);
    }
    else if(strncmp(line, "BEGIN", 5U) == 0)
    {
        args = next_token(line);
        pc_cmd_begin(args);
    }
    else if(strncmp(line, "DATA", 4U) == 0)
    {
        args = next_token(line);
        pc_cmd_data(args);
    }
    else if(strncmp(line, "COMMIT", 6U) == 0)
    {
        pc_cmd_commit();
    }
    else if(strncmp(line, "ABORT", 5U) == 0)
    {
        v3f_profile_store_staging_abort();
        memset(&s_upload, 0, sizeof(s_upload));
        pc_reply("OK ABORT\r\n");
    }
    else if(strncmp(line, "FACTORY", 7U) == 0)
    {
        pc_cmd_activate("0");
    }
    else if(strncmp(line, "ACTIVATE", 8U) == 0)
    {
        args = next_token(line);
        pc_cmd_activate(args);
    }
    else if(strncmp(line, "DELETE", 6U) == 0)
    {
        args = next_token(line);
        pc_cmd_delete(args);
    }
    else if((strncmp(line, "APPROVAL", 8U) == 0) &&
            ((line[8] == '\0') || (line[8] == ' ')))
    {
        args = next_token(line);
        pc_cmd_approval(args);
    }
    else if((strncmp(line, "CLAUDE", 6U) == 0) &&
            ((line[6] == '\0') || (line[6] == ' ')))
    {
        args = next_token(line);
        pc_cmd_claude(args);
    }
    else
    {
        pc_reply_err(PC_ERR_UNKNOWN, "cmd");
    }
}

#if V3F_ENABLE_USBFS_CDC

#include "ch32h417_usbfs_device.h"
#include "ch32h417_usbfs_hid_nkro.h"

static char s_line[PC_LINK_LINE_MAX];
static uint16_t s_line_len;
static uint8_t s_line_overflow;

static void pc_feed_byte(char c)
{
    if((c == '\n') || (c == '\r'))
    {
        if(s_line_overflow != 0U)
        {
            s_line_overflow = 0U;
            s_line_len = 0U;
            pc_reply_err(PC_ERR_ARGS, "line-too-long");
            return;
        }
        if(s_line_len != 0U)
        {
            s_line[s_line_len] = '\0';
            v3f_pc_link_handle_line(s_line);
            s_line_len = 0U;
        }
        return;
    }
    if(s_line_len < (PC_LINK_LINE_MAX - 1U))
    {
        s_line[s_line_len++] = c;
    }
    else
    {
        s_line_overflow = 1U;
    }
}

static void pc_cdc_write(const char *line)
{
    (void)ch32h417_usbfs_hid_nkro_debug_write(line);
}

void v3f_pc_link_poll(void)
{
    uint8_t packs = 0U;

    if(s_writer == 0)
    {
        v3f_pc_link_set_writer(pc_cdc_write);
    }

    while((USBFS_RingBuffer_Comm.RemainPack != 0U) &&
          (packs < PC_LINK_PACKS_PER_POLL))
    {
        uint8_t deal = USBFS_RingBuffer_Comm.DealPtr;
        uint8_t len = USBFS_RingBuffer_Comm.PackLen[deal];
        const uint8_t *data =
            &USBFS_Data_Buffer[(uint16_t)deal * DEF_USBD_FS_PACK_SIZE];
        uint8_t i;

        for(i = 0U; i < len; i++)
        {
            pc_feed_byte((char)data[i]);
        }

        NVIC_DisableIRQ(USBFS_IRQn);
        USBFS_RingBuffer_Comm.RemainPack--;
        USBFS_RingBuffer_Comm.DealPtr++;
        if(USBFS_RingBuffer_Comm.DealPtr == DEF_Ring_Buffer_Max_Blks)
        {
            USBFS_RingBuffer_Comm.DealPtr = 0U;
        }
        NVIC_EnableIRQ(USBFS_IRQn);
        packs++;
    }

    /* Re-arm EP1 OUT if the ISR paused reception on a full ring. */
    if((USBFS_RingBuffer_Comm.StopFlag != 0U) &&
       (USBFS_RingBuffer_Comm.RemainPack <
        (DEF_Ring_Buffer_Max_Blks - DEF_RING_BUFFER_REMINE)))
    {
        NVIC_DisableIRQ(USBFS_IRQn);
        USBFS_RingBuffer_Comm.StopFlag = 0U;
        USBFSD->UEP1_RX_CTRL =
            (USBFSD->UEP1_RX_CTRL & ~USBFS_UEP_R_RES_MASK) |
            USBFS_UEP_R_RES_ACK;
        NVIC_EnableIRQ(USBFS_IRQn);
    }
}

#else /* !V3F_ENABLE_USBFS_CDC */

void v3f_pc_link_poll(void)
{
}

#endif
