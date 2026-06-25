/*
 * Portable magnetic-key engine for the CH585 side.
 */

#include "magnetic_key_engine.h"

#include <stddef.h>
#include <string.h>

#define MAG_KEY_DEFAULT_RELEASED_ADC       200U
#define MAG_KEY_DEFAULT_PRESSED_ADC        850U
#define MAG_KEY_DEFAULT_PRESS_PM           450U
#define MAG_KEY_DEFAULT_RELEASE_PM         350U
#define MAG_KEY_DEFAULT_RT_PRESS_DELTA_PM  60U
#define MAG_KEY_DEFAULT_RT_RELEASE_DELTA_PM 40U
#define MAG_KEY_DEFAULT_FILTER_SHIFT       2U

static uint16_t mag_key_clamp_pm(int32_t value)
{
    if (value <= 0)
    {
        return 0U;
    }

    if (value >= (int32_t)MAG_KEY_POSITION_MAX_PM)
    {
        return MAG_KEY_POSITION_MAX_PM;
    }

    return (uint16_t)value;
}

void mag_key_default_config(mag_key_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    cfg->released_adc = MAG_KEY_DEFAULT_RELEASED_ADC;
    cfg->pressed_adc = MAG_KEY_DEFAULT_PRESSED_ADC;
    cfg->press_pm = MAG_KEY_DEFAULT_PRESS_PM;
    cfg->release_pm = MAG_KEY_DEFAULT_RELEASE_PM;
    cfg->rt_press_delta_pm = MAG_KEY_DEFAULT_RT_PRESS_DELTA_PM;
    cfg->rt_release_delta_pm = MAG_KEY_DEFAULT_RT_RELEASE_DELTA_PM;
    cfg->filter_shift = MAG_KEY_DEFAULT_FILTER_SHIFT;
    cfg->mode = MAG_KEY_MODE_RAPID_TRIGGER;
}

static void mag_key_sanitize_config(mag_key_config_t *cfg)
{
    if (cfg->press_pm > MAG_KEY_POSITION_MAX_PM)
    {
        cfg->press_pm = MAG_KEY_POSITION_MAX_PM;
    }

    if (cfg->release_pm > MAG_KEY_POSITION_MAX_PM)
    {
        cfg->release_pm = MAG_KEY_POSITION_MAX_PM;
    }

    if (cfg->rt_press_delta_pm > MAG_KEY_POSITION_MAX_PM)
    {
        cfg->rt_press_delta_pm = MAG_KEY_POSITION_MAX_PM;
    }

    if (cfg->rt_release_delta_pm > MAG_KEY_POSITION_MAX_PM)
    {
        cfg->rt_release_delta_pm = MAG_KEY_POSITION_MAX_PM;
    }

    if (cfg->filter_shift > 8U)
    {
        cfg->filter_shift = 8U;
    }

    if ((cfg->mode != MAG_KEY_MODE_STATIC) &&
        (cfg->mode != MAG_KEY_MODE_RAPID_TRIGGER))
    {
        cfg->mode = MAG_KEY_MODE_STATIC;
    }
}

uint16_t mag_key_position_from_adc(uint16_t adc, const mag_key_config_t *cfg)
{
    int32_t numerator;
    int32_t denominator;

    if ((cfg == NULL) || (cfg->pressed_adc == cfg->released_adc))
    {
        return 0U;
    }

    if (cfg->pressed_adc > cfg->released_adc)
    {
        numerator = ((int32_t)adc - (int32_t)cfg->released_adc) *
                    (int32_t)MAG_KEY_POSITION_MAX_PM;
        denominator = (int32_t)cfg->pressed_adc -
                      (int32_t)cfg->released_adc;
    }
    else
    {
        numerator = ((int32_t)cfg->released_adc - (int32_t)adc) *
                    (int32_t)MAG_KEY_POSITION_MAX_PM;
        denominator = (int32_t)cfg->released_adc -
                      (int32_t)cfg->pressed_adc;
    }

    return mag_key_clamp_pm(numerator / denominator);
}

