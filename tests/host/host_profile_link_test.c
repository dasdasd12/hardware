/*
 * Cross-MCU host test: wires the H417 V3F profile sync engine directly
 * to the CH585 profile receive handler (the fake "SPI" is a function
 * call), then exercises the full BEGIN/CHUNK/COMMIT protocol including
 * command loss, lost acks, Data-Flash persistence across a simulated
 * CH585 reboot, and status-poll reconciliation.
 *
 * Build and run both halves from hardware:
 *   make -C tests/host run
 */

#include <stdio.h>
#include <string.h>

#include "aik_approval_control.h"
#include "aik_host_shortcut.h"
#include "aik_profile_format.h"
#include "aik_profile_shortcut.h"
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

static uint8_t nkro_usage_set(
    const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
    uint8_t usage)
{
    uint8_t bit_index = (uint8_t)(usage - 0x04U);
    uint8_t byte_index = (uint8_t)(2U + (bit_index >> 3));

    return (uint8_t)((nkro16[byte_index] >>
                      (bit_index & 7U)) & 1U);
}

static void set_half_global_key(aik_spi_half_state_v1_t *left,
                                aik_spi_half_state_v1_t *right,
                                uint8_t key_id)
{
    if(key_id < AIK_KEY_COUNT_RIGHT)
    {
        aik_spi_half_set_bit(right, key_id);
    }
    else
    {
        aik_spi_half_set_bit(
            left, (uint8_t)(key_id - AIK_KEY_COUNT_RIGHT));
    }
}

static void test_host_shortcut_state_machine(void)
{
    aik_host_shortcut_state_t state;
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];

    aik_host_shortcut_reset(&state);
    CHECK(aik_host_shortcut_update(&state, 0U, 1U) ==
          AIK_HOST_SHORTCUT_NONE);

    /* An unknown center state releases F24 without re-arming the same hold. */
    CHECK(aik_host_shortcut_update(&state, 1U, 1U) ==
          AIK_HOST_SHORTCUT_CLAUDE_CODE);
    CHECK(aik_host_shortcut_update_valid(&state, 0U, 0U, 0U) ==
          AIK_HOST_SHORTCUT_NONE);
    CHECK(aik_host_shortcut_center_consumed(&state) == 1U);
    CHECK(aik_host_shortcut_update(&state, 0U, 1U) ==
          AIK_HOST_SHORTCUT_NONE);
    CHECK(aik_host_shortcut_update(&state, 0U, 0U) ==
          AIK_HOST_SHORTCUT_NONE);
    CHECK(aik_host_shortcut_center_consumed(&state) == 0U);
    CHECK(aik_host_shortcut_update(&state, 1U, 1U) ==
          AIK_HOST_SHORTCUT_CLAUDE_CODE);

    memset(nkro, 0, sizeof(nkro));
    aik_host_shortcut_apply(AIK_HOST_SHORTCUT_CLAUDE_CODE, nkro);
    CHECK(nkro_usage_set(nkro, AIK_HOST_SHORTCUT_HID_USAGE_F24) == 1U);

    /* Releasing Fn first must keep center consumed until center releases. */
    CHECK(aik_host_shortcut_update(&state, 0U, 1U) ==
          AIK_HOST_SHORTCUT_CLAUDE_CODE);
    CHECK(aik_host_shortcut_update(&state, 0U, 0U) ==
          AIK_HOST_SHORTCUT_NONE);
    CHECK(aik_host_shortcut_update(&state, 0U, 1U) ==
          AIK_HOST_SHORTCUT_NONE);
}

