/*
 * Cross-MCU host test: wires the H417 V3F profile sync engine directly
 * to the CH585 profile receive handler (the fake "SPI" is a function
 * call), then exercises the full BEGIN/CHUNK/COMMIT protocol including
 * command loss, lost acks, Data-Flash persistence across a simulated
 * CH585 reboot, and status-poll reconciliation.
 *
 * Build & run (from hardware/firmware), once per half:
 *   gcc -std=gnu99 -Wall -Wextra -DCH585_HALF_ID=0 \
 *       -I tests/host_stubs -I common \
 *       -I ch585/applications -I ch585/applications/half_scan \
 *       -I h417/v3f/applications \
 *       tests/host_profile_link_test.c \
 *       ch585/applications/half_scan/ch585_profile.c \
 *       ch585/applications/half_scan/ch585_half_report.c \
 *       ch585/applications/magnetic_key_engine.c \
 *       h417/v3f/applications/profile_runtime.c \
 *       h417/v3f/applications/profile_sync.c \
 *       h417/v3f/applications/factory_profile_image.c \
 *       h417/v3f/applications/half_state.c \
 *       -o build/host_profile_link_test && build/host_profile_link_test
 */

#include <stdio.h>
#include <string.h>

#include "aik_profile_format.h"
#include "ISP585.h"
#include "ch585_profile.h"
#include "ch585_half_report.h"
#include "magnetic_key_engine.h"
#include "profile_runtime.h"
#include "profile_store.h"
#include "profile_sync.h"
#include "ch585_link.h"
#include "factory_profile_image.h"

#ifndef CH585_HALF_ID
#define CH585_HALF_ID 0
#endif

static int s_failures;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

/* ------------------------------------------------------------------ */
/* Fake CH585 Data-Flash                                              */
/* ------------------------------------------------------------------ */

static uint8_t s_fake_eeprom[EEPROM_MAX_SIZE];

uint32_t host_fake_eeprom_read(uint32_t addr, void *buf, uint32_t len)
{
    if((addr + len) > sizeof(s_fake_eeprom))
    {
        return 1U;
    }
    memcpy(buf, &s_fake_eeprom[addr], len);
    return 0U;
}

uint32_t host_fake_eeprom_write(uint32_t addr, const void *buf, uint32_t len)
{
    if((addr + len) > sizeof(s_fake_eeprom))
    {
        return 1U;
    }
    memcpy(&s_fake_eeprom[addr], buf, len);
    return 0U;
}

uint32_t host_fake_eeprom_erase(uint32_t addr, uint32_t len)
{
    if((addr + len) > sizeof(s_fake_eeprom))
    {
        return 1U;
    }
    memset(&s_fake_eeprom[addr], 0xFF, len);
    return 0U;
}

/* ------------------------------------------------------------------ */
/* Fake H417 profile store                                            */
/* ------------------------------------------------------------------ */

const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id)
{
    (void)slot_id;
    return 0;
}
const uint8_t *v3f_profile_store_staging_ptr(void) { return 0; }
int v3f_profile_store_staging_begin(uint32_t l) { (void)l; return -1; }
int v3f_profile_store_staging_write(uint32_t o, const uint8_t *d, uint32_t l)
{ (void)o; (void)d; (void)l; return -1; }
int v3f_profile_store_staging_finish(void) { return -1; }
void v3f_profile_store_staging_abort(void) {}
int v3f_profile_store_commit_staging_to_slot(uint8_t s, uint32_t l)
{ (void)s; (void)l; return -1; }
uint8_t v3f_profile_store_get_active_slot(void) { return 0U; }
int v3f_profile_store_set_active_slot(uint8_t s) { (void)s; return -1; }

/* ------------------------------------------------------------------ */
/* Fake SPI link: routes commands straight into the CH585 handler.    */
/* ------------------------------------------------------------------ */

static mag_key_engine_t s_engine;
static uint32_t s_link_cmd_count;
static uint8_t s_drop_every;      /* drop cmd before it reaches the 585 */
static uint8_t s_drop_ack_every;  /* process cmd but lose the ack */

