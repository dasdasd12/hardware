/*
 * Host test for the V3F PC configuration protocol (pc_link command
 * core). The profile store is emulated in RAM, so the full upload
 * flow (BEGIN/DATA/COMMIT/ACTIVATE) runs against the real AKPK
 * validation and runtime install paths using the generated factory
 * package as payload.
 *
 * Build & run (from hardware/firmware):
 *   gcc -std=gnu99 -Wall -Wextra \
 *       -I common -I h417/v3f/applications \
 *       tests/host_pc_link_test.c \
 *       h417/v3f/applications/pc_link.c \
 *       h417/v3f/applications/profile_runtime.c \
 *       h417/v3f/applications/profile_sync.c \
 *       h417/v3f/applications/factory_profile_image.c \
 *       h417/v3f/applications/half_state.c \
 *       -o build/host_pc_link_test && build/host_pc_link_test
 */

#include <stdio.h>
#include <string.h>

#include "aik_profile_format.h"
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

const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id)
{
    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return 0;
    }
    return s_slots[slot_id - AIK_PROFILE_USER_SLOT_FIRST];
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

uint8_t v3f_profile_store_get_active_slot(void)
{
    return s_active_slot;
}

int v3f_profile_store_set_active_slot(uint8_t slot_id)
{
    if(slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL)
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
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
    v3f_pc_link_set_writer(reply_capture);

    CHECK(v3f_profile_runtime_init() == AIK_PROFILE_SLOT_FACTORY);

    send("AK PING");
    CHECK(reply_starts("OK PONG"));

    send("AK INFO");
    CHECK(reply_starts("OK INFO active=0"));
    CHECK(strstr(s_reply, "slots=000") != 0);

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
    CHECK(strstr(s_reply, "slots=100") != 0);

    send("AK ACTIVATE 1");
    CHECK(reply_starts("OK ACTIVATE 1"));
    CHECK(v3f_profile_runtime_get()->active_slot == 1U);
    CHECK(s_active_slot == 1U);

    /* Reboot: active slot restores from the (fake) store. */
    CHECK(v3f_profile_runtime_init() == 1U);

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
