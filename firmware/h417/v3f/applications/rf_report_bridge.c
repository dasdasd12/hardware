#include "rf_report_bridge.h"

#include <string.h>

void v3f_rf_report_bridge_prepare_cmd(aik_spi_host_cmd_v1_t *cmd,
                                      uint16_t host_seq,
                                      const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                      uint8_t enable_rf)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->cmd = (enable_rf != 0U) ? AIK_SPI_CMD_POLL_WITH_RF : AIK_SPI_CMD_POLL;
    cmd->host_seq = host_seq;
    if(nkro16 != 0)
    {
        memcpy(cmd->nkro16, nkro16, AIK_NKRO_REPORT_BYTES);
    }
    aik_spi_host_cmd_finish(cmd);
}

void v3f_rf_report_bridge_prepare_right_state_cmd(
    aik_spi_host_cmd_v1_t *cmd,
    uint16_t host_seq,
    const aik_spi_half_state_v1_t *right_state)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->cmd = AIK_SPI_CMD_PUSH_RIGHT_STATE;
    cmd->host_seq = host_seq;
    if(right_state != 0)
    {
        memcpy(cmd->nkro16, right_state, sizeof(*right_state));
    }
    aik_spi_host_cmd_finish(cmd);
}
