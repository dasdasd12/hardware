/*
 * Host test for the V3F PC configuration protocol (pc_link command
 * core). The profile store is emulated in RAM, so the full upload
 * flow (BEGIN/DATA/COMMIT/ACTIVATE) runs against the real AKPK
 * validation and runtime install paths using the generated factory
 * package as payload.
 *
 * Build and run from hardware:
 *   make -C tests/host run
 */

#include <stdio.h>
#include <string.h>

#include "aik_profile_format.h"
#include "approval_mailbox.h"
#include "pc_link.h"
#include "profile_runtime.h"
#include "profile_store.h"
#include "ch585_link.h"
#include "factory_profile_image.h"

static int s_failures;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

/* ------------------------------------------------------------------ */
/* RAM-backed profile store emulation                                 */
/* ------------------------------------------------------------------ */

static uint8_t s_slots[3][V3F_PROFILE_SLOT_SIZE];
static uint8_t s_staging[V3F_PROFILE_SLOT_SIZE];
static uint8_t s_active_slot;
static uint32_t s_staging_expected;
static uint32_t s_staging_written;
static uint8_t s_staging_active;
static uint8_t s_fail_set_active;
static uint32_t s_set_active_calls;

/* Approval mailbox is hardware-fixed in product code.  This parser test
 * stubs the public API so it never dereferences 0x20178800 on the host. */
static uint8_t s_approval_active;
static uint8_t s_approval_selected_yes;
static uint8_t s_approval_risk;
static uint8_t s_claude_state;
static uint32_t s_approval_tag;
static uint16_t s_approval_tool_len;
static uint16_t s_approval_summary_len;
static char s_approval_tool[AIK_APPROVAL_TOOL_MAX];
static char s_approval_summary[AIK_APPROVAL_SUMMARY_MAX];

void v3f_approval_mailbox_init(void)
{
    s_approval_active = 0u;
    s_approval_selected_yes = 0u;
    s_claude_state = AIK_CLAUDE_STATE_OFF;
}

uint8_t v3f_approval_mailbox_active(void)
{
    return s_approval_active;
}

uint8_t v3f_approval_mailbox_selected_yes(void)
{
    return s_approval_selected_yes;
}

void v3f_approval_mailbox_set_selected_yes(uint8_t selected_yes)
{
    s_approval_selected_yes = (uint8_t)(selected_yes != 0u);
}

int v3f_approval_mailbox_set_claude_state(uint8_t claude_state)
{
    if(claude_state > AIK_CLAUDE_STATE_DONE)
    {
        return V3F_APPROVAL_MAILBOX_ERR_PARAM;
    }

    s_claude_state = claude_state;
    if(claude_state == AIK_CLAUDE_STATE_OFF)
    {
        s_approval_active = 0u;
        s_approval_selected_yes = 0u;
    }
    return V3F_APPROVAL_MAILBOX_OK;
}

int v3f_approval_mailbox_show(uint32_t request_tag,
                              uint8_t risk,
                              const char *tool,
                              uint16_t tool_len,
                              const char *summary,
                              uint16_t summary_len)
{
    if((risk > AIK_APPROVAL_RISK_MAX) ||
       (tool_len > AIK_APPROVAL_TOOL_MAX) ||
       (summary_len > AIK_APPROVAL_SUMMARY_MAX))
    {
        return V3F_APPROVAL_MAILBOX_ERR_PARAM;
    }
    memset(s_approval_tool, 0, sizeof(s_approval_tool));
    memset(s_approval_summary, 0, sizeof(s_approval_summary));
    if(tool_len != 0u)
    {
        memcpy(s_approval_tool, tool, tool_len);
    }
    if(summary_len != 0u)
    {
        memcpy(s_approval_summary, summary, summary_len);
    }
    s_approval_tag = request_tag;
    s_approval_risk = risk;
    s_approval_tool_len = tool_len;
    s_approval_summary_len = summary_len;
    s_approval_active = 1u;
    s_approval_selected_yes = 0u;
    return V3F_APPROVAL_MAILBOX_OK;
}

int v3f_approval_mailbox_clear(uint32_t request_tag)
{
    if(s_approval_active == 0u)
    {
        return V3F_APPROVAL_MAILBOX_ERR_STATE;
    }
    if(request_tag != s_approval_tag)
    {
        return V3F_APPROVAL_MAILBOX_ERR_TAG;
    }
    s_approval_active = 0u;
    return V3F_APPROVAL_MAILBOX_OK;
}

