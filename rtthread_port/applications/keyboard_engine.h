/*
 * Prototype magnetic-key decoding engine.
 *
 * Input:  raw ADC values merged by ch585_spi_scan.
 * Output: filtered position and key press/release events.
 */

#ifndef KEYBOARD_ENGINE_H__
#define KEYBOARD_ENGINE_H__

#include <stdint.h>

#include "ch585_spi_scan.h"

#define KEYBOARD_ENGINE_POSITION_MAX      1000U
#define KEYBOARD_ENGINE_EVENT_CAPACITY    16U

typedef struct
{
    uint16_t key_id;
    uint8_t is_down;
    uint16_t raw_adc;
    uint16_t filtered_adc;
    uint16_t position_pm;
} keyboard_engine_event_t;

typedef struct
{
    uint16_t raw_adc;
    uint16_t filtered_adc;
    uint16_t position_pm;
    uint8_t is_down;
} keyboard_engine_key_state_t;

void keyboard_engine_init(void);
void keyboard_engine_update(const uint16_t *raw_adc);
uint16_t keyboard_engine_drain_events(keyboard_engine_event_t *events, uint16_t max_events);
const keyboard_engine_key_state_t *keyboard_engine_key_state(uint16_t key_id);
uint32_t keyboard_engine_frame_count(void);
uint32_t keyboard_engine_total_events(void);

#endif /* KEYBOARD_ENGINE_H__ */
