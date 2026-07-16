#ifndef AIK_HOST_SHORTCUT_H
#define AIK_HOST_SHORTCUT_H

#include <stdint.h>

#include "aik_spi_protocol.h"

#define AIK_HOST_SHORTCUT_NONE          0U
#define AIK_HOST_SHORTCUT_CLAUDE_CODE   1U

/* Dedicated host-only key captured by the software companion. */
#define AIK_HOST_SHORTCUT_HID_USAGE_F24 0x73U

typedef struct
{
    uint8_t center_consumed;
    uint8_t emit_active;
} aik_host_shortcut_state_t;

static inline void aik_host_shortcut_reset(aik_host_shortcut_state_t *state)
{
    if(state != 0)
    {
        state->center_consumed = 0U;
        state->emit_active = 0U;
    }
}

/*
 * Once Fn+center is recognized, keep center consumed until a confirmed
 * center release. An unknown center state releases F24 to avoid a stuck key,
 * but it cannot re-arm or leak Enter when the link recovers.
 */
static inline uint8_t aik_host_shortcut_update_valid(
    aik_host_shortcut_state_t *state,
    uint8_t fn_down,
    uint8_t center_down,
    uint8_t center_valid)
{
    if(state == 0)
    {
        return AIK_HOST_SHORTCUT_NONE;
    }
    if(center_valid == 0U)
    {
        state->emit_active = 0U;
        return AIK_HOST_SHORTCUT_NONE;
    }
    if(center_down == 0U)
    {
        state->center_consumed = 0U;
        state->emit_active = 0U;
        return AIK_HOST_SHORTCUT_NONE;
    }
    if((state->center_consumed == 0U) && (fn_down != 0U))
    {
        state->center_consumed = 1U;
        state->emit_active = 1U;
    }
    return (state->emit_active != 0U) ?
           AIK_HOST_SHORTCUT_CLAUDE_CODE :
           AIK_HOST_SHORTCUT_NONE;
}

static inline uint8_t aik_host_shortcut_update(
    aik_host_shortcut_state_t *state,
    uint8_t fn_down,
    uint8_t center_down)
{
    return aik_host_shortcut_update_valid(
        state, fn_down, center_down, 1U);
}

static inline uint8_t aik_host_shortcut_center_consumed(
    const aik_host_shortcut_state_t *state)
{
    return (state != 0) ? state->center_consumed : 0U;
}

static inline void aik_host_shortcut_apply(
    uint8_t shortcut,
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint8_t bit_index;
    uint8_t byte_index;

    if((shortcut != AIK_HOST_SHORTCUT_CLAUDE_CODE) || (nkro16 == 0))
    {
        return;
    }

    bit_index =
        (uint8_t)(AIK_HOST_SHORTCUT_HID_USAGE_F24 - 0x04U);
    byte_index = (uint8_t)(2U + (bit_index >> 3));
    if(byte_index < AIK_NKRO_REPORT_BYTES)
    {
        nkro16[byte_index] |=
            (uint8_t)(1U << (bit_index & 7U));
    }
}

#endif /* AIK_HOST_SHORTCUT_H */