uint8_t v3f_ch585_link_profile_cmd(uint8_t half_id,
                                   const aik_spi_host_cmd_v1_t *cmd,
                                   aik_spi_profile_xfer_v1_t *out)
{
    if(half_id != (uint8_t)CH585_HALF_ID)
    {
        return 0U;
    }

    s_link_cmd_count++;
    if((s_drop_every != 0U) &&
       ((s_link_cmd_count % s_drop_every) == 0U))
    {
        return 0U;
    }

    ch585_profile_handle_transfer_cmd(cmd, &s_engine);
    ch585_profile_fill_xfer(out);

    if((s_drop_ack_every != 0U) &&
       ((s_link_cmd_count % s_drop_ack_every) == 1U))
    {
        return 0U;
    }
    return 1U;
}

uint8_t v3f_ch585_link_query_profile_status(uint8_t half_id,
                                            uint16_t host_seq,
                                            aik_spi_profile_status_v1_t *out)
{
    if(half_id != (uint8_t)CH585_HALF_ID)
    {
        return 0U;
    }
    memset(out, 0, sizeof(*out));
    out->ack_seq = host_seq;
    out->half_id = half_id;
    ch585_profile_fill_status(out);
    aik_spi_profile_status_finish(out);
    return 1U;
}

/* ------------------------------------------------------------------ */

static uint8_t drive_sync(uint32_t max_ticks)
{
    uint32_t tick;

    for(tick = 0U; tick < max_ticks; tick++)
    {
        (void)v3f_profile_sync_poll((uint16_t)(tick * 2U));
        if(v3f_profile_sync_half_synced((uint8_t)CH585_HALF_ID) != 0U)
        {
            return 1U;
        }
    }
    return 0U;
}

static void reset_ch585_side(void)
{
    mag_key_config_t cfg;

    memset(&s_engine, 0, sizeof(s_engine));
    mag_key_default_config(&cfg);
    cfg.mode = MAG_KEY_MODE_RAPID_TRIGGER;
    (void)mag_key_engine_init(&s_engine,
                              aik_spi_half_key_count((uint8_t)CH585_HALF_ID),
                              &cfg);
    ch585_half_report_reset_factory();
    ch585_profile_boot_load(&s_engine);
}

#if CH585_HALF_ID == 0
static void set_key(v3f_global_key_state_t *keys, uint8_t key_id)
{
    keys->down[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
}
#endif

static void test_basic_sync(void)
{
    const ch585_profile_state_t *state;
    uint8_t i;

    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();
    CHECK(drive_sync(4096U) == 1U);

    state = ch585_profile_state();
    CHECK(state->profile_id16 == v3f_profile_runtime_get()->profile_id16);
    CHECK(state->generation16 == v3f_profile_runtime_get()->generation16);
    CHECK(state->active_slot == AIK_PROFILE_SLOT_FACTORY);

    for(i = 0U; i < s_engine.key_count; i++)
    {
        CHECK(s_engine.cfg[i].press_pm == 400U);
        CHECK(s_engine.cfg[i].release_pm == 350U);
        CHECK(s_engine.cfg[i].rt_press_delta_pm == 100U);
        CHECK(s_engine.cfg[i].mode == MAG_KEY_MODE_RAPID_TRIGGER);
        if(s_failures != 0)
        {
            break;
        }
    }
}

#if CH585_HALF_ID == 0
static void test_wireless_composition_matches_v3f(void)
{
    aik_spi_half_state_v1_t left;
    aik_spi_half_state_v1_t right;
    v3f_global_key_state_t keys;
    uint8_t nkro_585[AIK_NKRO_REPORT_BYTES];
    uint8_t nkro_v3f[AIK_NKRO_REPORT_BYTES];

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    /* Right half local key 5 (global 5) + left half local key 3
     * (global 44) + left fiveway up bit. */
    aik_spi_half_set_bit(&right, 5U);
    aik_spi_half_set_bit(&left, 3U);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_UP);

    ch585_half_report_build_nkro16(&left, &right, nkro_585);

    memset(&keys, 0, sizeof(keys));
    set_key(&keys, 5U);
    set_key(&keys, (uint8_t)(AIK_KEY_COUNT_RIGHT + 3U));
    v3f_profile_runtime_build_nkro16(&keys, nkro_v3f);
    v3f_profile_runtime_apply_local_keyboard(&left, nkro_v3f);

    CHECK(memcmp(nkro_585, nkro_v3f, sizeof(nkro_585)) == 0);
}
#endif

