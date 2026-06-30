#include "profile_runtime.h"

#include <string.h>

#define HID_USAGE_KEYBOARD_A 0x04U

static aik_profile_runtime_v1_t s_runtime;
static uint8_t s_runtime_loaded;

void v3f_profile_runtime_clear(void)
{
    memset(&s_runtime, 0, sizeof(s_runtime));
    s_runtime_loaded = 0U;
}

int v3f_profile_runtime_apply(const aik_profile_runtime_v1_t *runtime)
{
    if(aik_profile_runtime_valid(runtime) == 0U)
    {
        v3f_profile_runtime_clear();
        return V3F_PROFILE_RUNTIME_ERR_INVALID;
    }

    s_runtime = *runtime;
    s_runtime_loaded = 1U;
    return V3F_PROFILE_RUNTIME_OK;
}

uint8_t v3f_profile_runtime_loaded(void)
{
    return s_runtime_loaded;
}

uint32_t v3f_profile_runtime_generation(void)
{
    return s_runtime_loaded ? s_runtime.generation : 0U;
}

void v3f_profile_runtime_build_nkro16(
    const v3f_global_key_state_t *keys,
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint8_t key_id;

    memset(nkro16, 0, AIK_NKRO_REPORT_BYTES);
    if((keys == 0) || (s_runtime_loaded == 0U))
    {
        return;
    }

    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        const aik_profile_key_output_v1_t *output =
            &s_runtime.key_output[key_id];

        if(v3f_global_key_is_down(keys, key_id) == 0U)
        {
            continue;
        }

        if(output->modifier_mask != 0U)
        {
            nkro16[0] |= output->modifier_mask;
        }

        if(output->usage >= HID_USAGE_KEYBOARD_A)
        {
            uint8_t bit_index =
                (uint8_t)(output->usage - HID_USAGE_KEYBOARD_A);
            uint8_t byte_index = (uint8_t)(2U + (bit_index >> 3));
            uint8_t bit_mask = (uint8_t)(1U << (bit_index & 7U));

            if(byte_index < AIK_NKRO_REPORT_BYTES)
            {
                nkro16[byte_index] |= bit_mask;
            }
        }
    }
}
