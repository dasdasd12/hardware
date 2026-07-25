#include "rgb_status.h"

#ifndef V3F_ENABLE_RGB_STATUS
#define V3F_ENABLE_RGB_STATUS 0
#endif

#ifndef V3F_RGB_LED_COUNT
#define V3F_RGB_LED_COUNT 1
#endif

#define V3F_RGB_EFFECT_STATIC        0U
#define V3F_RGB_EFFECT_SOLID_BREATH  1U
#define V3F_RGB_EFFECT_RAINBOW_FLOW  2U
#define V3F_RGB_EFFECT_BREATH_FLOW   3U

#ifndef V3F_RGB_EFFECT_COUNT
#define V3F_RGB_EFFECT_COUNT 4U
#endif

#ifndef V3F_RGB_DEFAULT_EFFECT
#define V3F_RGB_DEFAULT_EFFECT V3F_RGB_EFFECT_BREATH_FLOW
#endif

#ifndef V3F_RGB_UPDATE_TICKS
#define V3F_RGB_UPDATE_TICKS 32U
#endif

#ifndef V3F_RGB_PHASE_STEP
#define V3F_RGB_PHASE_STEP 2U
#endif

#ifndef V3F_RGB_MAX_BRIGHTNESS
#define V3F_RGB_MAX_BRIGHTNESS 255U
#endif

#ifndef V3F_RGB_DEFAULT_ENABLED
#define V3F_RGB_DEFAULT_ENABLED 0U
#endif

#ifndef V3F_RGB_BOOT_RETRY_TICKS
#define V3F_RGB_BOOT_RETRY_TICKS 256U
#endif

#ifndef V3F_RGB_ASYNC_TIMEOUT_POLLS
#define V3F_RGB_ASYNC_TIMEOUT_POLLS 256U
#endif

#if V3F_ENABLE_RGB_STATUS
#include "ch32h417_pioc_rgb1w.h"

static uint8_t s_rgb_grb[V3F_RGB_LED_COUNT * 3U];
static uint8_t s_rgb_enabled = (V3F_RGB_DEFAULT_ENABLED != 0U) ? 1U : 0U;
static uint8_t s_rgb_effect = V3F_RGB_DEFAULT_EFFECT;
static uint8_t s_rgb_phase;
static uint16_t s_rgb_last_tick;
static uint16_t s_rgb_boot_retry_ticks;
static uint32_t s_rgb_render_count;
static uint32_t s_rgb_error_count;
static uint8_t s_rgb_last_result;
static uint8_t s_rgb_in_flight;
static uint8_t s_rgb_render_pending;
static uint8_t s_rgb_pending_red_once;
static uint16_t s_rgb_poll_count;
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

static void rgb_build_frame(uint8_t red_once)
{
    uint16_t i;

    if(red_once != 0U)
    {
        rgb_fill(32U, 0U, 0U);
    }
    else if(s_rgb_enabled == 0U)
    {
        rgb_fill(0U, 0U, 0U);
    }
    else if(s_rgb_effect == 0U)
    {
        rgb_fill(0U, V3F_RGB_MAX_BRIGHTNESS, V3F_RGB_MAX_BRIGHTNESS);
    }
    else if(s_rgb_effect == V3F_RGB_EFFECT_SOLID_BREATH)
    {
        uint8_t brightness = rgb_triangle(s_rgb_phase);
        rgb_fill(0U, brightness, brightness);
    }
    else if(s_rgb_effect == V3F_RGB_EFFECT_RAINBOW_FLOW)
    {
        for(i = 0U; i < V3F_RGB_LED_COUNT; i++)
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t hue = (uint8_t)(s_rgb_phase + (uint8_t)(i * 7U));

            rgb_hue(hue, V3F_RGB_MAX_BRIGHTNESS, &red, &green, &blue);
            rgb_put(i, red, green, blue);
        }
    }
    else
    {
        uint8_t breath = rgb_triangle(s_rgb_phase);

        for(i = 0U; i < V3F_RGB_LED_COUNT; i++)
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t hue = (uint8_t)((uint8_t)(s_rgb_phase * 2U) +
                                    (uint8_t)(i * 5U));

            rgb_hue(hue, breath, &red, &green, &blue);
            rgb_put(i, red, green, blue);
        }
    }
}

static void rgb_record_result(uint8_t result)
{
    s_rgb_last_result = result;
    if(result != CH32H417_PIOC_RGB1W_OK)
    {
        s_rgb_error_count++;
    }
}

