#include "ch585_local_inputs.h"

#include <stddef.h>
#include <string.h>

static uint8_t elapsed_ms(uint32_t now_ms, uint32_t then_ms, uint16_t need_ms)
{
    return ((uint32_t)(now_ms - then_ms) >= need_ms) ? 1U : 0U;
}

static int8_t ec11_transition_delta(uint8_t last_ab, uint8_t ab)
{
    static const int8_t table[16] = {
        0, 1, -1, 0,
        -1, 0, 0, 1,
        1, 0, 0, -1,
        0, -1, 1, 0
    };
    return table[((last_ab & 0x03U) << 2U) | (ab & 0x03U)];
}

void ch585_debounce_button_init(ch585_debounce_button_t *button,
                                uint8_t pressed,
                                uint16_t debounce_ms,
                                uint32_t now_ms)
{
    if (button == NULL)
    {
        return;
    }

    memset(button, 0, sizeof(*button));
    button->stable_pressed = pressed ? 1U : 0U;
    button->candidate_pressed = button->stable_pressed;
    button->debounce_ms = debounce_ms;
    button->candidate_since_ms = now_ms;
    button->initialized = 1U;
}

int ch585_debounce_button_update(ch585_debounce_button_t *button,
                                 uint8_t pressed,
                                 uint32_t now_ms)
{
    uint8_t normalized = pressed ? 1U : 0U;

    if ((button == NULL) || (button->initialized == 0U))
    {
        return 0;
    }

    if (normalized != button->candidate_pressed)
    {
        button->candidate_pressed = normalized;
        button->candidate_since_ms = now_ms;
        return 0;
    }

    if ((button->stable_pressed != button->candidate_pressed) &&
        elapsed_ms(now_ms, button->candidate_since_ms, button->debounce_ms))
    {
        button->stable_pressed = button->candidate_pressed;
        return button->stable_pressed ? 1 : -1;
    }

    return 0;
}

void ch585_ec11_init(ch585_ec11_t *enc,
                     uint8_t ab,
                     uint8_t button_pressed,
                     uint16_t debounce_ms,
                     uint32_t now_ms)
{
    if (enc == NULL)
    {
        return;
    }

    memset(enc, 0, sizeof(*enc));
    enc->last_ab = (uint8_t)(ab & 0x03U);
    enc->steps_per_detent = 4U;
    ch585_debounce_button_init(&enc->button, button_pressed, debounce_ms,
                               now_ms);
    enc->initialized = 1U;
}

int ch585_ec11_update(ch585_ec11_t *enc,
                      uint8_t ab,
                      uint8_t button_pressed,
                      uint32_t now_ms,
                      ch585_local_input_event_t events[],
                      uint8_t event_cap)
{
    uint8_t count = 0U;
    uint8_t current_ab;
    int8_t delta;
    int button_event;

    if ((enc == NULL) || (enc->initialized == 0U))
    {
        return 0;
    }

    current_ab = (uint8_t)(ab & 0x03U);
    delta = ec11_transition_delta(enc->last_ab, current_ab);
    if (delta != 0)
    {
        if (enc->reverse != 0U)
        {
            delta = (int8_t)-delta;
        }
        enc->step_accum = (int8_t)(enc->step_accum + delta);
        enc->last_ab = current_ab;

        while ((enc->step_accum >= (int8_t)enc->steps_per_detent) ||
               (enc->step_accum <= -(int8_t)enc->steps_per_detent))
        {
            int8_t out_delta =
                (enc->step_accum > 0) ? (int8_t)1 : (int8_t)-1;
            enc->step_accum =
                (int8_t)(enc->step_accum -
                         (out_delta * (int8_t)enc->steps_per_detent));
            if ((events != NULL) && (count < event_cap))
            {
                events[count].type = CH585_LOCAL_EVENT_ENCODER_DELTA;
                events[count].id = 0U;
                events[count].value = out_delta;
                events[count].flags = 0U;
                count++;
            }
        }
    }
    else
    {
        enc->last_ab = current_ab;
    }

    button_event =
        ch585_debounce_button_update(&enc->button, button_pressed, now_ms);
    if ((button_event != 0) && (events != NULL) && (count < event_cap))
    {
        events[count].type = CH585_LOCAL_EVENT_ENCODER_BUTTON;
        events[count].id = 0U;
        events[count].value = (int8_t)button_event;
        events[count].flags = 0U;
        count++;
    }

    return (int)count;
}

void ch585_fiveway_init(ch585_fiveway_t *fw,
                        uint8_t raw_mask,
                        uint8_t valid_mask,
                        uint8_t active_low_mask,
                        uint16_t debounce_ms,
                        uint32_t now_ms)
{
    uint8_t bit;
    uint8_t active_mask;

    if (fw == NULL)
    {
        return;
    }

    memset(fw, 0, sizeof(*fw));
    fw->valid_mask = (uint8_t)(valid_mask & CH585_FIVEWAY_ALL);
    fw->active_low_mask = (uint8_t)(active_low_mask & CH585_FIVEWAY_ALL);
    active_mask = (uint8_t)((raw_mask ^ fw->active_low_mask) & fw->valid_mask);

    for (bit = 0U; bit < 7U; bit++)
    {
        uint8_t pressed = (active_mask & (1U << bit)) ? 1U : 0U;
        ch585_debounce_button_init(&fw->bit[bit], pressed, debounce_ms,
                                   now_ms);
    }
    fw->stable_mask = active_mask;
    fw->initialized = 1U;
}

int ch585_fiveway_update(ch585_fiveway_t *fw,
                         uint8_t raw_mask,
                         uint32_t now_ms,
                         ch585_local_input_event_t events[],
                         uint8_t event_cap)
{
    uint8_t bit;
    uint8_t count = 0U;
    uint8_t active_mask;

    if ((fw == NULL) || (fw->initialized == 0U))
    {
        return 0;
    }

    active_mask = (uint8_t)((raw_mask ^ fw->active_low_mask) & fw->valid_mask);
    for (bit = 0U; bit < 7U; bit++)
    {
        uint8_t mask = (uint8_t)(1U << bit);
        int ev;

        if ((fw->valid_mask & mask) == 0U)
        {
            continue;
        }

        ev = ch585_debounce_button_update(&fw->bit[bit],
                                          (active_mask & mask) ? 1U : 0U,
                                          now_ms);
        if (ev != 0)
        {
            if (ev > 0)
            {
                fw->stable_mask = (uint8_t)(fw->stable_mask | mask);
            }
            else
            {
                fw->stable_mask = (uint8_t)(fw->stable_mask & ~mask);
            }
            if ((events != NULL) && (count < event_cap))
            {
                events[count].type = CH585_LOCAL_EVENT_FIVEWAY;
                events[count].id = bit;
                events[count].value = (int8_t)ev;
                events[count].flags = 0U;
                count++;
            }
        }
    }

    return (int)count;
}
