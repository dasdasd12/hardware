/*
 * Prototype magnetic-key decoding engine.
 *
 * The first version intentionally uses fixed-point integer math:
 * position_pm = 0..1000 means 0.0%..100.0% travel.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "keyboard_engine.h"

#define KEYBOARD_ENGINE_RELEASED_ADC      1000U
#define KEYBOARD_ENGINE_PRESSED_ADC       3000U
#define KEYBOARD_ENGINE_PRESS_PM          450U
#define KEYBOARD_ENGINE_RELEASE_PM        350U
#define KEYBOARD_ENGINE_FILTER_SHIFT      2U
#define KEYBOARD_ENGINE_FILTER_SCALE      256U

typedef struct
{
    keyboard_engine_key_state_t key[CH585_SCAN_TOTAL_KEYS];
    uint32_t filtered_q8[CH585_SCAN_TOTAL_KEYS];
    keyboard_engine_event_t event[KEYBOARD_ENGINE_EVENT_CAPACITY];
    uint16_t event_count;
    uint32_t frame_count;
    uint32_t total_events;
    uint8_t initialized;
} keyboard_engine_runtime_t;

static keyboard_engine_runtime_t g_engine;

static uint16_t keyboard_engine_clamp_position(int32_t value)
{
    if (value <= 0)
    {
        return 0U;
    }

    if (value >= (int32_t)KEYBOARD_ENGINE_POSITION_MAX)
    {
        return KEYBOARD_ENGINE_POSITION_MAX;
    }

    return (uint16_t)value;
}

static uint16_t keyboard_engine_position_from_adc(uint16_t adc)
{
    int32_t numerator;
    int32_t denominator;

    denominator = (int32_t)KEYBOARD_ENGINE_PRESSED_ADC - (int32_t)KEYBOARD_ENGINE_RELEASED_ADC;
    if (denominator == 0)
    {
        return 0U;
    }

    numerator = ((int32_t)adc - (int32_t)KEYBOARD_ENGINE_RELEASED_ADC) *
                (int32_t)KEYBOARD_ENGINE_POSITION_MAX;
    return keyboard_engine_clamp_position(numerator / denominator);
}

static void keyboard_engine_push_event(uint16_t key_id,
                                       const keyboard_engine_key_state_t *state)
{
    keyboard_engine_event_t *event;

    if ((state == NULL) || (g_engine.event_count >= KEYBOARD_ENGINE_EVENT_CAPACITY))
    {
        return;
    }

    event = &g_engine.event[g_engine.event_count++];
    event->key_id = key_id;
    event->is_down = state->is_down;
    event->raw_adc = state->raw_adc;
    event->filtered_adc = state->filtered_adc;
    event->position_pm = state->position_pm;
    g_engine.total_events++;
}

void keyboard_engine_init(void)
{
    uint16_t i;

    memset(&g_engine, 0, sizeof(g_engine));

    for (i = 0U; i < CH585_SCAN_TOTAL_KEYS; i++)
    {
        g_engine.key[i].raw_adc = KEYBOARD_ENGINE_RELEASED_ADC;
        g_engine.key[i].filtered_adc = KEYBOARD_ENGINE_RELEASED_ADC;
        g_engine.key[i].position_pm = 0U;
        g_engine.key[i].is_down = 0U;
        g_engine.filtered_q8[i] = (uint32_t)KEYBOARD_ENGINE_RELEASED_ADC *
                                  KEYBOARD_ENGINE_FILTER_SCALE;
    }

    g_engine.initialized = 1U;
}

void keyboard_engine_update(const uint16_t *raw_adc)
{
    uint16_t i;

    if (raw_adc == NULL)
    {
        return;
    }

    if (g_engine.initialized == 0U)
    {
        keyboard_engine_init();
    }

    g_engine.event_count = 0U;
    g_engine.frame_count++;

    for (i = 0U; i < CH585_SCAN_TOTAL_KEYS; i++)
    {
        keyboard_engine_key_state_t *state = &g_engine.key[i];
        int32_t target_q8 = (int32_t)raw_adc[i] * (int32_t)KEYBOARD_ENGINE_FILTER_SCALE;
        int32_t current_q8 = (int32_t)g_engine.filtered_q8[i];
        int32_t next_q8 = current_q8 + ((target_q8 - current_q8) >> KEYBOARD_ENGINE_FILTER_SHIFT);
        uint8_t was_down = state->is_down;

        if (next_q8 < 0)
        {
            next_q8 = 0;
        }

        g_engine.filtered_q8[i] = (uint32_t)next_q8;
        state->raw_adc = raw_adc[i];
        state->filtered_adc = (uint16_t)(g_engine.filtered_q8[i] / KEYBOARD_ENGINE_FILTER_SCALE);
        state->position_pm = keyboard_engine_position_from_adc(state->filtered_adc);

        if ((state->is_down == 0U) && (state->position_pm >= KEYBOARD_ENGINE_PRESS_PM))
        {
            state->is_down = 1U;
        }
        else if ((state->is_down != 0U) && (state->position_pm <= KEYBOARD_ENGINE_RELEASE_PM))
        {
            state->is_down = 0U;
        }

        if (state->is_down != was_down)
        {
            keyboard_engine_push_event(i, state);
        }
    }
}

uint16_t keyboard_engine_drain_events(keyboard_engine_event_t *events, uint16_t max_events)
{
    uint16_t count;

    if ((events == NULL) || (max_events == 0U))
    {
        g_engine.event_count = 0U;
        return 0U;
    }

    count = g_engine.event_count;
    if (count > max_events)
    {
        count = max_events;
    }

    memcpy(events, g_engine.event, (size_t)count * sizeof(events[0]));
    g_engine.event_count = 0U;
    return count;
}

const keyboard_engine_key_state_t *keyboard_engine_key_state(uint16_t key_id)
{
    if (key_id >= CH585_SCAN_TOTAL_KEYS)
    {
        return NULL;
    }

    return &g_engine.key[key_id];
}

uint32_t keyboard_engine_frame_count(void)
{
    return g_engine.frame_count;
}

uint32_t keyboard_engine_total_events(void)
{
    return g_engine.total_events;
}
