#ifndef V3F_RF_REPORT_BRIDGE_H
#define V3F_RF_REPORT_BRIDGE_H

#include <stdint.h>
#include "aik_spi_protocol.h"

void v3f_rf_report_bridge_prepare_cmd(aik_spi_host_cmd_v1_t *cmd,
                                      uint16_t host_seq,
                                      const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                      uint8_t output_mode);
void v3f_rf_report_bridge_prepare_right_state_cmd(
    aik_spi_host_cmd_v1_t *cmd,
    uint16_t host_seq,
    const aik_spi_half_state_v1_t *right_state,
    uint8_t output_mode,
    uint8_t approval_active,
    uint8_t approval_selected_yes,
    uint8_t right_state_valid,
    uint8_t battery_percent,
    uint8_t power_flags);

#endif
