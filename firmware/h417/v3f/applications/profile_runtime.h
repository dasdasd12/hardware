#ifndef V3F_PROFILE_RUNTIME_H
#define V3F_PROFILE_RUNTIME_H

#include <stdint.h>

#include "aik_profile_runtime.h"
#include "aik_spi_protocol.h"
#include "half_state.h"

#define V3F_PROFILE_RUNTIME_OK 0
#define V3F_PROFILE_RUNTIME_ERR_INVALID -1

void v3f_profile_runtime_clear(void);
int v3f_profile_runtime_apply(const aik_profile_runtime_v1_t *runtime);
uint8_t v3f_profile_runtime_loaded(void);
uint32_t v3f_profile_runtime_generation(void);
void v3f_profile_runtime_build_nkro16(
    const v3f_global_key_state_t *keys,
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);

#endif
