#include "ch585_link.h"

#include <string.h>

#include "ch32h417_ch585_spi_link.h"

typedef enum
{
    V3F_CH585_RESPONSE_NONE = 0,
    V3F_CH585_RESPONSE_STATE,
    V3F_CH585_RESPONSE_PROFILE_STATUS,
    V3F_CH585_RESPONSE_PROFILE_XFER,
} v3f_ch585_response_kind_t;

typedef union
{
    aik_spi_half_state_v1_t state;
    aik_spi_profile_status_v1_t status;
    aik_spi_profile_xfer_v1_t xfer;
} v3f_ch585_response_t;

static v3f_ch585_link_stats_t s_stats[2];

static ch32h417_ch585_spi_link_side_t link_side_from_half(uint8_t half_id)
{
    return (half_id == AIK_HALF_ID_RIGHT) ?
        CH32H417_CH585_SPI_LINK_SIDE_RIGHT :
        CH32H417_CH585_SPI_LINK_SIDE_LEFT;
}

static v3f_ch585_response_kind_t decode_response(
    const uint8_t raw[AIK_SPI_HALF_STATE_SIZE],
    v3f_ch585_response_t *response)
{
    if((raw == 0) || (response == 0))
    {
        return V3F_CH585_RESPONSE_NONE;
    }

    memcpy(response, raw, AIK_SPI_HALF_STATE_SIZE);
    if(aik_spi_half_state_valid(&response->state) != 0U)
    {
        return V3F_CH585_RESPONSE_STATE;
    }
    if(aik_spi_profile_status_valid(&response->status) != 0U)
    {
        return V3F_CH585_RESPONSE_PROFILE_STATUS;
    }
    if(aik_spi_profile_xfer_valid(&response->xfer) != 0U)
    {
        return V3F_CH585_RESPONSE_PROFILE_XFER;
    }
    return V3F_CH585_RESPONSE_NONE;
}

static uint8_t transfer_checked(uint8_t half_id,
                                const uint8_t *tx,
                                uint8_t *rx,
                                uint16_t len)
{
    int rc = ch32h417_ch585_spi_link_transfer(tx, rx, len);

    s_stats[half_id].last_diag = ch32h417_ch585_spi_link_last_diag();
    if(rc != CH32H417_CH585_SPI_LINK_OK)
    {
        s_stats[half_id].link_errors++;
        return 0U;
    }
    return 1U;
}

static uint8_t begin_command(uint8_t half_id,
                             const aik_spi_host_cmd_v1_t *cmd,
                             uint8_t cmd_rx[AIK_SPI_HOST_CMD_SIZE])
{
    ch32h417_ch585_spi_link_config_t config;

    ch32h417_ch585_spi_link_config_for_side(link_side_from_half(half_id),
                                            &config);
    ch32h417_ch585_spi_link_init(&config);
    return transfer_checked(half_id,
                            (const uint8_t *)cmd,
                            cmd_rx,
                            (uint16_t)AIK_SPI_HOST_CMD_SIZE);
}

static uint8_t read_response(uint8_t half_id,
                             uint8_t rx[AIK_SPI_HALF_STATE_SIZE])
{
    uint8_t read_dummy[AIK_SPI_HALF_STATE_SIZE];

    memset(read_dummy, 0, sizeof(read_dummy));
    return transfer_checked(half_id,
                            read_dummy,
                            rx,
                            (uint16_t)AIK_SPI_HALF_STATE_SIZE);
}

static void record_state_diag(uint8_t half_id,
                              const aik_spi_half_state_v1_t *state)
{
    memcpy(s_stats[half_id].last_rx_head,
           state,
           sizeof(s_stats[half_id].last_rx_head));
    memcpy(s_stats[half_id].last_rx_down,
           state->down_bits,
           sizeof(s_stats[half_id].last_rx_down));
    s_stats[half_id].last_magic = state->magic;
    s_stats[half_id].last_type = state->type;
    s_stats[half_id].last_crc = state->crc16;
    s_stats[half_id].last_calc_crc = aik_spi_half_state_crc(state);
}

static uint8_t accept_state(uint8_t half_id,
                            const aik_spi_half_state_v1_t *state,
                            aik_spi_half_state_v1_t *out)
{
    if(aik_spi_half_state_valid(state) == 0U)
    {
        return 0U;
    }

    *out = *state;
    record_state_diag(half_id, state);
    s_stats[half_id].ok_frames++;
    s_stats[half_id].last_seq = state->half_seq;
    return 1U;
}

static uint8_t profile_status_matches(
    const aik_spi_profile_status_v1_t *status,
    uint8_t half_id,
    uint16_t host_seq)
{
    return (uint8_t)(
        (aik_spi_profile_status_valid(status) != 0U) &&
        (status->half_id == half_id) &&
        (status->ack_seq == host_seq));
}

static uint8_t profile_xfer_matches(
    const aik_spi_profile_xfer_v1_t *xfer,
    uint8_t half_id,
    uint16_t host_seq)
{
    return (uint8_t)(
        (aik_spi_profile_xfer_valid(xfer) != 0U) &&
        (xfer->half_id == half_id) &&
        (xfer->ack_seq == host_seq));
}

void v3f_ch585_link_init(void)
{
    memset(s_stats, 0, sizeof(s_stats));
}