static void test_approval_control_state_machine(void)
{
    aik_approval_control_state_t state;
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];

    aik_approval_control_reset(&state);

    /* A valid neutral frame is required before either gesture can arm. */
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 1U, 0U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 1U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);

    /* An ordinary Up gesture cannot upgrade when Fn arrives later. */
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 0U, 1U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 1U, 1U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_nav_consumed(&state) == 0U);
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 0U, 0U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);

    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 1U, 1U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_SELECT_YES);
    CHECK(aik_approval_control_nav_consumed(&state) == 1U);

    /* Releasing Fn first keeps both directions consumed. */
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 0U, 1U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_nav_consumed(&state) == 1U);
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 0U, 0U, 0U, 0U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_nav_consumed(&state) == 1U);
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 0U, 1U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_nav_consumed(&state) == 1U);
    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 0U, 0U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_nav_consumed(&state) == 0U);

    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 0U, 1U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 1U, 1U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_confirm_consumed(&state) == 0U);
    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 0U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);

    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 1U, 1U, 1U) ==
          AIK_APPROVAL_CONTROL_CONFIRM_YES);
    CHECK(aik_approval_control_confirm_consumed(&state) == 1U);
    memset(nkro, 0, sizeof(nkro));
    aik_approval_control_apply_confirm(
        AIK_APPROVAL_CONTROL_CONFIRM_YES, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 1U);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_NO) == 0U);

    /* Unknown right state releases F22 and never re-arms the same pulse. */
    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 0U, 0U, 0U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_confirm_consumed(&state) == 1U);
    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 1U, 1U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_confirm_consumed(&state) == 1U);
    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 1U, 0U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_NONE);
    CHECK(aik_approval_control_confirm_consumed(&state) == 0U);

    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 0U, 1U, 1U, 1U) ==
          AIK_APPROVAL_CONTROL_CONFIRM_NO);
    memset(nkro, 0, sizeof(nkro));
    aik_approval_control_apply_confirm(
        AIK_APPROVAL_CONTROL_CONFIRM_NO, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_NO) == 1U);
}

static aik_hp_local_entry_t *find_runtime_local(
    v3f_profile_runtime_t *runtime,
    uint8_t signal_id)
{
    uint8_t i;

    for(i = 0U; i < runtime->local_count; i++)
    {
        if(runtime->locals[i].signal_id == signal_id)
        {
            return &runtime->locals[i];
        }
    }
    return 0;
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

static void test_invalid_chunk_rejected(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_profile_begin_v1_t begin;
    aik_spi_profile_chunk_v1_t chunk;
    aik_spi_profile_xfer_v1_t xfer;

    memset(&cmd, 0, sizeof(cmd));
    memset(&begin, 0, sizeof(begin));
    begin.slot_id = AIK_PROFILE_SLOT_FACTORY;
    begin.patch_flags = AIK_SPI_PROFILE_FLAG_ACTIVATE;
    begin.total_len = sizeof(aik_hp_header_t);
    cmd.cmd = AIK_SPI_CMD_PROFILE_BEGIN;
    cmd.host_seq = 1U;
    aik_spi_host_cmd_set_payload(&cmd, &begin, sizeof(begin));
    aik_spi_host_cmd_finish(&cmd);
    ch585_profile_handle_transfer_cmd(&cmd, &s_engine);
    ch585_profile_fill_xfer(&xfer);
    CHECK(xfer.state == AIK_SPI_XFER_STATE_RECEIVING);

    memset(&cmd, 0, sizeof(cmd));
    memset(&chunk, 0, sizeof(chunk));
    cmd.cmd = AIK_SPI_CMD_PROFILE_CHUNK;
    cmd.host_seq = 2U;
    aik_spi_host_cmd_set_payload(&cmd, &chunk, sizeof(chunk));
    aik_spi_host_cmd_finish(&cmd);
    ch585_profile_handle_transfer_cmd(&cmd, &s_engine);
    ch585_profile_fill_xfer(&xfer);
    CHECK(xfer.state == AIK_SPI_XFER_ERR_RANGE);
}

#if CH585_HALF_ID == 0
static void test_wireless_claude_shortcut(void)
{
    aik_spi_half_state_v1_t left;
    aik_spi_half_state_v1_t right;
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];

    ch585_half_report_reset_factory();
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, 38U);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER_QUALIFIED);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER);

    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, AIK_HOST_SHORTCUT_HID_USAGE_F24) == 1U);
    CHECK(nkro_usage_set(nkro, 0x28U) == 0U);

    memset(&right, 0, sizeof(right));
    memset(&left, 0, sizeof(left));
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER_QUALIFIED);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, AIK_HOST_SHORTCUT_HID_USAGE_F24) == 1U);
    CHECK(nkro_usage_set(nkro, 0x28U) == 0U);

    memset(&left, 0, sizeof(left));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, AIK_HOST_SHORTCUT_HID_USAGE_F24) == 0U);

    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER_QUALIFIED);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, AIK_HOST_SHORTCUT_HID_USAGE_F24) == 0U);
    CHECK(nkro_usage_set(nkro, 0x28U) == 1U);

    /* Fn+direction never has center qualification and must not launch. */
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, 38U);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_UP);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, AIK_HOST_SHORTCUT_HID_USAGE_F24) == 0U);
}

