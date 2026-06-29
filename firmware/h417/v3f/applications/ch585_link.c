#include "ch585_link.h"

#include <string.h>

#include "ch32h417_ch585_spi_link.h"

static v3f_ch585_link_stats_t s_stats[2];

static ch32h417_ch585_spi_link_side_t link_side_from_half(uint8_t half_id)
{
    return (half_id == AIK_HALF_ID_RIGHT) ?
        CH32H417_CH585_SPI_LINK_SIDE_RIGHT :
        CH32H417_CH585_SPI_LINK_SIDE_LEFT;
}

void v3f_ch585_link_init(void)
{
    memset(s_stats, 0, sizeof(s_stats));
}

uint8_t v3f_ch585_link_poll(uint8_t half_id,
                            const aik_spi_host_cmd_v1_t *cmd,
                            aik_spi_half_state_v1_t *out)
{
    ch32h417_ch585_spi_link_config_t config;
    uint8_t rx[AIK_SPI_HALF_STATE_SIZE];
    int rc;

    if((cmd == 0) || (out == 0) || (half_id > AIK_HALF_ID_RIGHT))
    {
        return 0U;
    }

    ch32h417_ch585_spi_link_config_for_side(link_side_from_half(half_id), &config);
    ch32h417_ch585_spi_link_init(&config);
    rc = ch32h417_ch585_spi_link_transfer((const uint8_t *)cmd,
                                          rx,
                                          (uint16_t)AIK_SPI_HALF_STATE_SIZE);
    s_stats[half_id].last_diag = ch32h417_ch585_spi_link_last_diag();
    if(rc != CH32H417_CH585_SPI_LINK_OK)
    {
        s_stats[half_id].link_errors++;
        return 0U;
    }

    memcpy(out, rx, sizeof(*out));
    memcpy(s_stats[half_id].last_rx_head,
           rx,
           sizeof(s_stats[half_id].last_rx_head));
    memcpy(s_stats[half_id].last_rx_down,
           out->down_bits,
           sizeof(s_stats[half_id].last_rx_down));
    s_stats[half_id].last_magic = out->magic;
    s_stats[half_id].last_type = out->type;
    s_stats[half_id].last_crc = out->crc16;
    s_stats[half_id].last_calc_crc = aik_spi_half_state_crc(out);
    if(aik_spi_half_state_valid(out) == 0U)
    {
        s_stats[half_id].invalid_frames++;
        return 0U;
    }

    s_stats[half_id].ok_frames++;
    s_stats[half_id].last_seq = out->half_seq;
    return 1U;
}

void v3f_ch585_link_stats(uint8_t half_id, v3f_ch585_link_stats_t *stats)
{
    if((stats == 0) || (half_id > AIK_HALF_ID_RIGHT))
    {
        return;
    }
    *stats = s_stats[half_id];
}