static void rgb_request_render(uint8_t red_once)
{
    s_rgb_render_pending = 1U;
    s_rgb_pending_red_once = (red_once != 0U) ? 1U : 0U;
}

static void rgb_service(void)
{
    uint8_t result;

    if(s_rgb_in_flight != 0U)
    {
        if(ch32h417_pioc_rgb1w_poll(&result) != 0U)
        {
            s_rgb_in_flight = 0U;
            s_rgb_poll_count = 0U;
            rgb_record_result(result);
        }
        else
        {
            if(s_rgb_poll_count < V3F_RGB_ASYNC_TIMEOUT_POLLS)
            {
                s_rgb_poll_count++;
            }
            if(s_rgb_poll_count >= V3F_RGB_ASYNC_TIMEOUT_POLLS)
            {
                ch32h417_pioc_rgb1w_halt();
                s_rgb_in_flight = 0U;
                s_rgb_poll_count = 0U;
                rgb_record_result(CH32H417_PIOC_RGB1W_ERR_TIMEOUT);
            }
        }
    }

    if((s_rgb_in_flight != 0U) || (s_rgb_render_pending == 0U))
    {
        return;
    }

    rgb_build_frame(s_rgb_pending_red_once);
    s_rgb_render_pending = 0U;
    s_rgb_pending_red_once = 0U;
    result =
        ch32h417_pioc_rgb1w_start_ram(&ch32h417_pioc_rgb1w_pin_pf13,
                                      s_rgb_grb,
                                      (uint16_t)sizeof(s_rgb_grb));
    s_rgb_render_count++;
    if(result == CH32H417_PIOC_RGB1W_OK)
    {
        s_rgb_in_flight = 1U;
        s_rgb_poll_count = 0U;
    }
    else
    {
        rgb_record_result(result);
    }
}
#endif

void v3f_rgb_status_init(void)
{
#if V3F_ENABLE_RGB_STATUS
    if(s_rgb_effect >= V3F_RGB_EFFECT_COUNT)
    {
        s_rgb_effect = V3F_RGB_EFFECT_STATIC;
    }
    ch32h417_pioc_rgb1w_init(&ch32h417_pioc_rgb1w_pin_pf13);
    s_rgb_boot_retry_ticks = V3F_RGB_BOOT_RETRY_TICKS;
    rgb_request_render(0U);
    rgb_service();
#endif
}

void v3f_rgb_status_red_once(void)
{
#if V3F_ENABLE_RGB_STATUS
    rgb_request_render(1U);
    rgb_service();
#endif
}

void v3f_rgb_status_set_enabled(uint8_t enabled)
{
#if V3F_ENABLE_RGB_STATUS
    s_rgb_enabled = (enabled != 0U) ? 1U : 0U;
    rgb_request_render(0U);
    rgb_service();
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
    rgb_request_render(0U);
    rgb_service();
#endif
}

void v3f_rgb_status_task(uint16_t tick)
{
#if V3F_ENABLE_RGB_STATUS
    uint16_t elapsed = (uint16_t)(tick - s_rgb_last_tick);
    uint8_t boot_retry = (s_rgb_boot_retry_ticks != 0U) ? 1U : 0U;

    rgb_service();
    if(elapsed < V3F_RGB_UPDATE_TICKS)
    {
        return;
    }

    s_rgb_last_tick = tick;
    s_rgb_phase = (uint8_t)(s_rgb_phase + V3F_RGB_PHASE_STEP);
    if(s_rgb_boot_retry_ticks != 0U)
    {
        s_rgb_boot_retry_ticks--;
    }
    if((s_rgb_enabled == 0U) || (s_rgb_effect == 0U))
    {
        if(boot_retry != 0U)
        {
            rgb_request_render(0U);
        }
    }
    else
    {
        rgb_request_render(0U);
    }
    rgb_service();
#else
    (void)tick;
#endif
}

uint8_t v3f_rgb_status_effect(void)
{
#if V3F_ENABLE_RGB_STATUS
    return s_rgb_effect;
#else
    return 0U;
#endif
}

uint8_t v3f_rgb_status_last_result(void)
{
#if V3F_ENABLE_RGB_STATUS
    return s_rgb_last_result;
#else
    return 0U;
#endif
}

uint32_t v3f_rgb_status_render_count(void)
{
#if V3F_ENABLE_RGB_STATUS
    return s_rgb_render_count;
#else
    return 0U;
#endif
}

uint32_t v3f_rgb_status_error_count(void)
{
#if V3F_ENABLE_RGB_STATUS
    return s_rgb_error_count;
#else
    return 0U;
#endif
}