int mag_key_engine_init(mag_key_engine_t *engine,
                        uint16_t key_count,
                        const mag_key_config_t *default_cfg)
{
    mag_key_config_t cfg;
    uint16_t key;

    if ((engine == NULL) || (key_count == 0U) ||
        (key_count > MAG_KEY_ENGINE_MAX_KEYS))
    {
        return MAG_KEY_STATUS_PARAM;
    }

    if (default_cfg == NULL)
    {
        mag_key_default_config(&cfg);
    }
    else
    {
        cfg = *default_cfg;
        mag_key_sanitize_config(&cfg);
    }

    memset(engine, 0, sizeof(*engine));
    engine->key_count = key_count;

    for (key = 0U; key < key_count; key++)
    {
        engine->cfg[key] = cfg;
        engine->state[key].raw_adc = cfg.released_adc;
        engine->state[key].filtered_adc = cfg.released_adc;
        engine->state[key].position_pm = 0U;
        engine->filtered_q8[key] = (uint32_t)cfg.released_adc *
                                   MAG_KEY_FILTER_SCALE;
        engine->valley_pm[key] = 0U;
        engine->peak_pm[key] = 0U;
    }

    engine->initialized = 1U;
    return MAG_KEY_STATUS_OK;
}

int mag_key_engine_set_key_config(mag_key_engine_t *engine,
                                  uint16_t key_id,
                                  const mag_key_config_t *cfg)
{
    mag_key_config_t next;

    if ((engine == NULL) || (cfg == NULL) ||
        (engine->initialized == 0U) || (key_id >= engine->key_count))
    {
        return MAG_KEY_STATUS_PARAM;
    }

    next = *cfg;
    mag_key_sanitize_config(&next);
    engine->cfg[key_id] = next;
    return MAG_KEY_STATUS_OK;
}

int mag_key_engine_set_global_config(mag_key_engine_t *engine,
                                     const mag_key_config_t *cfg)
{
    uint16_t key;

    if ((engine == NULL) || (cfg == NULL) || (engine->initialized == 0U))
    {
        return MAG_KEY_STATUS_PARAM;
    }

    for (key = 0U; key < engine->key_count; key++)
    {
        int rc = mag_key_engine_set_key_config(engine, key, cfg);
        if (rc != MAG_KEY_STATUS_OK)
        {
            return rc;
        }
    }

    return MAG_KEY_STATUS_OK;
}

static void mag_key_push_event(mag_key_engine_t *engine, uint16_t key_id)
{
    const mag_key_state_t *state = &engine->state[key_id];
    mag_key_event_t *event;

    engine->total_events++;

    if (engine->event_count >= MAG_KEY_ENGINE_EVENT_CAPACITY)
    {
        engine->dropped_events++;
        return;
    }

    event = &engine->event[engine->event_count++];
    event->key_id = key_id;
    event->is_down = state->is_down;
    event->raw_adc = state->raw_adc;
    event->filtered_adc = state->filtered_adc;
    event->position_pm = state->position_pm;
    event->frame_id = engine->frame_count;
}

static uint16_t mag_key_filter_adc(mag_key_engine_t *engine,
                                   uint16_t key_id,
                                   uint16_t raw)
{
    const mag_key_config_t *cfg = &engine->cfg[key_id];
    int32_t target_q8 = (int32_t)raw * (int32_t)MAG_KEY_FILTER_SCALE;
    int32_t current_q8 = (int32_t)engine->filtered_q8[key_id];
    int32_t next_q8;

    next_q8 = current_q8 + ((target_q8 - current_q8) >> cfg->filter_shift);
    if (next_q8 < 0)
    {
        next_q8 = 0;
    }

    engine->filtered_q8[key_id] = (uint32_t)next_q8;
    return (uint16_t)(engine->filtered_q8[key_id] / MAG_KEY_FILTER_SCALE);
}

