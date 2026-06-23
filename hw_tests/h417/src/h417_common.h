#ifndef H417_COMMON_H
#define H417_COMMON_H

#include <stdint.h>
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"

#define H417_STATUS_MAGIC 0x48343137u

typedef struct
{
    volatile uint32_t magic;
    volatile uint32_t test_id;
    volatile uint32_t phase;
    volatile uint32_t cycle;
    volatile uint32_t last_item;
    volatile uint32_t pass_count;
    volatile uint32_t fail_count;
} h417_status_t;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t item_id;
} h417_pin_t;

extern volatile h417_status_t g_h417_status;

void h417_board_clock_gpio_init(void);
void h417_status_begin(uint32_t test_id);
void h417_status_pass(uint32_t item_id);
void h417_status_fail(uint32_t item_id);
void h417_status_phase(uint32_t phase, uint32_t item_id);
void h417_delay_cycles(uint32_t cycles);
void h417_pin_output(const h417_pin_t *pin);
void h417_pin_set(const h417_pin_t *pin, uint8_t high);
void h417_pin_toggle(const h417_pin_t *pin);

void h417_gpio_status_run(void);
void h417_ws2812_run(void);
void h417_lcd_signal_run(void);

#endif
