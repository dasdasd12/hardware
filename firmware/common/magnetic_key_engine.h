/*
 * Portable magnetic-key engine for the H417 side.
 *
 * Input is calibrated raw ADC values for the full keyboard. Output is per-key
 * travel position plus press/release events. The engine is standalone so it can
 * be integrated after the SPI frame format is stable.
 */

#ifndef MAGNETIC_KEY_ENGINE_H__
#define MAGNETIC_KEY_ENGINE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAG_KEY_ENGINE_MAX_KEYS        128U
#define MAG_KEY_ENGINE_EVENT_CAPACITY  64U
#define MAG_KEY_POSITION_MAX_PM        1000U
#define MAG_KEY_FILTER_SCALE           256U

typedef enum
{
    MAG_KEY_STATUS_OK = 0,
    MAG_KEY_STATUS_PARAM = -1
} mag_key_status_t;

typedef enum
{
    MAG_KEY_MODE_STATIC = 0,
    MAG_KEY_MODE_RAPID_TRIGGER = 1
} mag_key_mode_t;

typedef struct
{
    uint16_t released_adc;
    uint16_t pressed_adc;
    uint16_t press_pm;
    uint16_t release_pm;
    uint16_t rt_press_delta_pm;
    uint16_t rt_release_delta_pm;
    uint8_t filter_shift;
    uint8_t mode;
} mag_key_config_t;

typedef struct
{
    uint16_t raw_adc;
    uint16_t filtered_adc;
    uint16_t position_pm;
    uint8_t is_down;
} mag_key_state_t;

typedef struct
{
    uint16_t key_id;
    uint8_t is_down;
    uint16_t raw_adc;
    uint16_t filtered_adc;
    uint16_t position_pm;
    uint32_t frame_id;
} mag_key_event_t;

typedef struct
{
    uint16_t key_count;
    mag_key_config_t cfg[MAG_KEY_ENGINE_MAX_KEYS];
    mag_key_state_t state[MAG_KEY_ENGINE_MAX_KEYS];
    uint32_t filtered_q8[MAG_KEY_ENGINE_MAX_KEYS];
    uint16_t valley_pm[MAG_KEY_ENGINE_MAX_KEYS];
    uint16_t peak_pm[MAG_KEY_ENGINE_MAX_KEYS];
    mag_key_event_t event[MAG_KEY_ENGINE_EVENT_CAPACITY];
    uint16_t event_count;
    uint32_t frame_count;
    uint32_t total_events;
    uint32_t dropped_events;
    uint8_t initialized;
} mag_key_engine_t;

void mag_key_default_config(mag_key_config_t *cfg);
int mag_key_engine_init(mag_key_engine_t *engine,
                        uint16_t key_count,
                        const mag_key_config_t *default_cfg);
int mag_key_engine_set_key_config(mag_key_engine_t *engine,
                                  uint16_t key_id,
                                  const mag_key_config_t *cfg);
int mag_key_engine_set_global_config(mag_key_engine_t *engine,
                                     const mag_key_config_t *cfg);
int mag_key_engine_update(mag_key_engine_t *engine, const uint16_t *raw_adc);
uint16_t mag_key_engine_drain_events(mag_key_engine_t *engine,
                                     mag_key_event_t *events,
                                     uint16_t max_events);
const mag_key_state_t *mag_key_engine_state(const mag_key_engine_t *engine,
                                            uint16_t key_id);
uint16_t mag_key_position_from_adc(uint16_t adc, const mag_key_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* MAGNETIC_KEY_ENGINE_H__ */