static void test_wireless_approval_controls(void)
{
    aik_spi_half_state_v1_t left;
    aik_spi_half_state_v1_t right;
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ch585_half_report_set_approval_context(1U, 1U, 1U);
    ch585_half_report_build_nkro16(&left, &right, nkro);

    /* Up selects Yes in V3F but emits no host key and no arrow. */
    aik_spi_half_set_bit(&right, 38U);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_UP);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x52U) == 0U);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 0U);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_NO) == 0U);

    /* Fn release before direction release cannot leak Up. */
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x52U) == 0U);
    memset(&left, 0, sizeof(left));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_UP);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x52U) == 1U);

    /* Prime a new confirm gesture, then confirm the selected Yes. */
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    aik_spi_half_set_bit(&right, 38U);
    aik_spi_half_set_bit(&right, AIK_RIGHT_LOCAL_BIT_EC11_MUTE);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 1U);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_NO) == 0U);
    CHECK(ch585_half_report_consumer_usage(&left, &right) ==
          AIK_CONSUMER_USAGE_NONE);

    /* A profile that maps EC11 press to a keyboard key is suppressed too. */
    ch585_half_report_set_local(
        AIK_HP_SIGNAL_EC11_PRESS, AIK_HP_TARGET_KEYBOARD, 0x04U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x04U) == 0U);
    ch585_half_report_set_local(
        AIK_HP_SIGNAL_EC11_PRESS,
        AIK_HP_TARGET_CONSUMER,
        AIK_CONSUMER_USAGE_MUTE);

    /* Fn release first keeps Mute consumed and F22 held for the pulse. */
    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, AIK_RIGHT_LOCAL_BIT_EC11_MUTE);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 1U);
    CHECK(ch585_half_report_consumer_usage(&left, &right) ==
          AIK_CONSUMER_USAGE_NONE);

    /* Link loss releases F22 but the same pulse cannot re-arm. */
    ch585_half_report_set_approval_context(1U, 1U, 0U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 0U);
    ch585_half_report_set_approval_context(1U, 1U, 1U);
    aik_spi_half_set_bit(&right, 38U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 0U);
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);

    /* No selection context means the next confirmation emits F23. */
    ch585_half_report_set_approval_context(1U, 0U, 1U);
    aik_spi_half_set_bit(&right, 38U);
    aik_spi_half_set_bit(&right, AIK_RIGHT_LOCAL_BIT_EC11_MUTE);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 0U);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_NO) == 1U);
    CHECK(ch585_half_report_consumer_usage(&left, &right) ==
          AIK_CONSUMER_USAGE_NONE);

    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);

    /* Starting as an ordinary press stays Mute after Fn arrives. */
    aik_spi_half_set_bit(&right, AIK_RIGHT_LOCAL_BIT_EC11_MUTE);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(ch585_half_report_consumer_usage(&left, &right) ==
          AIK_CONSUMER_USAGE_MUTE);
    aik_spi_half_set_bit(&right, 38U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_NO) == 0U);
    CHECK(ch585_half_report_consumer_usage(&left, &right) ==
          AIK_CONSUMER_USAGE_MUTE);

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ch585_half_report_set_approval_context(0U, 0U, 1U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
}

