#ifndef AIK_APPROVAL_CONTROL_H
#define AIK_APPROVAL_CONTROL_H

#include <stdint.h>

#include "aik_spi_protocol.h"

#define AIK_APPROVAL_CONTROL_NONE        0U
#define AIK_APPROVAL_CONTROL_SELECT_YES  1U
#define AIK_APPROVAL_CONTROL_SELECT_NO   2U
#define AIK_APPROVAL_CONTROL_CONFIRM_YES 3U
#define AIK_APPROVAL_CONTROL_CONFIRM_NO  4U

/* Dedicated host-only keys captured by the software companion. */
#define AIK_APPROVAL_CONTROL_HID_USAGE_YES 0x71U /* Keyboard F22 */
#define AIK_APPROVAL_CONTROL_HID_USAGE_NO  0x72U /* Keyboard F23 */

#define AIK_APPROVAL_NAV_IDLE     0U
#define AIK_APPROVAL_NAV_ORDINARY 1U
#define AIK_APPROVAL_NAV_YES      2U
#define AIK_APPROVAL_NAV_NO       3U

#define AIK_APPROVAL_CONFIRM_IDLE     0U
#define AIK_APPROVAL_CONFIRM_ORDINARY 1U
#define AIK_APPROVAL_CONFIRM_APPROVAL 2U

typedef struct
{
    uint8_t nav_class;
    uint8_t nav_ready;
    uint8_t confirm_class;
    uint8_t confirm_ready;
    uint8_t confirm_emit_active;
    uint8_t confirm_selected_yes;
} aik_approval_control_state_t;

static inline void aik_approval_control_reset(
    aik_approval_control_state_t *state)
{
    if(state != 0)
    {
        state->nav_class = AIK_APPROVAL_NAV_IDLE;
        state->nav_ready = 0U;
        state->confirm_class = AIK_APPROVAL_CONFIRM_IDLE;
        state->confirm_ready = 0U;
        state->confirm_emit_active = 0U;
        state->confirm_selected_yes = 1U;
    }
}

/*
 * A direction gesture is classified only on its first valid down frame.
 * Pressing Fn after an ordinary arrow has started cannot upgrade that same
 * hold into an approval action. Once classified as an approval gesture, both
 * directions remain consumed until a valid all-released frame is observed.
 */
static inline uint8_t aik_approval_control_update_nav_valid(
    aik_approval_control_state_t *state,
    uint8_t approval_active,
    uint8_t fn_down,
    uint8_t up_down,
    uint8_t down_down,
    uint8_t nav_valid)
{
    if(state == 0)
    {
        return AIK_APPROVAL_CONTROL_NONE;
    }
    if(nav_valid == 0U)
    {
        if(state->nav_class == AIK_APPROVAL_NAV_IDLE)
        {
            state->nav_ready = 0U;
        }
        return AIK_APPROVAL_CONTROL_NONE;
    }
    if((up_down == 0U) && (down_down == 0U))
    {
        state->nav_class = AIK_APPROVAL_NAV_IDLE;
        state->nav_ready = 1U;
        return AIK_APPROVAL_CONTROL_NONE;
    }
    if(state->nav_class != AIK_APPROVAL_NAV_IDLE)
    {
        return AIK_APPROVAL_CONTROL_NONE;
    }
    if((state->nav_ready != 0U) &&
       (approval_active != 0U) &&
       (fn_down != 0U) &&
       ((up_down ^ down_down) != 0U))
    {
        state->nav_ready = 0U;
        if(up_down != 0U)
        {
            state->nav_class = AIK_APPROVAL_NAV_YES;
            return AIK_APPROVAL_CONTROL_SELECT_YES;
        }
        state->nav_class = AIK_APPROVAL_NAV_NO;
        return AIK_APPROVAL_CONTROL_SELECT_NO;
    }

    state->nav_class = AIK_APPROVAL_NAV_ORDINARY;
    state->nav_ready = 0U;
    return AIK_APPROVAL_CONTROL_NONE;
}

