#include "h417_common.h"

enum
{
    H417_ITEM_WS2812 = 21
};

static const h417_pin_t ws_pin = {GPIOF, GPIO_Pin_13, H417_ITEM_WS2812};

static void ws_wait(uint32_t cycles)
{
    h417_delay_cycles(cycles);
}

static void ws_send_bit(uint8_t bit)
{
    if(bit)
    {
        h417_pin_set(&ws_pin, 1);
        ws_wait(46u);
        h417_pin_set(&ws_pin, 0);
        ws_wait(24u);
    }
    else
    {
        h417_pin_set(&ws_pin, 1);
        ws_wait(18u);
        h417_pin_set(&ws_pin, 0);
        ws_wait(52u);
    }
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
    h417_pin_set(&ws_pin, 0);
    h417_delay_cycles(9000u);
}

void h417_ws2812_run(void)
{
    h417_pin_output(&ws_pin);
    h417_pin_set(&ws_pin, 0);

    while(1)
    {
        h417_status_phase(20, H417_ITEM_WS2812);
        ws_send_color(0x20u, 0x00u, 0x00u, 32u);
        h417_delay_cycles(2000000u);
        ws_send_color(0x00u, 0x20u, 0x00u, 32u);
        h417_delay_cycles(2000000u);
        ws_send_color(0x00u, 0x00u, 0x20u, 32u);
        h417_delay_cycles(2000000u);
        ws_send_color(0x00u, 0x00u, 0x00u, 32u);
        h417_status_pass(H417_ITEM_WS2812);
        g_h417_status.cycle++;
        h417_delay_cycles(2000000u);
    }
}
