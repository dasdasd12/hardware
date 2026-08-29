#include "v5f_flash_animation.h"

#include <rtthread.h>

#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "v5f_product_h4v1.h"
#include "v5f_display.h"

#define V5F_FLASH_ANIMATION_SELECT_PORT       GPIOE
#define V5F_FLASH_ANIMATION_SELECT_PIN        GPIO_Pin_14
#define V5F_FLASH_ANIMATION_DEBOUNCE_MS        60u

static uint8_t s_initialized;
static uint8_t s_candidate_requested;
static uint8_t s_stable_requested;
static uint8_t s_failure_latched;
static rt_tick_t s_candidate_since;
static const char *s_last_failure;

static uint8_t read_animation_request(void)
{
    return (uint8_t)(
        GPIO_ReadInputDataBit(V5F_FLASH_ANIMATION_SELECT_PORT,
                              V5F_FLASH_ANIMATION_SELECT_PIN) == Bit_RESET);
}

void v5f_flash_animation_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOE, ENABLE);
    gpio.GPIO_Pin = V5F_FLASH_ANIMATION_SELECT_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Low;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(V5F_FLASH_ANIMATION_SELECT_PORT, &gpio);

    /* Fail safe to the normal UI until one physical level lasts for 60 ms. */
    s_stable_requested = 0u;
    s_candidate_requested = read_animation_request();
    s_candidate_since = rt_tick_get();
    s_failure_latched = 0u;
    s_last_failure = RT_NULL;
    s_initialized = 1u;
}

void v5f_flash_animation_poll(void)
{
    uint8_t requested;
    rt_tick_t now;
    rt_tick_t debounce_ticks;

    if(s_initialized == 0u)
    {
        return;
    }

    now = rt_tick_get();
    requested = read_animation_request();
    if(requested != s_candidate_requested)
    {
        s_candidate_requested = requested;
        s_candidate_since = now;
        return;
    }

    debounce_ticks = rt_tick_from_millisecond(
        (rt_int32_t)V5F_FLASH_ANIMATION_DEBOUNCE_MS);
    if(debounce_ticks == 0u)
    {
        debounce_ticks = 1u;
    }
    if((s_stable_requested != s_candidate_requested) &&
       ((rt_tick_t)(now - s_candidate_since) >= debounce_ticks))
    {
        s_stable_requested = s_candidate_requested;
        if(s_stable_requested == 0u)
        {
            /* A full 60 ms UI selection arms one clean retry. */
            s_failure_latched = 0u;
            s_last_failure = RT_NULL;
        }
    }
}

uint8_t v5f_flash_animation_requested(void)
{
    return (uint8_t)((s_initialized != 0u) &&
                     (s_stable_requested != 0u) &&
                     (s_failure_latched == 0u));
}

uint8_t v5f_flash_animation_should_continue(void)
{
    v5f_flash_animation_poll();
    return v5f_flash_animation_requested();
}

uint8_t v5f_flash_animation_failure_latched(void)
{
    return s_failure_latched;
}

const char *v5f_flash_animation_last_failure(void)
{
    return (s_last_failure != RT_NULL) ? s_last_failure : "unknown";
}

int v5f_flash_animation_run_blocking(void)
{
    int player_result;
    int display_result;

    if(v5f_flash_animation_requested() == 0u)
    {
        return V5F_FLASH_ANIMATION_OK;
    }

    display_result = v5f_display_deactivate();
    if(display_result != V5F_DISPLAY_OK)
    {
        s_failure_latched = 1u;
        s_last_failure = "display_deactivate";
        (void)v5f_display_activate_ui();
        return V5F_FLASH_ANIMATION_ERR_DISPLAY;
    }

    s_last_failure = RT_NULL;
    player_result = v5f_product_h4v1_run();

    /* Always return Layer1 to the product UI, including player failures. */
    display_result = v5f_display_activate_ui();
    if(display_result != V5F_DISPLAY_OK)
    {
        s_failure_latched = 1u;
        s_last_failure = "display_activate";
        return V5F_FLASH_ANIMATION_ERR_DISPLAY;
    }

    if(player_result != V5F_FLASH_ANIMATION_OK)
    {
        s_failure_latched = 1u;
        s_last_failure = v5f_product_h4v1_last_failure();
    }

    return player_result;
}