const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id)
{
    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return 0;
    }
    return s_slots[slot_id - AIK_PROFILE_USER_SLOT_FIRST];
}

uint8_t v3f_profile_store_slot_present(uint8_t slot_id)
{
    uint32_t index;
    const uint8_t *slot = v3f_profile_store_slot_ptr(slot_id);

    if(slot == 0)
    {
        return 0U;
    }
    for(index = 0U; index < AIK_PKG_HEADER_SIZE; index++)
    {
        if(slot[index] != 0xFFU)
        {
            return 1U;
        }
    }
    return 0U;
}

const uint8_t *v3f_profile_store_staging_ptr(void)
{
    return s_staging;
}

int v3f_profile_store_staging_begin(uint32_t total_len)
{
    if((total_len == 0U) || (total_len > V3F_PROFILE_SLOT_SIZE))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    memset(s_staging, 0xFF, sizeof(s_staging));
    s_staging_expected = total_len;
    s_staging_written = 0U;
    s_staging_active = 1U;
    return V3F_PROFILE_STORE_OK;
}

int v3f_profile_store_staging_write(uint32_t offset, const uint8_t *data,
                                    uint32_t len)
{
    if((s_staging_active == 0U) || (offset != s_staging_written) ||
       ((offset + len) > s_staging_expected))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    memcpy(&s_staging[offset], data, len);
    s_staging_written += len;
    return V3F_PROFILE_STORE_OK;
}

int v3f_profile_store_staging_finish(void)
{
    if((s_staging_active == 0U) ||
       (s_staging_written != s_staging_expected))
    {
        return V3F_PROFILE_STORE_ERR_STATE;
    }
    s_staging_active = 0U;
    return V3F_PROFILE_STORE_OK;
}

void v3f_profile_store_staging_abort(void)
{
    s_staging_active = 0U;
    s_staging_written = 0U;
}

int v3f_profile_store_commit_staging_to_slot(uint8_t slot_id, uint32_t len)
{
    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL) ||
       (len > V3F_PROFILE_SLOT_SIZE))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    memset(s_slots[slot_id - AIK_PROFILE_USER_SLOT_FIRST], 0xFF,
           V3F_PROFILE_SLOT_SIZE);
    memcpy(s_slots[slot_id - AIK_PROFILE_USER_SLOT_FIRST], s_staging, len);
    return V3F_PROFILE_STORE_OK;
}

int v3f_profile_store_delete_slot(uint8_t slot_id)
{
    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    memset(s_slots[slot_id - AIK_PROFILE_USER_SLOT_FIRST], 0xFF,
           V3F_PROFILE_SLOT_SIZE);
    if(s_active_slot == slot_id)
    {
        s_active_slot = AIK_PROFILE_SLOT_FACTORY;
    }
    return V3F_PROFILE_STORE_OK;
}

uint8_t v3f_profile_store_get_active_slot(void)
{
    return s_active_slot;
}

int v3f_profile_store_set_active_slot(uint8_t slot_id)
{
    s_set_active_calls++;
    if(slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL)
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    if(s_fail_set_active != 0U)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    s_active_slot = slot_id;
    return V3F_PROFILE_STORE_OK;
}

/* Fake CH585 link: profile sync gets no medium in this test. */
uint8_t v3f_ch585_link_profile_cmd(uint8_t half_id,
                                   const aik_spi_host_cmd_v1_t *cmd,
                                   aik_spi_profile_xfer_v1_t *out)
{
    (void)half_id; (void)cmd; (void)out;
    return 0U;
}

uint8_t v3f_ch585_link_query_profile_status(uint8_t half_id,
                                            uint16_t host_seq,
                                            aik_spi_profile_status_v1_t *out)
{
    (void)half_id; (void)host_seq; (void)out;
    return 0U;
}

