#include "h417_common.h"

enum
{
    H417_ITEM_WS2812 = 21
};

static const h417_pin_t ws_pin = {GPIOF, GPIO_Pin_13, H417_ITEM_WS2812};

#define WS2812_LED_COUNT 77u
#define WS2812_TEST_LEVEL 0x08u
#define WS2812_TEST_CORE_HZ 70000000u
#define WS2812_NS_TO_CYCLES(ns) \
    ((uint32_t)((((uint64_t)WS2812_TEST_CORE_HZ) * (uint64_t)(ns) + 999999999ull) / 1000000000ull))

enum
{
    WS2812_STARTUP_PROBE_PULSES = 64u,
    WS2812_STARTUP_PROBE_DELAY = 20000u,
    WS2812_T0H_CYCLES = WS2812_NS_TO_CYCLES(300u),
    WS2812_T1H_CYCLES = WS2812_NS_TO_CYCLES(800u),
    WS2812_BIT_CYCLES = WS2812_NS_TO_CYCLES(1250u),
    WS2812_RESET_CYCLES = WS2812_NS_TO_CYCLES(90000u),
    WS2812_HOLD_CYCLES = WS2812_TEST_CORE_HZ / 2u
};

static inline uint32_t ws_cycle_now(void)
{
    uint32_t cycles;
    __asm__ volatile("csrr %0, mcycle" : "=r"(cycles));
    return cycles;
}

static inline void ws_wait_from(uint32_t start, uint32_t cycles)
{
    while((uint32_t)(ws_cycle_now() - start) < cycles)
    {
    }
}

static inline void ws_wait(uint32_t cycles)
{
    ws_wait_from(ws_cycle_now(), cycles);
}

static inline void ws_drive_high(void)
{
    ws_pin.port->BSHR = ws_pin.pin;
}

static inline void ws_drive_low(void)
{
    ws_pin.port->BCR = ws_pin.pin;
}

static void ws_send_bit(uint8_t bit)
{
    uint32_t start = ws_cycle_now();
    uint32_t high_cycles = bit ? WS2812_T1H_CYCLES : WS2812_T0H_CYCLES;

    ws_drive_high();
    ws_wait_from(start, high_cycles);
    ws_drive_low();
    ws_wait_from(start, WS2812_BIT_CYCLES);
}

static void ws_send_byte(uint8_t value)
{
    uint8_t mask;
    for(mask = 0x80u; mask != 0u; mask >>= 1)
    {
        ws_send_bit((value & mask) != 0u);
    }
}

static void ws_send_color(uint8_t red, uint8_t green, uint8_t blue, uint16_t count)
{
    uint16_t i;
    for(i = 0; i < count; ++i)
    {
        ws_send_byte(green);
        ws_send_byte(red);
        ws_send_byte(blue);
    }
    ws_drive_low();
    ws_wait(WS2812_RESET_CYCLES);
}

static void ws_startup_probe(void)
{
    uint32_t i;

    h417_status_phase(10, H417_ITEM_WS2812);
    for(i = 0; i < WS2812_STARTUP_PROBE_PULSES; ++i)
    {
        ws_drive_high();
        h417_delay_cycles(WS2812_STARTUP_PROBE_DELAY);
        ws_drive_low();
        h417_delay_cycles(WS2812_STARTUP_PROBE_DELAY);
    }

    h417_delay_cycles(WS2812_STARTUP_PROBE_DELAY);
}

void h417_ws2812_run(void)
{
    h417_pin_output(&ws_pin);
    ws_drive_low();
    ws_startup_probe();

    while(1)
    {
        h417_status_phase(20, H417_ITEM_WS2812);
        ws_send_color(WS2812_TEST_LEVEL, 0x00u, 0x00u, WS2812_LED_COUNT);
        ws_wait(WS2812_HOLD_CYCLES);
        ws_send_color(0x00u, WS2812_TEST_LEVEL, 0x00u, WS2812_LED_COUNT);
        ws_wait(WS2812_HOLD_CYCLES);
        ws_send_color(0x00u, 0x00u, WS2812_TEST_LEVEL, WS2812_LED_COUNT);
        ws_wait(WS2812_HOLD_CYCLES);
        ws_send_color(0x00u, 0x00u, 0x00u, WS2812_LED_COUNT);
        h417_status_pass(H417_ITEM_WS2812);
        g_h417_status.cycle++;
        ws_wait(WS2812_HOLD_CYCLES);
    }
}
