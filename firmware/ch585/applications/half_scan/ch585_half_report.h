#ifndef CH585_HALF_REPORT_H
#define CH585_HALF_REPORT_H

#include <stdint.h>

#include "aik_spi_protocol.h"

void ch585_half_report_build_nkro16(const aik_spi_half_state_v1_t *left,
                                    const aik_spi_half_state_v1_t *right,
                                    uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);

#endif
