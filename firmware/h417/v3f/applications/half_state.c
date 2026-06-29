#include "half_state.h"

#include <string.h>

static void set_global_key(v3f_global_key_state_t *state, uint8_t key_id)
{
    if(key_id < AIK_KEY_COUNT_TOTAL)
    {
        state->down[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
    }
}

static uint8_t half_key_down(const aik_spi_half_state_v1_t *half, uint8_t key_id)
{
    return (uint8_t)((half->down_bits[key_id >> 3] >> (key_id & 7U)) & 1U);
}

void v3f_half_state_clear(v3f_global_key_state_t *state)
{
    if(state != 0)
    {
        memset(state->down, 0, sizeof(state->down));
    }
}

void v3f_half_state_merge(const aik_spi_half_state_v1_t *left,
                          const aik_spi_half_state_v1_t *right,
                          v3f_global_key_state_t *out)
{
    uint8_t key;

    if(out == 0)
    {
        return;
    }

    v3f_half_state_clear(out);

    if(right != 0)
    {
        for(key = 0U; key < AIK_KEY_COUNT_RIGHT; key++)
        {
            if(half_key_down(right, key) != 0U)
            {
                set_global_key(out, key);
            }
        }
    }

    if(left != 0)
    {
        for(key = 0U; key < AIK_KEY_COUNT_LEFT; key++)
        {
            if(half_key_down(left, key) != 0U)
            {
                set_global_key(out, (uint8_t)(AIK_KEY_COUNT_RIGHT + key));
            }
        }
    }
}

uint8_t v3f_global_key_is_down(const v3f_global_key_state_t *state,
                               uint8_t key_id)
{
    if((state == 0) || (key_id >= AIK_KEY_COUNT_TOTAL))
    {
        return 0U;
    }
    return (uint8_t)((state->down[key_id >> 3] >> (key_id & 7U)) & 1U);
}