static inline uint8_t aik_approval_control_nav_consumed(
    const aik_approval_control_state_t *state)
{
    return (uint8_t)(
        (state != 0) &&
        ((state->nav_class == AIK_APPROVAL_NAV_YES) ||
         (state->nav_class == AIK_APPROVAL_NAV_NO)));
}

/*
 * The right EC11 press arrives as a short debounced pulse. Classify its
 * rising edge once, latch the selected answer for the whole pulse, and keep
 * the normal Mute action consumed even when Fn is released first.
 *
 * If the right link becomes unknown, release F22/F23 immediately but retain
 * the classification. A valid released frame is required before re-arming.
 */
static inline uint8_t aik_approval_control_update_confirm_valid(
    aik_approval_control_state_t *state,
    uint8_t approval_active,
    uint8_t selected_yes,
    uint8_t fn_down,
    uint8_t press_down,
    uint8_t press_valid)
{
    if(state == 0)
    {
        return AIK_APPROVAL_CONTROL_NONE;
    }
    if(press_valid == 0U)
    {
        state->confirm_emit_active = 0U;
        if(state->confirm_class == AIK_APPROVAL_CONFIRM_IDLE)
        {
            state->confirm_ready = 0U;
        }
        return AIK_APPROVAL_CONTROL_NONE;
    }
    if(press_down == 0U)
    {
        state->confirm_class = AIK_APPROVAL_CONFIRM_IDLE;
        state->confirm_ready = 1U;
        state->confirm_emit_active = 0U;
        return AIK_APPROVAL_CONTROL_NONE;
    }
    if(state->confirm_class == AIK_APPROVAL_CONFIRM_IDLE)
    {
        if((state->confirm_ready != 0U) &&
           (approval_active != 0U) &&
           (fn_down != 0U))
        {
            state->confirm_class = AIK_APPROVAL_CONFIRM_APPROVAL;
            state->confirm_selected_yes =
                (selected_yes != 0U) ? 1U : 0U;
            state->confirm_emit_active = 1U;
        }
        else
        {
            state->confirm_class = AIK_APPROVAL_CONFIRM_ORDINARY;
            state->confirm_emit_active = 0U;
        }
        state->confirm_ready = 0U;
    }
    if((state->confirm_class == AIK_APPROVAL_CONFIRM_APPROVAL) &&
       (state->confirm_emit_active != 0U))
    {
        return (state->confirm_selected_yes != 0U) ?
               AIK_APPROVAL_CONTROL_CONFIRM_YES :
               AIK_APPROVAL_CONTROL_CONFIRM_NO;
    }
    return AIK_APPROVAL_CONTROL_NONE;
}

static inline uint8_t aik_approval_control_confirm_consumed(
    const aik_approval_control_state_t *state)
{
    return (uint8_t)(
        (state != 0) &&
        (state->confirm_class == AIK_APPROVAL_CONFIRM_APPROVAL));
}

static inline uint8_t aik_approval_control_any_consumed(
    const aik_approval_control_state_t *state)
{
    return (uint8_t)(
        (aik_approval_control_nav_consumed(state) != 0U) ||
        (aik_approval_control_confirm_consumed(state) != 0U));
}

static inline void aik_approval_control_apply_confirm(
    uint8_t action,
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint8_t usage;
    uint8_t bit_index;
    uint8_t byte_index;

    if(nkro16 == 0)
    {
        return;
    }
    if(action == AIK_APPROVAL_CONTROL_CONFIRM_YES)
    {
        usage = AIK_APPROVAL_CONTROL_HID_USAGE_YES;
    }
    else if(action == AIK_APPROVAL_CONTROL_CONFIRM_NO)
    {
        usage = AIK_APPROVAL_CONTROL_HID_USAGE_NO;
    }
    else
    {
        return;
    }

    bit_index = (uint8_t)(usage - 0x04U);
    byte_index = (uint8_t)(2U + (bit_index >> 3));
    if(byte_index < AIK_NKRO_REPORT_BYTES)
    {
        nkro16[byte_index] |=
            (uint8_t)(1U << (bit_index & 7U));
    }
}

#endif /* AIK_APPROVAL_CONTROL_H */
