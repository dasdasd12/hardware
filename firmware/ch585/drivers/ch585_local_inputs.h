/*
 * Debounced local inputs for the two CH585 controllers.
 *
 * U2 owns the screen/five-way interaction pins and U3 owns the EC11 encoder.
 * This file contains only logic; GPIO reads are supplied by the caller.
 */

#ifndef CH585_LOCAL_INPUTS_H__
#define CH585_LOCAL_INPUTS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH585_LOCAL_INPUT_EVENT_CAP 8U

#define CH585_FIVEWAY_UP      (1U << 0)
#define CH585_FIVEWAY_DOWN    (1U << 1)
#define CH585_FIVEWAY_LEFT    (1U << 2)
#define CH585_FIVEWAY_RIGHT   (1U << 3)
/* RKJXT Push-Com/common net. It is not an independent center key signal. */
#define CH585_FIVEWAY_COM     (1U << 4)
#define CH585_FIVEWAY_CENTER  CH585_FIVEWAY_COM
#define CH585_FIVEWAY_A       (1U << 5)
#define CH585_FIVEWAY_B       (1U << 6)
#define CH585_FIVEWAY_ALL     0x7FU

typedef enum
{
    CH585_LOCAL_EVENT_NONE = 0,
    CH585_LOCAL_EVENT_ENCODER_DELTA = 1,
    CH585_LOCAL_EVENT_ENCODER_BUTTON = 2,
    CH585_LOCAL_EVENT_FIVEWAY = 3
} ch585_local_event_type_t;

typedef struct
{
    uint8_t type;
    uint8_t id;
    int8_t value;
    uint8_t flags;
} ch585_local_input_event_t;

typedef struct
{
    uint8_t stable_pressed;
    uint8_t candidate_pressed;
    uint8_t initialized;
    uint16_t debounce_ms;
    uint32_t candidate_since_ms;
} ch585_debounce_button_t;

typedef struct
{
    uint8_t last_ab;
    int8_t step_accum;
    uint8_t steps_per_detent;
    uint8_t reverse;
    uint8_t initialized;
    ch585_debounce_button_t button;
} ch585_ec11_t;

typedef struct
{
    uint8_t valid_mask;
    uint8_t active_low_mask;
    ch585_debounce_button_t bit[7];
    uint8_t stable_mask;
    uint8_t initialized;
} ch585_fiveway_t;

void ch585_debounce_button_init(ch585_debounce_button_t *button,
                                uint8_t pressed,
                                uint16_t debounce_ms,
                                uint32_t now_ms);
int ch585_debounce_button_update(ch585_debounce_button_t *button,
                                 uint8_t pressed,
                                 uint32_t now_ms);

void ch585_ec11_init(ch585_ec11_t *enc,
                     uint8_t ab,
                     uint8_t button_pressed,
                     uint16_t debounce_ms,
                     uint32_t now_ms);
int ch585_ec11_update(ch585_ec11_t *enc,
                      uint8_t ab,
                      uint8_t button_pressed,
                      uint32_t now_ms,
                      ch585_local_input_event_t events[],
                      uint8_t event_cap);

void ch585_fiveway_init(ch585_fiveway_t *fw,
                        uint8_t raw_mask,
                        uint8_t valid_mask,
                        uint8_t active_low_mask,
                        uint16_t debounce_ms,
                        uint32_t now_ms);
int ch585_fiveway_update(ch585_fiveway_t *fw,
                         uint8_t raw_mask,
                         uint32_t now_ms,
                         ch585_local_input_event_t events[],
                         uint8_t event_cap);

#ifdef __cplusplus
}
#endif

#endif /* CH585_LOCAL_INPUTS_H__ */
