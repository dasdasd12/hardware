#include "h417_common.h"
#include "ch32h417_pioc_rgb1w.h"

enum
{
    H417_ITEM_WS2812 = 21
};

#define WS2812_LED_COUNT 77u
#define WS2812_TEST_LEVEL 0x08u
#define WS2812_COLOR_FRAMES 128u
#define WS2812_OFF_FRAMES 32u
#define WS2812_BREATH_HOLD_FRAMES 18u
#define WS2812_CHASE_HOLD_FRAMES 10u
#define WS2812_RAINBOW_HOLD_FRAMES 12u
#define WS2812_RAINBOW_BAND_WIDTH 7u
#define WS2812_PIOC_TIMEOUT_LOOPS 3000000u

#define WS2812_EFFECT_SOLID 0u
#define WS2812_EFFECT_BREATH 1u
#define WS2812_EFFECT_CHASE 2u
#define WS2812_EFFECT_RAINBOW_BAND 3u

#ifndef WS2812_EFFECT
#define WS2812_EFFECT WS2812_EFFECT_SOLID
#endif

enum
{
    WS2812_PHASE_FULL_RED = 20,
    WS2812_PHASE_FULL_GREEN = 21,
    WS2812_PHASE_FULL_BLUE = 22,
    WS2812_PHASE_FULL_OFF = 23,
    WS2812_PHASE_BREATH = 30,
    WS2812_PHASE_CHASE = 31,
    WS2812_PHASE_RAINBOW_BAND = 32,
    WS2812_PHASE_PIOC_ERROR = 0x200,
    WS2812_PHASE_PIOC_TIMEOUT = 0x2fe
};

static uint8_t ws_frame[WS2812_LED_COUNT * 3u];

static void ws_set_pixel(uint16_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if(index >= WS2812_LED_COUNT)
    {
        return;
    }

    ws_frame[(3u * index) + 0u] = green;
    ws_frame[(3u * index) + 1u] = red;
    ws_frame[(3u * index) + 2u] = blue;
}

static void ws_fill(uint8_t red, uint8_t green, uint8_t blue)
{
    uint16_t i;

    for(i = 0u; i < WS2812_LED_COUNT; ++i)
    {
        ws_set_pixel(i, red, green, blue);
    }
}

static void ws_record_status(uint8_t status)
{
    if(status == CH32H417_PIOC_RGB1W_OK)
    {
        h417_status_pass(H417_ITEM_WS2812);
    }
    else if(status == CH32H417_PIOC_RGB1W_ERR_TIMEOUT)
    {
        h417_status_phase(WS2812_PHASE_PIOC_TIMEOUT, H417_ITEM_WS2812);
        h417_status_fail(H417_ITEM_WS2812);
    }
    else
    {
        h417_status_phase(WS2812_PHASE_PIOC_ERROR + status, H417_ITEM_WS2812);
        h417_status_fail(H417_ITEM_WS2812);
    }
}

static void ws_send_frame(uint32_t phase)
{
    uint8_t status;

    h417_status_phase(phase, H417_ITEM_WS2812);
    status = ch32h417_pioc_rgb1w_send_ram(&ch32h417_pioc_rgb1w_pin_pf13,
                                          ws_frame,
                                          sizeof(ws_frame),
                                          WS2812_PIOC_TIMEOUT_LOOPS);
    ws_record_status(status);

    if(status != CH32H417_PIOC_RGB1W_OK)
    {
        ch32h417_pioc_rgb1w_init(&ch32h417_pioc_rgb1w_pin_pf13);
    }

    g_h417_status.cycle++;
}

static void ws_hold_frame(uint16_t frames, uint32_t phase)
{
    uint16_t i;

    for(i = 0u; i < frames; ++i)
    {
        ws_send_frame(phase);
    }
}

static void ws_repeat_color(uint8_t red, uint8_t green, uint8_t blue, uint16_t frames, uint32_t phase)
{
    ws_fill(red, green, blue);
    ws_hold_frame(frames, phase);
}

