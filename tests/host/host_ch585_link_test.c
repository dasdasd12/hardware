/*
 * Host behavior test for the H417 <-> CH585 transaction layer.
 *
 * Build and run from hardware:
 *   make -C tests/host run
 */

#include <stdio.h>
#include <string.h>

#include "aik_spi_protocol.h"
#include "ch32h417_ch585_spi_link.h"
#include "ch585_link.h"

#define FAKE_MAX_STEPS 8U

typedef struct
{
    uint16_t len;
    int result;
    uint8_t rx[AIK_SPI_HOST_CMD_SIZE];
} fake_step_t;

static fake_step_t s_steps[FAKE_MAX_STEPS];
static uint8_t s_step_count;
static uint8_t s_step_index;
static uint8_t s_failures;
static ch32h417_ch585_spi_link_side_t s_last_side;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

void ch32h417_ch585_spi_link_config_for_side(
    ch32h417_ch585_spi_link_side_t side,
    ch32h417_ch585_spi_link_config_t *config)
{
    config->side = side;
}

void ch32h417_ch585_spi_link_init(
    const ch32h417_ch585_spi_link_config_t *config)
{
    s_last_side = config->side;
}

int ch32h417_ch585_spi_link_transfer(const uint8_t *tx,
                                     uint8_t *rx,
                                     uint16_t len)
{
    fake_step_t *step;

    CHECK(tx != 0);
    CHECK(rx != 0);
    CHECK(s_step_index < s_step_count);
    if(s_step_index >= s_step_count)
    {
        return CH32H417_CH585_SPI_LINK_ERR_PARAM;
    }

    step = &s_steps[s_step_index++];
    CHECK(step->len == len);
    memset(rx, 0, len);
    memcpy(rx, step->rx, len);
    return step->result;
}

uint32_t ch32h417_ch585_spi_link_last_diag(void)
{
    return (uint32_t)(0xA500U + s_step_index);
}

static void fake_reset(void)
{
    memset(s_steps, 0, sizeof(s_steps));
    s_step_count = 0U;
    s_step_index = 0U;
    s_last_side = CH32H417_CH585_SPI_LINK_SIDE_LEFT;
    v3f_ch585_link_init();
}

static void fake_add(uint16_t len, int result, const void *rx)
{
    fake_step_t *step;
    uint16_t copy_len;

    CHECK(s_step_count < FAKE_MAX_STEPS);
    if(s_step_count >= FAKE_MAX_STEPS)
    {
        return;
    }

    step = &s_steps[s_step_count++];
    step->len = len;
    step->result = result;
    if(rx != 0)
    {
        /*
         * Every slave response is 12 bytes, including a response drained
         * during the leading 32-byte command transfer.
         */
        copy_len = (len < AIK_SPI_HALF_STATE_SIZE) ?
                   len :
                   AIK_SPI_HALF_STATE_SIZE;
        memcpy(step->rx, rx, copy_len);
    }
}

static void make_poll_cmd(aik_spi_host_cmd_v1_t *cmd,
                          uint8_t command,
                          uint16_t host_seq)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->cmd = command;
    cmd->host_seq = host_seq;
    aik_spi_host_cmd_finish(cmd);
}

static void make_state(aik_spi_half_state_v1_t *state, uint16_t seq)
{
    memset(state, 0, sizeof(*state));
    state->half_seq = seq;
    aik_spi_half_set_bit(state, 3U);
    aik_spi_half_state_finish(state, AIK_HALF_FRAME_BITS_LEFT);
}

static void make_status(aik_spi_profile_status_v1_t *status,
                        uint8_t half_id,
                        uint16_t ack_seq)
{
    memset(status, 0, sizeof(*status));
    status->ack_seq = ack_seq;
    status->half_id = half_id;
    status->flags = AIK_PROFILE_STATUS_FLAG_VALID;
    aik_spi_profile_status_finish(status);
}

