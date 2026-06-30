#ifndef CH585_RF_NKRO_TX_H
#define CH585_RF_NKRO_TX_H

#include <stdint.h>

#include "aik_spi_protocol.h"

void ch585_rf_nkro_tx_init(void);
void ch585_rf_nkro_tx_poll(void);
void ch585_rf_nkro_tx_set_enabled(uint8_t enabled);
uint8_t ch585_rf_nkro_tx_is_enabled(void);
void ch585_rf_nkro_tx_set_report(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                 uint16_t host_seq,
                                 uint8_t flags);
uint32_t ch585_rf_nkro_tx_done_count(void);
uint32_t ch585_rf_nkro_tx_report_count(void);

#endif
