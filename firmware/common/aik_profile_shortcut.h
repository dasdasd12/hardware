#ifndef AIK_PROFILE_SHORTCUT_H
#define AIK_PROFILE_SHORTCUT_H

#include <stdint.h>

#define AIK_PROFILE_SHORTCUT_NONE      0xFFU
#define AIK_PROFILE_SHORTCUT_SLOT_MASK 0x0FU

typedef struct
{
    uint8_t consuming;
} aik_profile_shortcut_state_t;

static inline void aik_profile_shortcut_reset(
    aik_profile_shortcut_state_t *state)
{
    if(state != 0)
    {
        state->consuming = 0U;
    }
}

/*
 * A chord starts when physical Fn and one or more slot keys are down.
 * Exactly one slot key emits its zero-based slot once; an ambiguous
 * multi-slot chord is consumed without switching. Once started, the
 * whole chord remains consumed until Fn and every slot key are released,
 * so changing the held keys cannot retrigger or leak a partial chord.
 * Invalid input leaves the state untouched so a short link interruption
 * cannot falsely release and re-arm a chord that is still physically held.
 */
static inline uint8_t aik_profile_shortcut_update_valid(
    aik_profile_shortcut_state_t *state,
    uint8_t fn_down,
    uint8_t slot_mask,
    uint8_t input_valid)
{
    uint8_t slot;

    if(state == 0)
    {
        return AIK_PROFILE_SHORTCUT_NONE;
    }
    if(input_valid == 0U)
    {
        return AIK_PROFILE_SHORTCUT_NONE;
    }

    slot_mask &= AIK_PROFILE_SHORTCUT_SLOT_MASK;
    if(state->consuming != 0U)
    {
        if((fn_down == 0U) && (slot_mask == 0U))
        {
            state->consuming = 0U;
        }
        return AIK_PROFILE_SHORTCUT_NONE;
    }

    if((fn_down == 0U) || (slot_mask == 0U))
    {
        return AIK_PROFILE_SHORTCUT_NONE;
    }

    state->consuming = 1U;
    if((slot_mask & (uint8_t)(slot_mask - 1U)) != 0U)
    {
        return AIK_PROFILE_SHORTCUT_NONE;
    }

    for(slot = 0U; slot < 4U; slot++)
    {
        if(slot_mask == (uint8_t)(1U << slot))
        {
            return slot;
        }
    }
    return AIK_PROFILE_SHORTCUT_NONE;
}

static inline uint8_t aik_profile_shortcut_update(
    aik_profile_shortcut_state_t *state,
    uint8_t fn_down,
    uint8_t slot_mask)
{
    return aik_profile_shortcut_update_valid(
        state, fn_down, slot_mask, 1U);
}

static inline uint8_t aik_profile_shortcut_consuming(
    const aik_profile_shortcut_state_t *state)
{
    return (state != 0) ? state->consuming : 0U;
}

static inline uint8_t aik_profile_shortcut_consumed_slot_mask(
    const aik_profile_shortcut_state_t *state,
    uint8_t slot_mask)
{
    return (aik_profile_shortcut_consuming(state) != 0U) ?
           (uint8_t)(slot_mask & AIK_PROFILE_SHORTCUT_SLOT_MASK) :
           0U;
}

#endif /* AIK_PROFILE_SHORTCUT_H */