static void make_xfer(aik_spi_profile_xfer_v1_t *xfer,
                      uint8_t half_id,
                      uint16_t ack_seq)
{
    memset(xfer, 0, sizeof(*xfer));
    xfer->ack_seq = ack_seq;
    xfer->half_id = half_id;
    xfer->state = AIK_SPI_XFER_STATE_RECEIVING;
    aik_spi_profile_xfer_finish(xfer);
}

static void test_normal_poll(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_half_state_v1_t expected;
    aik_spi_half_state_v1_t actual;
    v3f_ch585_link_stats_t stats;

    fake_reset();
    make_poll_cmd(&cmd, AIK_SPI_CMD_POLL, 10U);
    make_state(&expected, 77U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, 0);
    fake_add(AIK_SPI_HALF_STATE_SIZE, CH32H417_CH585_SPI_LINK_OK,
             &expected);

    CHECK(v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &cmd, &actual) != 0U);
    CHECK(s_step_index == 2U);
    CHECK(actual.half_seq == expected.half_seq);
    CHECK(s_last_side == CH32H417_CH585_SPI_LINK_SIDE_LEFT);
    v3f_ch585_link_stats(AIK_HALF_ID_LEFT, &stats);
    CHECK(stats.ok_frames == 1U);
    CHECK(stats.invalid_frames == 0U);
    CHECK(stats.command_phase_frames == 0U);
}

static void test_wired_poll_drains_command_phase_state(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_half_state_v1_t expected;
    aik_spi_half_state_v1_t actual;
    v3f_ch585_link_stats_t stats;

    fake_reset();
    make_poll_cmd(&cmd, AIK_SPI_CMD_POLL, 11U);
    make_state(&expected, 88U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK,
             &expected);

    CHECK(v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &cmd, &actual) != 0U);
    CHECK(s_step_index == 1U);
    CHECK(actual.half_seq == expected.half_seq);
    v3f_ch585_link_stats(AIK_HALF_ID_LEFT, &stats);
    CHECK(stats.command_phase_frames == 1U);
}

static void test_wireless_push_does_not_accept_command_phase_state(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_half_state_v1_t state;
    aik_spi_half_state_v1_t actual;

    fake_reset();
    make_poll_cmd(&cmd, AIK_SPI_CMD_PUSH_RIGHT_STATE, 12U);
    make_state(&state, 89U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, &state);

    CHECK(v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &cmd, &actual) == 0U);
    CHECK(s_step_index == 1U);
}

static void test_invalid_read_stops_after_one_short_frame(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_half_state_v1_t actual;
    v3f_ch585_link_stats_t stats;

    fake_reset();
    make_poll_cmd(&cmd, AIK_SPI_CMD_POLL, 13U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, 0);
    fake_add(AIK_SPI_HALF_STATE_SIZE, CH32H417_CH585_SPI_LINK_OK, 0);

    CHECK(v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &cmd, &actual) == 0U);
    CHECK(s_step_index == 2U);
    v3f_ch585_link_stats(AIK_HALF_ID_LEFT, &stats);
    CHECK(stats.invalid_frames == 1U);
}

static void test_profile_status_requires_half_and_sequence(void)
{
    aik_spi_profile_status_v1_t status;
    aik_spi_profile_status_v1_t actual;

    fake_reset();
    make_status(&status, AIK_HALF_ID_RIGHT, 21U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, 0);
    fake_add(AIK_SPI_HALF_STATE_SIZE, CH32H417_CH585_SPI_LINK_OK,
             &status);
    CHECK(v3f_ch585_link_query_profile_status(
              AIK_HALF_ID_RIGHT, 21U, &actual) != 0U);
    CHECK(s_last_side == CH32H417_CH585_SPI_LINK_SIDE_RIGHT);

    fake_reset();
    make_status(&status, AIK_HALF_ID_LEFT, 21U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, 0);
    fake_add(AIK_SPI_HALF_STATE_SIZE, CH32H417_CH585_SPI_LINK_OK,
             &status);
    CHECK(v3f_ch585_link_query_profile_status(
              AIK_HALF_ID_RIGHT, 21U, &actual) == 0U);

    fake_reset();
    make_status(&status, AIK_HALF_ID_RIGHT, 20U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, 0);
    fake_add(AIK_SPI_HALF_STATE_SIZE, CH32H417_CH585_SPI_LINK_OK,
             &status);
    CHECK(v3f_ch585_link_query_profile_status(
              AIK_HALF_ID_RIGHT, 21U, &actual) == 0U);
}