static void test_wireless_profile_shortcut_consumption(void)
{
    aik_spi_half_state_v1_t left;
    aik_spi_half_state_v1_t right;
    uint8_t base_pairs[AIK_KEY_COUNT_TOTAL * 2U];
    uint8_t fn_pairs[AIK_KEY_COUNT_TOTAL * 2U];
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);

    memset(base_pairs, 0, sizeof(base_pairs));
    base_pairs[38U * 2U] = 0x04U; /* Fn physical key -> A */
    base_pairs[52U * 2U] = 0x05U; /* profile 1 key -> B */
    base_pairs[51U * 2U] = 0x06U; /* profile 2 key -> C */
    ch585_half_report_set_key_outputs(base_pairs);
    ch585_half_report_clear_fn_overlay();

    /* A reserved Fn+slot chord wins over configurable base outputs. */
    set_half_global_key(&left, &right, 38U);
    set_half_global_key(&left, &right, 52U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x04U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x05U) == 0U);

    /* A short loss of the right-half state must not release the chord or
     * expose a still-held slot key from the left half. */
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, 0, nkro);
    CHECK(nkro_usage_set(nkro, 0x05U) == 0U);

    /* Fn has now been released. A profile table swap while the slot key
     * remains held must not reset the shortcut state or expose the new
     * mapping. */
    base_pairs[52U * 2U] = 0x07U; /* D */
    ch585_half_report_set_key_outputs(base_pairs);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x07U) == 0U);

    memset(&left, 0, sizeof(left));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    set_half_global_key(&left, &right, 52U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x07U) == 1U);

    /* The reserved chord also wins over an active configurable Fn overlay. */
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    memset(fn_pairs, 0, sizeof(fn_pairs));
    fn_pairs[52U * 2U] = 0x08U; /* E */
    ch585_half_report_set_fn_overlay(38U, fn_pairs);
    set_half_global_key(&left, &right, 38U);
    set_half_global_key(&left, &right, 52U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x04U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x07U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x08U) == 0U);

    /* Ambiguous multi-slot chords switch nowhere and leak no digit. */
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    set_half_global_key(&left, &right, 38U);
    set_half_global_key(&left, &right, 52U);
    set_half_global_key(&left, &right, 51U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x07U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x06U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x08U) == 0U);

    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x07U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x06U) == 0U);

    memset(&left, 0, sizeof(left));
    ch585_half_report_build_nkro16(&left, &right, nkro);

    /* A newly recognized profile chord has priority over the F24 chord.
     * The center itself is not part of the profile chord and remains Enter. */
    set_half_global_key(&left, &right, 38U);
    set_half_global_key(&left, &right, 52U);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER_QUALIFIED);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, AIK_HOST_SHORTCUT_HID_USAGE_F24) == 0U);
    CHECK(nkro_usage_set(nkro, 0x28U) == 1U);
    CHECK(nkro_usage_set(nkro, 0x07U) == 0U);

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ch585_half_report_build_nkro16(&left, &right, nkro);
    ch585_half_report_reset_factory();
}

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

