#ifndef V3F_DEFAULT_PROFILE_H
#define V3F_DEFAULT_PROFILE_H

#include <stdint.h>
#include "aik_spi_protocol.h"
#include "half_state.h"

void v3f_default_profile_build_nkro16(const v3f_global_key_state_t *keys,
                                      uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);

#endif