static void test_matching_status_or_xfer_can_be_drained(void)
{
    aik_spi_profile_status_v1_t status;
    aik_spi_profile_status_v1_t actual_status;
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_profile_xfer_v1_t xfer;
    aik_spi_profile_xfer_v1_t actual_xfer;

    fake_reset();
    make_status(&status, AIK_HALF_ID_LEFT, 30U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK,
             &status);
    CHECK(v3f_ch585_link_query_profile_status(
              AIK_HALF_ID_LEFT, 30U, &actual_status) != 0U);
    CHECK(s_step_index == 1U);

    fake_reset();
    make_poll_cmd(&cmd, AIK_SPI_CMD_PROFILE_GET_XFER, 31U);
    make_xfer(&xfer, AIK_HALF_ID_LEFT, 31U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, &xfer);
    CHECK(v3f_ch585_link_profile_cmd(
              AIK_HALF_ID_LEFT, &cmd, &actual_xfer) != 0U);
    CHECK(s_step_index == 1U);
    CHECK(actual_xfer.ack_seq == 31U);
}

static void test_profile_xfer_rejects_wrong_half(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_profile_xfer_v1_t xfer;
    aik_spi_profile_xfer_v1_t actual;

    fake_reset();
    make_poll_cmd(&cmd, AIK_SPI_CMD_PROFILE_BEGIN, 40U);
    make_xfer(&xfer, AIK_HALF_ID_RIGHT, 40U);
    fake_add(AIK_SPI_HOST_CMD_SIZE, CH32H417_CH585_SPI_LINK_OK, 0);
    fake_add(AIK_SPI_HALF_STATE_SIZE, CH32H417_CH585_SPI_LINK_OK, &xfer);
    CHECK(v3f_ch585_link_profile_cmd(
              AIK_HALF_ID_LEFT, &cmd, &actual) == 0U);
}

static void test_driver_error_is_counted(void)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_half_state_v1_t actual;
    v3f_ch585_link_stats_t stats;

    fake_reset();
    make_poll_cmd(&cmd, AIK_SPI_CMD_POLL, 50U);
    fake_add(AIK_SPI_HOST_CMD_SIZE,
             CH32H417_CH585_SPI_LINK_ERR_RXNE, 0);
    CHECK(v3f_ch585_link_poll(
              AIK_HALF_ID_LEFT, &cmd, &actual) == 0U);
    v3f_ch585_link_stats(AIK_HALF_ID_LEFT, &stats);
    CHECK(stats.link_errors == 1U);
    CHECK(stats.last_diag == 0xA501U);
}

int main(void)
{
    test_normal_poll();
    test_wired_poll_drains_command_phase_state();
    test_wireless_push_does_not_accept_command_phase_state();
    test_invalid_read_stops_after_one_short_frame();
    test_profile_status_requires_half_and_sequence();
    test_matching_status_or_xfer_can_be_drained();
    test_profile_xfer_rejects_wrong_half();
    test_driver_error_is_counted();

    if(s_failures != 0U)
    {
        printf("host_ch585_link_test: %u failure(s)\n",
               (unsigned int)s_failures);
        return 1;
    }
    printf("host_ch585_link_test: PASS\n");
    return 0;
}
