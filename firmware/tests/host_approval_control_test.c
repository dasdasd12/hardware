/*
 * Host test for approval controls and the V3F -> left CH585 context flags.
 *
 * Build and run from hardware/firmware:
 *   gcc -std=gnu99 -Wall -Wextra -I common -I h417/v3f/applications \
 *       tests/host_approval_control_test.c \
 *       h417/v3f/applications/rf_report_bridge.c \
 *       -o build/host_approval_control_test &&
 *       build/host_approval_control_test
 */

#include <stdio.h>
#include <string.h>

#include "aik_approval_control.h"
#include "rf_report_bridge.h"

static int s_failures;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

static uint8_t nkro_usage_set(
    const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
    uint8_t usage)
{
    uint8_t bit_index = (uint8_t)(usage - 0x04U);
    uint8_t byte_index = (uint8_t)(2U + (bit_index >> 3));

    return (uint8_t)((nkro16[byte_index] >>
                      (bit_index & 7U)) & 1U);
}

static void test_control_actions(void)
{
    aik_approval_control_state_t state;
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];

    aik_approval_control_reset(&state);
    (void)aik_approval_control_update_nav_valid(
        &state, 1U, 0U, 0U, 0U, 1U);
    (void)aik_approval_control_update_confirm_valid(
        &state, 1U, 0U, 0U, 0U, 1U);

    CHECK(aik_approval_control_update_nav_valid(
              &state, 1U, 1U, 1U, 0U, 1U) ==
          AIK_APPROVAL_CONTROL_SELECT_YES);
    CHECK(aik_approval_control_nav_consumed(&state) == 1U);
    (void)aik_approval_control_update_nav_valid(
        &state, 1U, 0U, 0U, 0U, 1U);

    CHECK(aik_approval_control_update_confirm_valid(
              &state, 1U, 0U, 1U, 1U, 1U) ==
          AIK_APPROVAL_CONTROL_CONFIRM_NO);
    memset(nkro, 0, sizeof(nkro));
    aik_approval_control_apply_confirm(
        AIK_APPROVAL_CONTROL_CONFIRM_NO, nkro);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_YES) == 0U);
    CHECK(nkro_usage_set(
              nkro, AIK_APPROVAL_CONTROL_HID_USAGE_NO) == 1U);
}

static void test_right_state_context_flags(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_half_state_v1_t right;

    memset(&right, 0, sizeof(right));
    right.half_seq = 7U;
    aik_spi_half_state_finish(
        &right, aik_spi_half_frame_bits(AIK_HALF_ID_RIGHT));

    v3f_rf_report_bridge_prepare_right_state_cmd(
        &cmd,
        9U,
        &right,
        AIK_OUTPUT_MODE_BLE,
        1U,
        0U,
        1U,
        73U,
        AIK_SPI_POWER_FLAG_BAT_VALID);
    CHECK(aik_spi_host_cmd_valid(&cmd) == 1U);
    CHECK(cmd.cmd == AIK_SPI_CMD_PUSH_RIGHT_STATE);
    CHECK((cmd.flags & AIK_SPI_FLAG_OUTPUT_MODE_MASK) ==
          AIK_OUTPUT_MODE_BLE);
    CHECK((cmd.flags & AIK_SPI_FLAG_APPROVAL_ACTIVE) != 0U);
    CHECK((cmd.flags & AIK_SPI_FLAG_APPROVAL_SELECTED_YES) == 0U);
    CHECK((cmd.flags & AIK_SPI_FLAG_RIGHT_STATE_VALID) != 0U);
    CHECK(memcmp(cmd.nkro16, &right, sizeof(right)) == 0);
    CHECK(aik_spi_host_cmd_battery_percent(&cmd) == 73U);
    CHECK((aik_spi_host_cmd_power_flags(&cmd) &
           AIK_SPI_POWER_FLAG_BAT_VALID) != 0U);

    v3f_rf_report_bridge_prepare_right_state_cmd(
        &cmd,
        10U,
        &right,
        AIK_OUTPUT_MODE_RF24,
        1U,
        1U,
        0U,
        AIK_BATTERY_PERCENT_UNKNOWN,
        0U);
    CHECK(aik_spi_host_cmd_valid(&cmd) == 1U);
    CHECK((cmd.flags & AIK_SPI_FLAG_APPROVAL_SELECTED_YES) != 0U);
    CHECK((cmd.flags & AIK_SPI_FLAG_RIGHT_STATE_VALID) == 0U);
}

static void test_battery_piggyback(void)
{
    uint16_t half_seq;

    half_seq = aik_spi_half_seq_pack_battery(0x5AU, 68U, 1U);
    CHECK((uint8_t)half_seq == 0x5AU);
    CHECK(aik_spi_half_seq_battery_valid(half_seq) == 1U);
    CHECK(aik_spi_half_seq_battery_percent(half_seq) == 68U);

    half_seq = aik_spi_half_seq_pack_battery(0xA5U, 101U, 1U);
    CHECK((uint8_t)half_seq == 0xA5U);
    CHECK(aik_spi_half_seq_battery_valid(half_seq) == 0U);
}

int main(void)
{
    test_control_actions();
    test_right_state_context_flags();
    test_battery_piggyback();
    if(s_failures == 0)
    {
        printf("host_approval_control_test: all checks passed\n");
        return 0;
    }
    printf("host_approval_control_test: %d failures\n", s_failures);
    return 1;
}