static void test_wireless_profile_mapping(void)
{
    v3f_profile_runtime_t *runtime =
        (v3f_profile_runtime_t *)(void *)v3f_profile_runtime_get();
    v3f_profile_runtime_t saved = *runtime;
    aik_spi_half_state_v1_t left;
    aik_spi_half_state_v1_t right;
    aik_hp_local_entry_t *consumer;
    aik_hp_local_entry_t *wheel;
    uint8_t patch[AIK_HP_MAX_SIZE];
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];
    uint16_t patch_len;
    const aik_hp_header_t *hdr;

    /* A base-only remap of physical key 38 must disable the legacy
     * hardcoded Fn swallowing and report the configured key normally. */
    runtime->has_fn_overlay = 0U;
    runtime->fn_hold_key = 0xFFU;
    memset(runtime->fn_keys, 0, sizeof(runtime->fn_keys));
    runtime->base_keys[38U].usage = 0x04U; /* A */
    runtime->base_keys[38U].modifier_mask = 0U;
    runtime->base_keys[5U].usage = 0x05U;  /* B */
    runtime->base_keys[5U].modifier_mask = 0U;
    runtime->generation16++;
    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();
    CHECK(drive_sync(4096U) == 1U);

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, 38U);
    aik_spi_half_set_bit(&right, 5U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x04U) == 1U);
    CHECK(nkro_usage_set(nkro, 0x05U) == 1U);

    /* The existing single hold-overlay is carried in AKHR and interpreted
     * by the wireless composer with the same fallback rules as H417. */
    runtime->has_fn_overlay = 1U;
    runtime->fn_hold_key = 38U;
    memset(runtime->fn_keys, 0, sizeof(runtime->fn_keys));
    runtime->fn_keys[5U].usage = 0x06U; /* C */
    runtime->generation16++;

    consumer = find_runtime_local(runtime, AIK_HP_SIGNAL_EC11_CW);
    wheel = find_runtime_local(runtime, AIK_HP_SIGNAL_WHEEL_UP);
    CHECK(consumer != 0);
    CHECK(wheel != 0);
    if(consumer != 0)
    {
        consumer->target_kind = AIK_HP_TARGET_CONSUMER;
        consumer->value = AIK_CONSUMER_USAGE_MUTE;
    }
    if(wheel != 0)
    {
        wheel->target_kind = AIK_HP_TARGET_MOUSE_WHEEL;
        wheel->value = 0x00FFU; /* reverse the physical up step */
    }

    patch_len = v3f_profile_runtime_build_half_patch(
        AIK_HALF_ID_LEFT, patch, sizeof(patch));
    CHECK(patch_len != 0U);
    hdr = (const aik_hp_header_t *)patch;
    CHECK((hdr->flags & AIK_HP_FLAG_HAS_FN_DISPATCH77) != 0U);
    CHECK(hdr->fn_hold_key == 38U);
    CHECK(memcmp(patch + hdr->fn_dispatch_offset, runtime->fn_keys,
                 sizeof(runtime->fn_keys)) == 0);

    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();
    CHECK(drive_sync(4096U) == 1U);

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, 38U);
    aik_spi_half_set_bit(&right, 5U);
    ch585_half_report_build_nkro16(&left, &right, nkro);
    CHECK(nkro_usage_set(nkro, 0x04U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x05U) == 0U);
    CHECK(nkro_usage_set(nkro, 0x06U) == 1U);

    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, AIK_RIGHT_LOCAL_BIT_EC11_CW);
    CHECK(v3f_profile_runtime_consumer_usage(&right) ==
          AIK_CONSUMER_USAGE_MUTE);
    CHECK(ch585_half_report_consumer_usage(&left, &right) ==
          AIK_CONSUMER_USAGE_MUTE);
    CHECK(ch585_half_report_mouse_wheel_step(
              AIK_HP_SIGNAL_WHEEL_UP) == -1);

    *runtime = saved;
    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();
    CHECK(drive_sync(4096U) == 1U);
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

static void test_factory_slot_persistence(void)
{
    const ch585_profile_state_t *state;

    CHECK(v3f_profile_runtime_install_package(g_v3f_factory_profile_image,
                                              g_v3f_factory_profile_image_size,
                                              AIK_PROFILE_SLOT_FACTORY) ==
          V3F_PROFILE_OK);
    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();
    CHECK(drive_sync(4096U) == 1U);
    CHECK(ch585_profile_state()->active_slot == AIK_PROFILE_SLOT_FACTORY);

    /* Factory has no Data-Flash slot, but its active-slot meta must still
     * survive a CH585-only reboot after switching away from a user slot. */
    reset_ch585_side();
    state = ch585_profile_state();
    CHECK(state->active_slot == AIK_PROFILE_SLOT_FACTORY);
    CHECK(state->patch_applied == 0U);
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

    test_invalid_chunk_rejected();
    test_basic_sync();
#if CH585_HALF_ID == 0
    test_host_shortcut_state_machine();
    test_approval_control_state_machine();
    test_wireless_claude_shortcut();
    test_wireless_approval_controls();
    test_wireless_profile_shortcut_consumption();
    test_wireless_composition_matches_v3f();
    test_wireless_profile_mapping();
#endif
    test_lossy_link();
    test_user_slot_persistence();
    test_status_reconciliation();
    test_factory_slot_persistence();

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