void v3f_ch585_link_stats(uint8_t half_id, v3f_ch585_link_stats_t *stats)
{
    if(stats == 0)
    {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    stats->link_errors = (uint32_t)(10U + half_id);
    stats->invalid_frames = (uint32_t)(20U + half_id);
    stats->command_phase_frames = (uint32_t)(30U + half_id);
    stats->profile_status_ok = (uint32_t)(40U + half_id);
    stats->profile_status_invalid = (uint32_t)(50U + half_id);
    stats->last_magic = 0xD7U;
    stats->last_type = 0x11U;
}

/* ------------------------------------------------------------------ */
/* Reply capture                                                      */
/* ------------------------------------------------------------------ */

static char s_reply[256];

static void reply_capture(const char *line)
{
    (void)snprintf(s_reply, sizeof(s_reply), "%s", line);
}

static const char *send(const char *line)
{
    s_reply[0] = '\0';
    v3f_pc_link_handle_line(line);
    return s_reply;
}

static uint8_t reply_starts(const char *prefix)
{
    return (uint8_t)(strncmp(s_reply, prefix, strlen(prefix)) == 0);
}

static void set_package_revision(uint8_t *pkg, uint32_t revision)
{
    aik_pkg_header_t *hdr = (aik_pkg_header_t *)pkg;

    hdr->revision = revision;
    hdr->package_crc32c = 0U;
    hdr->package_crc32c = aik_crc32c(0U, pkg, hdr->total_size);
}

/* ------------------------------------------------------------------ */

static void upload_package(const uint8_t *pkg, uint32_t len,
                           uint8_t slot_id, uint32_t chunk)
{
    char line[400];
    uint32_t offset = 0U;

    (void)snprintf(line, sizeof(line), "AK BEGIN %x %lx %lx",
                   (unsigned int)slot_id,
                   (unsigned long)len,
                   (unsigned long)aik_crc32c(0U, pkg, len));
    send(line);
    CHECK(reply_starts("OK BEGIN"));

    while(offset < len)
    {
        uint32_t take = ((len - offset) < chunk) ? (len - offset) : chunk;
        int pos;
        uint32_t i;

        pos = snprintf(line, sizeof(line), "AK DATA %lx ",
                       (unsigned long)offset);
        for(i = 0U; i < take; i++)
        {
            pos += snprintf(&line[pos], sizeof(line) - (size_t)pos,
                            "%02x", pkg[offset + i]);
        }
        send(line);
        if(!reply_starts("OK DATA"))
        {
            s_failures++;
            printf("FAIL upload at offset %lu: %s\n",
                   (unsigned long)offset, s_reply);
            return;
        }
        offset += take;
    }

    send("AK COMMIT");
}

int main(void)
{
    memset(s_slots, 0xFF, sizeof(s_slots));
    v3f_pc_link_set_writer(reply_capture);
    v3f_approval_mailbox_init();

    CHECK(v3f_profile_runtime_init() == AIK_PROFILE_SLOT_FACTORY);

    send("AK PING");
    CHECK(reply_starts("OK PONG"));

    send("AK INFO");
    CHECK(reply_starts("OK INFO active=0"));
    CHECK(strstr(s_reply, "slots=111") != 0);

    send("AK SYNC 0");
    CHECK(reply_starts("OK SYNC 0 s=0"));
    send("AK SYNC 1");
    CHECK(reply_starts("OK SYNC 1 s=0"));
    send("AK SYNC 2");
    CHECK(reply_starts("ERR 2 sync"));
    send("AK LINK 0");
    CHECK(strstr(s_reply, "e=10 i=20 c=30 p=40 q=50") != 0);
    send("AK LINK 1");
    CHECK(strstr(s_reply, "e=11 i=21 c=31 p=41 q=51") != 0);
    send("AK LINK");
    CHECK(reply_starts("ERR 2 link"));

    /* Erased slots use the factory profile virtually; a non-erased package
     * with a valid header but bad payload CRC is reported unavailable. */
    memcpy(s_slots[1], g_v3f_factory_profile_image,
           g_v3f_factory_profile_image_size);
    s_slots[1][g_v3f_factory_profile_image_size / 2U] ^= 0x5AU;
    send("AK INFO");
    CHECK(strstr(s_reply, "slots=101") != 0);
    memset(s_slots[1], 0xFF, sizeof(s_slots[1]));

    send("AK CLAUDE STATE RUNNING");
    CHECK(reply_starts("OK CLAUDE STATE RUNNING"));
    CHECK(s_claude_state == AIK_CLAUDE_STATE_RUNNING);

    /* Strict approval SHOW parser: fixed tag/risk, printable ASCII hex,
     * bounded tool/summary, No selected by default, and Claude state kept. */
    send("AK APPROVAL SHOW 1234abcd 2 42617368 "
         "52756e206d616b65202d42");
    CHECK(reply_starts("OK APPROVAL SHOW 1234abcd"));
    CHECK(s_approval_active == 1u);
    CHECK(s_approval_selected_yes == 0u);
    CHECK(s_approval_tag == 0x1234abcdu);
    CHECK(s_approval_risk == 2u);
    CHECK(s_approval_tool_len == 4u);
    CHECK(memcmp(s_approval_tool, "Bash", 4u) == 0);
    CHECK(s_approval_summary_len == 11u);
    CHECK(memcmp(s_approval_summary, "Run make -B", 11u) == 0);
    CHECK(s_claude_state == AIK_CLAUDE_STATE_RUNNING);

    send("AK CLAUDE STATE DONE");
    CHECK(reply_starts("OK CLAUDE STATE DONE"));
    CHECK(s_claude_state == AIK_CLAUDE_STATE_DONE);
    CHECK(s_approval_active == 1u);

    send("AK APPROVAL SHOW 00000001 0 - -");
    CHECK(reply_starts("OK APPROVAL SHOW 00000001"));
    CHECK(s_approval_active == 1u);
    CHECK(s_approval_selected_yes == 0u);
    CHECK(s_approval_tool_len == 0u);
    CHECK(s_approval_summary_len == 0u);
    CHECK(s_claude_state == AIK_CLAUDE_STATE_DONE);

    send("AK APPROVAL SHOW 1234abc 2 - -");
    CHECK(reply_starts("ERR 2 approval-show"));
    send("AK APPROVAL SHOW 01234abcd 2 - -");
    CHECK(reply_starts("ERR 2 approval-show"));
    send("AK APPROVAL SHOW 1234abcd 22 - -");
    CHECK(reply_starts("ERR 2 approval-show"));
    send("AK APPROVAL SHOW 1234abcd g - -");
    CHECK(reply_starts("ERR 2 approval-show"));
    send("AK APPROVAL SHOW 1234abcd 2 4 -");
    CHECK(reply_starts("ERR 2 approval-text"));
    send("AK APPROVAL SHOW 1234abcd 2 0a -");
    CHECK(reply_starts("ERR 2 approval-text"));
    send("AK APPROVAL SHOW 1234abcd 2 - - extra");
    CHECK(reply_starts("ERR 2 approval-show"));
    {
        char line[400];
        uint16_t index;
        int used = snprintf(line, sizeof(line),
                            "AK APPROVAL SHOW 1234abcd 2 ");

        for(index = 0u; index < (AIK_APPROVAL_TOOL_MAX + 1u); index++)
        {
            used += snprintf(&line[used],
                             sizeof(line) - (size_t)used,
                             "41");
        }
        (void)snprintf(&line[used], sizeof(line) - (size_t)used, " -");
        send(line);
        CHECK(reply_starts("ERR 4 approval-text"));
    }
    {
        char line[400];
        uint16_t index;
        int used = snprintf(line, sizeof(line),
                            "AK APPROVAL SHOW 1234abcd 2 - ");

        for(index = 0u; index < (AIK_APPROVAL_SUMMARY_MAX + 1u); index++)
        {
            used += snprintf(&line[used],
                             sizeof(line) - (size_t)used,
                             "41");
        }
        send(line);
        CHECK(reply_starts("ERR 4 approval-text"));
    }

    send("AK APPROVAL CLEAR 00000002");
    CHECK(reply_starts("ERR 3 approval-tag"));
    CHECK(s_approval_active == 1u);
    send("AK APPROVAL CLEAR 00000001");
    CHECK(reply_starts("OK APPROVAL CLEAR 00000001"));
    CHECK(s_approval_active == 0u);
    CHECK(s_claude_state == AIK_CLAUDE_STATE_DONE);
    send("AK APPROVAL CLEAR 00000001");
    CHECK(reply_starts("ERR 3 approval-inactive"));
    send("AK APPROVAL CLEAR 00000001 extra");
    CHECK(reply_starts("ERR 2 approval-clear"));

    send("AK APPROVAL SHOW 00000003 0 - -");
    CHECK(reply_starts("OK APPROVAL SHOW 00000003"));
    CHECK(s_approval_active == 1u);
    send("AK CLAUDE STATE OFF");
    CHECK(reply_starts("OK CLAUDE STATE OFF"));
    CHECK(s_claude_state == AIK_CLAUDE_STATE_OFF);
    CHECK(s_approval_active == 0u);

    send("AK CLAUDE STATE");
    CHECK(reply_starts("ERR 2 claude-state"));
    send("AK CLAUDE STATE RUNNING extra");
    CHECK(reply_starts("ERR 2 claude-state"));
    send("AK CLAUDE STATE running");
    CHECK(reply_starts("ERR 4 claude-state"));
    send("AK CLAUDE RUNNING");
    CHECK(reply_starts("ERR 1 claude"));
    send("AK CLAUDEX STATE RUNNING");
    CHECK(reply_starts("ERR 1 cmd"));
    CHECK(s_claude_state == AIK_CLAUDE_STATE_OFF);

    /* Bad requests. */
    send("AK BEGIN 0 100 0");
    CHECK(reply_starts("ERR 4"));
    send("AK DATA 0 aabb");
    CHECK(reply_starts("ERR 3"));
    send("AK NOPE");
    CHECK(reply_starts("ERR 1"));
    send("hello world");
    CHECK(s_reply[0] == '\0'); /* non-AK lines are ignored */

    /* Full upload into slot 1 and activation. */
    upload_package(g_v3f_factory_profile_image,
                   g_v3f_factory_profile_image_size, 1U, 64U);
    CHECK(reply_starts("OK COMMIT 1"));

    send("AK INFO");
    CHECK(strstr(s_reply, "slots=111") != 0);
    CHECK(strstr(s_reply, "stored=100") != 0);

    send("AK ACTIVATE 1");
    CHECK(reply_starts("OK ACTIVATE 1"));
    CHECK(v3f_profile_runtime_get()->active_slot == 1U);
    CHECK(s_active_slot == 1U);

    /* Delete is persisted. Deleting the active slot installs Default,
     * while the named user slot remains selectable as a Default alias. */
    send("AK DELETE 1");
    CHECK(reply_starts("OK DELETE 1"));
    CHECK(v3f_profile_runtime_get()->active_slot ==
          AIK_PROFILE_SLOT_FACTORY);
    CHECK(s_active_slot == AIK_PROFILE_SLOT_FACTORY);
    send("AK INFO");
    CHECK(strstr(s_reply, "slots=111") != 0);
    CHECK(strstr(s_reply, "stored=000") != 0);
    send("AK ACTIVATE 1");
    CHECK(reply_starts("OK ACTIVATE 1"));
    CHECK(v3f_profile_runtime_get()->active_slot == 1U);

    /* Restore the package for the existing activation/replacement tests. */
    upload_package(g_v3f_factory_profile_image,
                   g_v3f_factory_profile_image_size, 1U, 64U);
    CHECK(reply_starts("OK COMMIT 1"));
    send("AK ACTIVATE 1");
    CHECK(reply_starts("OK ACTIVATE 1"));

    send("AK DELETE 0");
    CHECK(reply_starts("ERR 2 delete"));
    send("AK DELETE 4");
    CHECK(reply_starts("ERR 2 delete"));
    send("AK DELETE 1 extra");
    CHECK(reply_starts("ERR 2 delete"));

    /* An unchanged package in the already-active slot is a true no-op:
     * no metadata write and no release-to-rearm event. */
    {
        uint32_t calls = s_set_active_calls;

        (void)v3f_profile_runtime_rearm_take();
        send("AK ACTIVATE 1");
        CHECK(reply_starts("OK ACTIVATE 1"));
        CHECK(s_set_active_calls == calls);
        CHECK(v3f_profile_runtime_rearm_take() == 0U);
    }

    /* Replacing the package in the current slot must still reload it;
     * slot equality alone is not sufficient for the no-op decision. */
    {
        uint32_t calls = s_set_active_calls;
        uint32_t next_revision =
            v3f_profile_runtime_get()->revision + 1U;

        set_package_revision(s_slots[0], next_revision);
        send("AK ACTIVATE 1");
        CHECK(reply_starts("OK ACTIVATE 1"));
        CHECK(s_set_active_calls == (calls + 1U));
        CHECK(v3f_profile_runtime_get()->revision == next_revision);
        CHECK(v3f_profile_runtime_rearm_take() == 1U);
    }

    /* An erased user slot starts from its built-in preset and can be
     * selected without first writing a package into flash. */
    send("AK ACTIVATE 2");
    CHECK(reply_starts("OK ACTIVATE 2"));
    CHECK(v3f_profile_runtime_get()->active_slot == 2U);
    CHECK(s_active_slot == 2U);
    CHECK(s_slots[1][0] == 0xFFU);
    CHECK(v3f_profile_runtime_rearm_take() == 1U);

    /* Non-erased corrupt content is still rejected before either
     * persistent or visible runtime state changes. */
    {
        v3f_profile_runtime_t before = *v3f_profile_runtime_get();
        uint8_t active_before = s_active_slot;

        s_slots[2][0] = 0U;
        send("AK ACTIVATE 3");
        CHECK(reply_starts("ERR 7"));
        CHECK(memcmp(v3f_profile_runtime_get(), &before,
                     sizeof(before)) == 0);
        CHECK(s_active_slot == active_before);
        CHECK(v3f_profile_runtime_rearm_take() == 0U);
        memset(s_slots[2], 0xFF, sizeof(s_slots[2]));
    }

    /* Metadata failure is transactional as well: the fully parsed
     * candidate must not become visible and must not arm rearm. */
    {
        v3f_profile_runtime_t before = *v3f_profile_runtime_get();
        uint8_t active_before = s_active_slot;

        s_fail_set_active = 1U;
        send("AK ACTIVATE 3");
        s_fail_set_active = 0U;
        CHECK(reply_starts("ERR 5"));
        CHECK(memcmp(v3f_profile_runtime_get(), &before,
                     sizeof(before)) == 0);
        CHECK(s_active_slot == active_before);
        CHECK(v3f_profile_runtime_rearm_take() == 0U);
    }

    /* Reboot: active slot restores from the (fake) store. */
    CHECK(v3f_profile_runtime_init() == 2U);

    /* Out-of-order offset is rejected. */
    send("AK BEGIN 2 80 0");
    CHECK(reply_starts("OK BEGIN"));
    send("AK DATA 8 00112233");
    CHECK(reply_starts("ERR 4"));
    send("AK ABORT");
    CHECK(reply_starts("OK ABORT"));

    /* CRC mismatch is rejected at commit time. */
    {
        char line[128];

        (void)snprintf(line, sizeof(line), "AK BEGIN 2 %lx deadbeef",
                       (unsigned long)g_v3f_factory_profile_image_size);
        send(line);
        CHECK(reply_starts("OK BEGIN"));
    }
    {
        uint32_t offset = 0U;
        char line[400];

        while(offset < g_v3f_factory_profile_image_size)
        {
            uint32_t take =
                ((g_v3f_factory_profile_image_size - offset) < 128U) ?
                (g_v3f_factory_profile_image_size - offset) : 128U;
            int pos = snprintf(line, sizeof(line), "AK DATA %lx ",
                               (unsigned long)offset);
            uint32_t i;

            for(i = 0U; i < take; i++)
            {
                pos += snprintf(&line[pos], sizeof(line) - (size_t)pos,
                                "%02x",
                                g_v3f_factory_profile_image[offset + i]);
            }
            send(line);
            offset += take;
        }
        send("AK COMMIT");
        CHECK(reply_starts("ERR 6"));
    }

    /* Corrupted package content fails AKPK validation at commit. */
    {
        static uint8_t bad[32768];
        uint32_t len = g_v3f_factory_profile_image_size;

        memcpy(bad, g_v3f_factory_profile_image, len);
        bad[len / 2U] ^= 0xA5U;
        upload_package(bad, len, 3U, 128U);
        CHECK(reply_starts("ERR 7"));
    }

    send("AK FACTORY");
    CHECK(reply_starts("OK ACTIVATE 0"));
    CHECK(v3f_profile_runtime_get()->active_slot ==
          AIK_PROFILE_SLOT_FACTORY);

    if(s_failures == 0)
    {
        printf("host_pc_link_test: all checks passed\n");
        return 0;
    }
    printf("host_pc_link_test: %d failures\n", s_failures);
    return 1;
}