static uint8_t mag_key_static_down_next(const mag_key_config_t *cfg,
                                        uint8_t was_down,
                                        uint16_t position_pm)
{
    if (was_down == 0U)
    {
        return (uint8_t)(position_pm >= cfg->press_pm);
    }

    if (position_pm <= cfg->release_pm)
    {
        return 0U;
    }

    return 1U;
}

static uint8_t mag_key_rt_down_next(mag_key_engine_t *engine,
                                    uint16_t key_id,
                                    uint16_t position_pm)
{
    const mag_key_config_t *cfg = &engine->cfg[key_id];
    mag_key_state_t *state = &engine->state[key_id];
    uint16_t valley = engine->valley_pm[key_id];
    uint16_t peak = engine->peak_pm[key_id];

    if (state->is_down == 0U)
    {
        if ((position_pm <= cfg->release_pm) || (position_pm < valley))
        {
            valley = position_pm;
            engine->valley_pm[key_id] = valley;
        }

        if ((position_pm >= cfg->press_pm) &&
            ((uint32_t)position_pm >=
             (uint32_t)valley + (uint32_t)cfg->rt_press_delta_pm))
        {
            engine->peak_pm[key_id] = position_pm;
            return 1U;
        }

        return 0U;
    }

    if (position_pm > peak)
    {
        peak = position_pm;
        engine->peak_pm[key_id] = peak;
    }

    if ((position_pm <= cfg->release_pm) ||
        ((uint32_t)position_pm + (uint32_t)cfg->rt_release_delta_pm <=
         (uint32_t)peak))
    {
        engine->valley_pm[key_id] = position_pm;
        return 0U;
    }

    return 1U;
}

int mag_key_engine_update(mag_key_engine_t *engine, const uint16_t *raw_adc)
{
    uint16_t key;

    if ((engine == NULL) || (raw_adc == NULL) ||
        (engine->initialized == 0U))
    {
        return MAG_KEY_STATUS_PARAM;
    }

    engine->event_count = 0U;
    engine->frame_count++;

    for (key = 0U; key < engine->key_count; key++)
    {
        mag_key_state_t *state = &engine->state[key];
        const mag_key_config_t *cfg = &engine->cfg[key];
        uint8_t was_down = state->is_down;
        uint8_t next_down;

        state->raw_adc = raw_adc[key];
        state->filtered_adc = mag_key_filter_adc(engine, key, raw_adc[key]);
        state->position_pm = mag_key_position_from_adc(state->filtered_adc,
                                                       cfg);

        if (cfg->mode == MAG_KEY_MODE_RAPID_TRIGGER)
        {
            next_down = mag_key_rt_down_next(engine, key, state->position_pm);
        }
        else
        {
            next_down = mag_key_static_down_next(cfg,
                                                 state->is_down,
                                                 state->position_pm);
        }

        state->is_down = next_down;
        if (state->is_down != was_down)
        {
            mag_key_push_event(engine, key);
        }
    }

    return MAG_KEY_STATUS_OK;
}

uint16_t mag_key_engine_drain_events(mag_key_engine_t *engine,
                                     mag_key_event_t *events,
                                     uint16_t max_events)
{
    uint16_t count;

    if ((engine == NULL) || (engine->initialized == 0U))
    {
        return 0U;
    }

    if ((events == NULL) || (max_events == 0U))
    {
        engine->event_count = 0U;
        return 0U;
    }

    count = engine->event_count;
    if (count > max_events)
    {
        count = max_events;
    }

    memcpy(events, engine->event, (size_t)count * sizeof(events[0]));
    engine->event_count = 0U;
    return count;
}

const mag_key_state_t *mag_key_engine_state(const mag_key_engine_t *engine,
                                            uint16_t key_id)
{
    if ((engine == NULL) || (engine->initialized == 0U) ||
        (key_id >= engine->key_count))
    {
        return NULL;
    }

    return &engine->state[key_id];
}
