#include "rgb_status.h"

#ifndef V3F_ENABLE_RGB_STATUS
#define V3F_ENABLE_RGB_STATUS 0
#endif

#ifndef V3F_RGB_LED_COUNT
#define V3F_RGB_LED_COUNT 1
#endif

#ifndef V3F_RGB_EFFECT_COUNT
#define V3F_RGB_EFFECT_COUNT 3U
#endif

#ifndef V3F_RGB_UPDATE_TICKS
#define V3F_RGB_UPDATE_TICKS 128U
#endif

#if V3F_ENABLE_RGB_STATUS
#include "ch32h417_pioc_rgb1w.h"

static uint8_t s_rgb_grb[V3F_RGB_LED_COUNT * 3U];
static uint8_t s_rgb_enabled = 1U;
static uint8_t s_rgb_effect;
static uint8_t s_rgb_phase;
static uint16_t s_rgb_last_tick;
#endif

#if V3F_ENABLE_RGB_STATUS
static uint8_t rgb_triangle(uint8_t phase)
{
    return (phase < 128U) ? (uint8_t)(phase << 1) :
                            (uint8_t)((255U - phase) << 1);
}

static uint8_t rgb_scale(uint8_t value, uint8_t scale)
{
    return (uint8_t)(((uint16_t)value * (uint16_t)scale) >> 8);
}

static void rgb_hue(uint8_t hue, uint8_t brightness,
                    uint8_t *red, uint8_t *green, uint8_t *blue)
{
    uint8_t region = (uint8_t)(hue / 43U);
    uint8_t step = (uint8_t)((uint16_t)(hue - (uint8_t)(region * 43U)) * 6U);
    uint8_t rise = rgb_scale(step, brightness);
    uint8_t fall = rgb_scale((uint8_t)(255U - step), brightness);

    switch(region)
    {
    case 0:
        *red = brightness;
        *green = rise;
        *blue = 0U;
        break;
    case 1:
        *red = fall;
        *green = brightness;
        *blue = 0U;
        break;
    case 2:
        *red = 0U;
        *green = brightness;
        *blue = rise;
        break;
    case 3:
        *red = 0U;
        *green = fall;
        *blue = brightness;
        break;
    case 4:
        *red = rise;
        *green = 0U;
        *blue = brightness;
        break;
    default:
        *red = brightness;
        *green = 0U;
        *blue = fall;
        break;
    }
}

static void rgb_put(uint16_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    s_rgb_grb[(index * 3U) + 0U] = green;
    s_rgb_grb[(index * 3U) + 1U] = red;
    s_rgb_grb[(index * 3U) + 2U] = blue;
}

static void rgb_fill(uint8_t red, uint8_t green, uint8_t blue)
{
    uint16_t i;

    for(i = 0U; i < V3F_RGB_LED_COUNT; i++)
    {
        rgb_put(i, red, green, blue);
    }
}

static void rgb_render(void)
{
    uint16_t i;

    if(s_rgb_enabled == 0U)
    {
        rgb_fill(0U, 0U, 0U);
    }
    else if(s_rgb_effect == 0U)
    {
        rgb_fill(0U, 24U, 32U);
    }
    else if(s_rgb_effect == 1U)
    {
        uint8_t brightness = (uint8_t)(8U + (rgb_triangle(s_rgb_phase) >> 3));
        rgb_fill(0U, brightness, (uint8_t)(brightness + 8U));
    }
    else
    {
        for(i = 0U; i < V3F_RGB_LED_COUNT; i++)
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t hue = (uint8_t)(s_rgb_phase + (uint8_t)(i * 7U));

            rgb_hue(hue, 24U, &red, &green, &blue);
            rgb_put(i, red, green, blue);
        }
    }

    (void)ch32h417_pioc_rgb1w_send_ram(&ch32h417_pioc_rgb1w_pin_pf13,
                                       s_rgb_grb,
                                       (uint16_t)sizeof(s_rgb_grb),
                                       100000U);
}
#endif

void v3f_rgb_status_init(void)
{
#if V3F_ENABLE_RGB_STATUS
    ch32h417_pioc_rgb1w_init(&ch32h417_pioc_rgb1w_pin_pf13);
    rgb_render();
#endif
}

void v3f_rgb_status_red_once(void)
{
#if V3F_ENABLE_RGB_STATUS
    rgb_fill(32U, 0U, 0U);
    (void)ch32h417_pioc_rgb1w_send_ram(&ch32h417_pioc_rgb1w_pin_pf13,
                                       s_rgb_grb,
                                       (uint16_t)sizeof(s_rgb_grb),
                                       100000U);
#endif
}

void v3f_rgb_status_set_enabled(uint8_t enabled)
{
#if V3F_ENABLE_RGB_STATUS
    s_rgb_enabled = (enabled != 0U) ? 1U : 0U;
    rgb_render();
#else
    (void)enabled;
#endif
}

void v3f_rgb_status_toggle_enabled(void)
{
#if V3F_ENABLE_RGB_STATUS
    v3f_rgb_status_set_enabled((uint8_t)(s_rgb_enabled == 0U));
#endif
}

void v3f_rgb_status_next_effect(void)
{
#if V3F_ENABLE_RGB_STATUS
    s_rgb_effect++;
    if(s_rgb_effect >= V3F_RGB_EFFECT_COUNT)
    {
        s_rgb_effect = 0U;
    }
    s_rgb_enabled = 1U;
    rgb_render();
#endif
}

void v3f_rgb_status_task(uint16_t tick)
{
#if V3F_ENABLE_RGB_STATUS
    uint16_t elapsed = (uint16_t)(tick - s_rgb_last_tick);

    if(elapsed < V3F_RGB_UPDATE_TICKS)
    {
        return;
    }

    s_rgb_last_tick = tick;
    s_rgb_phase = (uint8_t)(s_rgb_phase + 3U);
    if((s_rgb_enabled == 0U) || (s_rgb_effect == 0U))
    {
        return;
    }

    rgb_render();
#else
    (void)tick;
#endif
}