uint8_t v3f_ch585_link_poll(uint8_t half_id,
                            const aik_spi_host_cmd_v1_t *cmd,
                            aik_spi_half_state_v1_t *out)
{
    uint8_t cmd_rx[AIK_SPI_HOST_CMD_SIZE];
    uint8_t rx[AIK_SPI_HALF_STATE_SIZE];
    v3f_ch585_response_t response;
    v3f_ch585_response_kind_t kind;

    if((cmd == 0) || (out == 0) || (half_id > AIK_HALF_ID_RIGHT))
    {
        return 0U;
    }

    if(begin_command(half_id, cmd, cmd_rx) == 0U)
    {
        return 0U;
    }

    kind = decode_response(cmd_rx, &response);
    if(kind != V3F_CH585_RESPONSE_NONE)
    {
        /*
         * The half was still in TX12 while this CMD32 was clocked. The
         * command therefore was not received; the first 12 bytes are the
         * pending response that the command transfer has now drained.
         * Do not issue another 12-byte read here: that would land in the
         * half's next RX32 phase and keep the two sides out of step.
         */
        s_stats[half_id].command_phase_frames++;
        if((kind == V3F_CH585_RESPONSE_STATE) &&
           (cmd->cmd == AIK_SPI_CMD_POLL))
        {
            return accept_state(half_id, &response.state, out);
        }
        return 0U;
    }

    if(read_response(half_id, rx) == 0U)
    {
        return 0U;
    }

    kind = decode_response(rx, &response);
    if(kind == V3F_CH585_RESPONSE_STATE)
    {
        return accept_state(half_id, &response.state, out);
    }

    memcpy(&response.state, rx, sizeof(response.state));
    record_state_diag(half_id, &response.state);
    s_stats[half_id].invalid_frames++;
    return 0U;
}

uint8_t v3f_ch585_link_query_profile_status(
    uint8_t half_id,
    uint16_t host_seq,
    aik_spi_profile_status_v1_t *out)
{
    aik_spi_host_cmd_v1_t cmd;
    uint8_t cmd_rx[AIK_SPI_HOST_CMD_SIZE];
    uint8_t rx[AIK_SPI_HALF_STATE_SIZE];
    v3f_ch585_response_t response;
    v3f_ch585_response_kind_t kind;

    if((out == 0) || (half_id > AIK_HALF_ID_RIGHT))
    {
        return 0U;
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = AIK_SPI_CMD_GET_PROFILE_STATUS;
    cmd.host_seq = host_seq;
    aik_spi_host_cmd_finish(&cmd);

    if(begin_command(half_id, &cmd, cmd_rx) == 0U)
    {
        return 0U;
    }

    kind = decode_response(cmd_rx, &response);
    if(kind != V3F_CH585_RESPONSE_NONE)
    {
        s_stats[half_id].command_phase_frames++;
        if((kind == V3F_CH585_RESPONSE_PROFILE_STATUS) &&
           (profile_status_matches(&response.status,
                                   half_id,
                                   host_seq) != 0U))
        {
            *out = response.status;
            s_stats[half_id].profile_status_ok++;
            s_stats[half_id].last_profile_status = *out;
            return 1U;
        }
        s_stats[half_id].profile_status_invalid++;
        return 0U;
    }

    if(read_response(half_id, rx) == 0U)
    {
        return 0U;
    }

    kind = decode_response(rx, &response);
    if((kind == V3F_CH585_RESPONSE_PROFILE_STATUS) &&
       (profile_status_matches(&response.status,
                               half_id,
                               host_seq) != 0U))
    {
        *out = response.status;
        s_stats[half_id].profile_status_ok++;
        s_stats[half_id].last_profile_status = *out;
        return 1U;
    }

    s_stats[half_id].profile_status_invalid++;
    return 0U;
}

uint8_t v3f_ch585_link_profile_cmd(uint8_t half_id,
                                   const aik_spi_host_cmd_v1_t *cmd,
                                   aik_spi_profile_xfer_v1_t *out)
{
    uint8_t cmd_rx[AIK_SPI_HOST_CMD_SIZE];
    uint8_t rx[AIK_SPI_HALF_STATE_SIZE];
    v3f_ch585_response_t response;
    v3f_ch585_response_kind_t kind;

    if((cmd == 0) || (out == 0) || (half_id > AIK_HALF_ID_RIGHT))
    {
        return 0U;
    }

    if(begin_command(half_id, cmd, cmd_rx) == 0U)
    {
        return 0U;
    }

    kind = decode_response(cmd_rx, &response);
    if(kind != V3F_CH585_RESPONSE_NONE)
    {
        s_stats[half_id].command_phase_frames++;
        if((kind == V3F_CH585_RESPONSE_PROFILE_XFER) &&
           (profile_xfer_matches(&response.xfer,
                                 half_id,
                                 cmd->host_seq) != 0U))
        {
            *out = response.xfer;
            return 1U;
        }
        s_stats[half_id].invalid_frames++;
        return 0U;
    }

    if(read_response(half_id, rx) == 0U)
    {
        return 0U;
    }

    kind = decode_response(rx, &response);
    if((kind == V3F_CH585_RESPONSE_PROFILE_XFER) &&
       (profile_xfer_matches(&response.xfer,
                             half_id,
                             cmd->host_seq) != 0U))
    {
        *out = response.xfer;
        return 1U;
    }

    s_stats[half_id].invalid_frames++;
    return 0U;
}

void v3f_ch585_link_stats(uint8_t half_id, v3f_ch585_link_stats_t *stats)
{
    if((stats == 0) || (half_id > AIK_HALF_ID_RIGHT))
    {
        return;
    }
    *stats = s_stats[half_id];
}
