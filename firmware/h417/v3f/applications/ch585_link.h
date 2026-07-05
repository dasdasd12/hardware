#ifndef V3F_CH585_LINK_H
#define V3F_CH585_LINK_H

#include <stdint.h>
#include "aik_spi_protocol.h"

typedef struct
{
    uint32_t ok_frames;
    uint32_t link_errors;
    uint32_t invalid_frames;
    uint16_t last_seq;
    uint8_t last_magic;
    uint8_t last_type;
    uint16_t last_crc;
    uint16_t last_calc_crc;
    uint8_t last_rx_head[4];
    uint8_t last_rx_down[AIK_HALF_DOWN_BITS_BYTES];
    uint32_t last_diag;
    uint32_t profile_status_ok;
    uint32_t profile_status_invalid;
    aik_spi_profile_status_v1_t last_profile_status;
} v3f_ch585_link_stats_t;

void v3f_ch585_link_init(void);
uint8_t v3f_ch585_link_poll(uint8_t half_id,
                            const aik_spi_host_cmd_v1_t *cmd,
                            aik_spi_half_state_v1_t *out);
uint8_t v3f_ch585_link_query_profile_status(
    uint8_t half_id,
    uint16_t host_seq,
    aik_spi_profile_status_v1_t *out);
void v3f_ch585_link_stats(uint8_t half_id, v3f_ch585_link_stats_t *stats);

#endif