static void test_lossy_link(void)
{
    reset_ch585_side();
    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();

    s_drop_every = 3U;
    CHECK(drive_sync(60000U) == 1U);
    s_drop_every = 0U;

    reset_ch585_side();
    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();

    s_drop_ack_every = 4U;
    CHECK(drive_sync(60000U) == 1U);
    s_drop_ack_every = 0U;

    CHECK(ch585_profile_state()->profile_id16 ==
          v3f_profile_runtime_get()->profile_id16);
}

static void test_user_slot_persistence(void)
{
    const ch585_profile_state_t *state;

    /* Install the same package as user slot 2 on the V3F side; the
     * push must persist the patch into the fake Data-Flash. */
    CHECK(v3f_profile_runtime_install_package(g_v3f_factory_profile_image,
                                              g_v3f_factory_profile_image_size,
                                              2U) == V3F_PROFILE_OK);
    reset_ch585_side();
    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();
    CHECK(drive_sync(4096U) == 1U);

    state = ch585_profile_state();
    CHECK(state->active_slot == 2U);

    /* Simulated CH585 reboot: state must come back from Data-Flash
     * without any sync traffic. */
    reset_ch585_side();
    state = ch585_profile_state();
    CHECK(state->active_slot == 2U);
    CHECK(state->patch_applied == 1U);
    CHECK(s_engine.cfg[0].press_pm == 400U); /* patch, not 450 default */
}

static void test_status_reconciliation(void)
{
    uint32_t tick;

    CHECK(v3f_profile_sync_half_synced((uint8_t)CH585_HALF_ID) == 1U);

    /* Corrupt the CH585 side back to factory: the periodic status poll
     * must notice the mismatch and re-push. */
    reset_ch585_side();
    memset(s_fake_eeprom, 0xFF, sizeof(s_fake_eeprom));
    reset_ch585_side();
    CHECK(ch585_profile_state()->active_slot == AIK_PROFILE_SLOT_FACTORY);

    v3f_profile_sync_status_poll(0U);
    v3f_profile_sync_status_poll(1U);
    CHECK(drive_sync(4096U) == 1U);
    CHECK(ch585_profile_state()->active_slot == 2U);

    for(tick = 0U; tick < 8U; tick++)
    {
        v3f_profile_sync_status_poll((uint16_t)tick);
    }
    CHECK(v3f_profile_sync_half_synced((uint8_t)CH585_HALF_ID) == 1U);
}

int main(void)
{
    memset(s_fake_eeprom, 0xFF, sizeof(s_fake_eeprom));
    reset_ch585_side();

    CHECK(ch585_profile_state()->active_slot == AIK_PROFILE_SLOT_FACTORY);
    CHECK(ch585_profile_state()->patch_applied == 0U);

    CHECK(v3f_profile_runtime_init() == AIK_PROFILE_SLOT_FACTORY);
    CHECK(v3f_profile_runtime_valid() == 1U);

    /* Factory identities must agree across MCUs without any sync. */
    CHECK(ch585_profile_state()->profile_id16 ==
          v3f_profile_runtime_get()->profile_id16);

    test_basic_sync();
#if CH585_HALF_ID == 0
    test_wireless_composition_matches_v3f();
#endif
    test_lossy_link();
    test_user_slot_persistence();
    test_status_reconciliation();

    if(s_failures == 0)
    {
        printf("host_profile_link_test (half %u): all checks passed\n",
               (unsigned int)CH585_HALF_ID);
        return 0;
    }
    printf("host_profile_link_test (half %u): %d failures\n",
           (unsigned int)CH585_HALF_ID, s_failures);
    return 1;
}
