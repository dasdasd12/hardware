#include "pc_link.h"

#include <stdio.h>
#include <string.h>

#include "aik_profile_format.h"
#include "profile_runtime.h"
#include "profile_store.h"
#include "profile_sync.h"
#include "factory_profile_image.h"

#ifndef V3F_ENABLE_USBFS_CDC
#define V3F_ENABLE_USBFS_CDC 0
#endif

#define PC_LINK_LINE_MAX     320U
#define PC_LINK_PACKS_PER_POLL 4U

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

static uint8_t slot_has_valid_package(uint8_t slot_id)
{
    const uint8_t *slot = v3f_profile_store_slot_ptr(slot_id);
    const aik_pkg_header_t *hdr = (const aik_pkg_header_t *)slot;

    if(slot == 0)
    {
        return 0U;
    }
    /* Light header check only; full CRC validation runs on ACTIVATE. */
    return (uint8_t)((hdr->magic[0] == AIK_PKG_MAGIC0) &&
                     (hdr->magic[1] == AIK_PKG_MAGIC1) &&
                     (hdr->magic[2] == AIK_PKG_MAGIC2) &&
                     (hdr->magic[3] == AIK_PKG_MAGIC3) &&
                     (hdr->total_size >= AIK_PKG_HEADER_SIZE) &&
                     (hdr->total_size <= V3F_PROFILE_SLOT_SIZE));
}

static void pc_cmd_info(void)
{
    const v3f_profile_runtime_t *rt = v3f_profile_runtime_get();
    char buf[96];

    (void)snprintf(buf, sizeof(buf),
                   "OK INFO active=%u id16=%04x gen=%u slots=%u%u%u\r\n",
                   (unsigned int)rt->active_slot,
                   (unsigned int)rt->profile_id16,
                   (unsigned int)rt->generation16,
                   (unsigned int)slot_has_valid_package(1U),
                   (unsigned int)slot_has_valid_package(2U),
                   (unsigned int)slot_has_valid_package(3U));
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

    if(slot == AIK_PROFILE_SLOT_FACTORY)
    {
        status = v3f_profile_runtime_install_package(
            g_v3f_factory_profile_image,
            g_v3f_factory_profile_image_size,
            AIK_PROFILE_SLOT_FACTORY);
    }
    else
    {
        const uint8_t *pkg = v3f_profile_store_slot_ptr((uint8_t)slot);

        status = (pkg != 0) ?
                 v3f_profile_runtime_install_package(pkg,
                                                     V3F_PROFILE_SLOT_SIZE,
                                                     (uint8_t)slot) :
                 V3F_PROFILE_ERR_PARAM;
    }

    if(status != V3F_PROFILE_OK)
    {
        pc_reply_err(PC_ERR_PACKAGE, "install");
        return;
    }
    if(v3f_profile_store_set_active_slot((uint8_t)slot) !=
       V3F_PROFILE_STORE_OK)
    {
        pc_reply_err(PC_ERR_FLASH, "meta");
        return;
    }

    v3f_profile_sync_mark_all_dirty();
    (void)snprintf(buf, sizeof(buf), "OK ACTIVATE %u id16=%04x gen=%u\r\n",
                   (unsigned int)slot,
                   (unsigned int)v3f_profile_runtime_get()->profile_id16,
                   (unsigned int)v3f_profile_runtime_get()->generation16);
    pc_reply(buf);
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
    (void)ch32h417_usbfs_hid_nkro_cdc_write(line);
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