static void ws_effect_solid(void)
{
    while(1)
    {
        ws_repeat_color(WS2812_TEST_LEVEL, 0x00u, 0x00u, WS2812_COLOR_FRAMES, WS2812_PHASE_FULL_RED);
        ws_repeat_color(0x00u, WS2812_TEST_LEVEL, 0x00u, WS2812_COLOR_FRAMES, WS2812_PHASE_FULL_GREEN);
        ws_repeat_color(0x00u, 0x00u, WS2812_TEST_LEVEL, WS2812_COLOR_FRAMES, WS2812_PHASE_FULL_BLUE);
        ws_repeat_color(0x00u, 0x00u, 0x00u, WS2812_OFF_FRAMES, WS2812_PHASE_FULL_OFF);
    }
}

static void ws_effect_breath(void)
{
    uint8_t level;

    while(1)
    {
        for(level = 0u; level <= WS2812_TEST_LEVEL; ++level)
        {
            ws_fill(0x00u, 0x00u, level);
            ws_hold_frame(WS2812_BREATH_HOLD_FRAMES, WS2812_PHASE_BREATH);
        }

        for(level = WS2812_TEST_LEVEL - 1u; level > 0u; --level)
        {
            ws_fill(0x00u, 0x00u, level);
            ws_hold_frame(WS2812_BREATH_HOLD_FRAMES, WS2812_PHASE_BREATH);
        }
    }
}

static void ws_effect_chase(void)
{
    uint16_t head;

    while(1)
    {
        for(head = 0u; head < (WS2812_LED_COUNT + 3u); ++head)
        {
            ws_fill(0x00u, 0x00u, 0x00u);

            if(head < WS2812_LED_COUNT)
            {
                ws_set_pixel(head, 0x00u, WS2812_TEST_LEVEL, 0x00u);
            }
            if((head > 0u) && ((head - 1u) < WS2812_LED_COUNT))
            {
                ws_set_pixel(head - 1u, 0x00u, WS2812_TEST_LEVEL / 2u, 0x00u);
            }
            if((head > 1u) && ((head - 2u) < WS2812_LED_COUNT))
            {
                ws_set_pixel(head - 2u, 0x00u, WS2812_TEST_LEVEL / 4u, 0x00u);
            }

            ws_hold_frame(WS2812_CHASE_HOLD_FRAMES, WS2812_PHASE_CHASE);
        }
    }
}

static void ws_palette(uint8_t index, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    switch(index % 6u)
    {
    case 0u:
        *red = WS2812_TEST_LEVEL;
        *green = 0x00u;
        *blue = 0x00u;
        break;
    case 1u:
        *red = WS2812_TEST_LEVEL;
        *green = WS2812_TEST_LEVEL / 2u;
        *blue = 0x00u;
        break;
    case 2u:
        *red = 0x00u;
        *green = WS2812_TEST_LEVEL;
        *blue = 0x00u;
        break;
    case 3u:
        *red = 0x00u;
        *green = WS2812_TEST_LEVEL / 2u;
        *blue = WS2812_TEST_LEVEL;
        break;
    case 4u:
        *red = 0x00u;
        *green = 0x00u;
        *blue = WS2812_TEST_LEVEL;
        break;
    default:
        *red = WS2812_TEST_LEVEL / 2u;
        *green = 0x00u;
        *blue = WS2812_TEST_LEVEL;
        break;
    }
}

static void ws_effect_rainbow_band(void)
{
    uint16_t offset;
    uint16_t led;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t palette_index;

    while(1)
    {
        for(offset = 0u; offset < (WS2812_RAINBOW_BAND_WIDTH * 6u); ++offset)
        {
            for(led = 0u; led < WS2812_LED_COUNT; ++led)
            {
                palette_index = (uint8_t)(((led + offset) / WS2812_RAINBOW_BAND_WIDTH) % 6u);
                ws_palette(palette_index, &red, &green, &blue);
                ws_set_pixel(led, red, green, blue);
            }

            ws_hold_frame(WS2812_RAINBOW_HOLD_FRAMES, WS2812_PHASE_RAINBOW_BAND);
        }
    }
}

void h417_ws2812_run(void)
{
    ch32h417_pioc_rgb1w_init(&ch32h417_pioc_rgb1w_pin_pf13);
    h417_delay_cycles(100000u);

    switch(WS2812_EFFECT)
    {
    case WS2812_EFFECT_BREATH:
        ws_effect_breath();
        break;
    case WS2812_EFFECT_CHASE:
        ws_effect_chase();
        break;
    case WS2812_EFFECT_RAINBOW_BAND:
        ws_effect_rainbow_band();
        break;
    default:
        ws_effect_solid();
        break;
    }
}
