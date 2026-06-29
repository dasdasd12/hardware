#ifndef V3F_HALF_STATE_H
#define V3F_HALF_STATE_H

#include <stdint.h>
#include "aik_spi_protocol.h"

#define V3F_GLOBAL_DOWN_BYTES 10U

typedef struct
{
    uint8_t down[V3F_GLOBAL_DOWN_BYTES];
} v3f_global_key_state_t;

void v3f_half_state_clear(v3f_global_key_state_t *state);
void v3f_half_state_merge(const aik_spi_half_state_v1_t *left,
                          const aik_spi_half_state_v1_t *right,
                          v3f_global_key_state_t *out);
uint8_t v3f_global_key_is_down(const v3f_global_key_state_t *state,
                               uint8_t key_id);

#endif
