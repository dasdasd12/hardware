#include "board_init.h"

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "ch32h417_pwr.h"
#include "approval_mailbox.h"
#include "h417_board_config.h"

#define V3F_TRACE_BASE ((volatile uint32_t *)0x20178000u)
#define V3F_TRACE_MAGIC 0x56334650u
#define V3F_TRACE_VIO18_INITIAL 47u
#define V3F_TRACE_VIO18_CTLR    48u

#if H417_BOARD_HAS_CAPS_LOCK_LED
#define V3F_CAPS_LOCK_LED_PORT  GPIOB
#define V3F_CAPS_LOCK_LED_PIN   GPIO_Pin_7
#endif

#if H417_BOARD_HAS_RGB_POWER_ENABLE
#define V3F_RGB_POWER_EN_PORT   GPIOE
#define V3F_RGB_POWER_EN_PIN    GPIO_Pin_12
#endif

#ifndef V3F_WAKE_V5F
#define V3F_WAKE_V5F 0
#endif

#define V5F_START_ADDR 0x00010000u

volatile uint32_t WFE_MASK = 0;
volatile uint32_t WFE_WkupSource = 0;

#if H417_BOARD_HAS_CAPS_LOCK_LED
static uint8_t s_caps_lock_led_enabled;
#endif
#if H417_BOARD_HAS_RGB_POWER_ENABLE
static uint8_t s_rgb_power_enabled;
#endif

static void v3f_board_delay_cycles(uint32_t cycles)
{
    volatile uint32_t i;

    for(i = 0U; i < cycles; i++)
    {
        __asm volatile("nop");
    }
}

#if H417_BOARD_HAS_RGB_POWER_ENABLE
static void v3f_board_rgb_power_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOE, ENABLE);
    /* LED_EN is externally pulled up, but the software lighting default is
     * off. Preload PE12 low before enabling its output to shorten the VLED
     * power-on window during reset. PE12 high enables the Q2/Q1 power path. */
    GPIO_ResetBits(V3F_RGB_POWER_EN_PORT, V3F_RGB_POWER_EN_PIN);
    gpio.GPIO_Pin = V3F_RGB_POWER_EN_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Low;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(V3F_RGB_POWER_EN_PORT, &gpio);
    GPIO_ResetBits(V3F_RGB_POWER_EN_PORT, V3F_RGB_POWER_EN_PIN);
    s_rgb_power_enabled = 0U;
}
#endif

#if H417_BOARD_HAS_CAPS_LOCK_LED
static void v3f_board_caps_lock_led_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOB, ENABLE);
    /* The LED anode is tied to VDD, so PB7 is an active-low current sink.
     * Preload high, then use open-drain output: low lights the LED and high
     * releases PB7 without enabling an internal pull-down. */
    GPIO_SetBits(V3F_CAPS_LOCK_LED_PORT, V3F_CAPS_LOCK_LED_PIN);
    gpio.GPIO_Pin = V3F_CAPS_LOCK_LED_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Low;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(V3F_CAPS_LOCK_LED_PORT, &gpio);
    s_caps_lock_led_enabled = 0U;
}
#endif

void v3f_board_rgb_power_set(uint8_t enabled)
{
#if H417_BOARD_HAS_RGB_POWER_ENABLE
    enabled = (enabled != 0U) ? 1U : 0U;
    if(enabled == s_rgb_power_enabled)
    {
        return;
    }

    s_rgb_power_enabled = enabled;
    if(enabled != 0U)
    {
        GPIO_SetBits(V3F_RGB_POWER_EN_PORT, V3F_RGB_POWER_EN_PIN);
    }
    else
    {
        GPIO_ResetBits(V3F_RGB_POWER_EN_PORT, V3F_RGB_POWER_EN_PIN);
    }
#else
    (void)enabled;
#endif
}

void v3f_board_caps_lock_led_set(uint8_t enabled)
{
#if H417_BOARD_HAS_CAPS_LOCK_LED
    enabled = (enabled != 0U) ? 1U : 0U;
    if(enabled == s_caps_lock_led_enabled)
    {
        return;
    }

    s_caps_lock_led_enabled = enabled;
    if(enabled != 0U)
    {
        GPIO_ResetBits(V3F_CAPS_LOCK_LED_PORT, V3F_CAPS_LOCK_LED_PIN);
    }
    else
    {
        GPIO_SetBits(V3F_CAPS_LOCK_LED_PORT, V3F_CAPS_LOCK_LED_PIN);
    }
#else
    (void)enabled;
#endif
}

void v3f_board_init(void)
{
    SystemInit();
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);

    V3F_TRACE_BASE[V3F_TRACE_VIO18_INITIAL] =
        (uint32_t)PWR_GetVIO18InitialStatus();
    /* EVT reference code uses MODE3 for software-selected 3.3V VIO18. */
    PWR_VIO18ModeCfg(PWR_VIO18CFGMODE_SW);
    PWR_VIO18LevelCfg(PWR_VIO18Level_MODE3);
    v3f_board_delay_cycles(10000U);
    V3F_TRACE_BASE[V3F_TRACE_VIO18_CTLR] = PWR->CTLR;
#if H417_BOARD_HAS_RGB_POWER_ENABLE
    /* Match the hardware-tested sequence: claim PE12 after VIO is stable. */
    v3f_board_rgb_power_init();
#endif
#if H417_BOARD_HAS_CAPS_LOCK_LED
    v3f_board_caps_lock_led_init();
#endif

    V3F_TRACE_BASE[0] = V3F_TRACE_MAGIC;
    V3F_TRACE_BASE[1] = RCC->CFGR2;
    V3F_TRACE_BASE[2] = RCC->CTLR;
    V3F_TRACE_BASE[3] = RCC->HBPCENR;

    /*
     * Publish a valid inactive approval snapshot before V5F can start.
     * The mailbox is NOLOAD shared SRAM, so retained power-on contents must
     * never be interpreted as a live request.
     */
    v3f_approval_mailbox_init();
}

void v3f_board_start_v5f(void)
{
#if V3F_WAKE_V5F
    /*
     * V5F must already contain a valid image at V5F_START_ADDR.
     * A V3F-only erase/program operation can leave this region blank; waking
     * the secondary core in that state may destabilise shared chip resources,
     * including USB. Use the production dual-core flash flow when this option
     * is enabled.
     */
    /* Publish all V3F peripheral setup before V5F starts touching RCC/GPIO. */
    __asm volatile("fence iorw, iorw" ::: "memory");
    NVIC_WakeUp_V5F(V5F_START_ADDR);
#endif
}

void v3f_board_delay_1ms(void)
{
    v3f_board_delay_us(1000U);
}

void v3f_board_delay_us(uint32_t us)
{
    uint32_t ticks;

    if(us == 0U)
    {
        return;
    }

    ticks = (uint32_t)((((uint64_t)SystemCoreClock * (uint64_t)us) +
                        999999ULL) / 1000000ULL);
    if(ticks == 0U)
    {
        ticks = 1U;
    }

    SysTick0->ISR &= ~(1U << 0);
    SysTick0->CNT = 0U;
    SysTick0->CMP = ticks;
    SysTick0->CTLR = (1U << 2);
    SysTick0->CTLR |= (1U << 0);
    while((SysTick0->ISR & (1U << 0)) == 0U)
    {
    }
    SysTick0->CTLR &= ~(1U << 0);
}

void v3f_trace_set(uint32_t index, uint32_t value)
{
    if(index < 64U)
    {
        V3F_TRACE_BASE[index] = value;
    }
}

void v3f_trace_inc(uint32_t index)
{
    if(index < 64U)
    {
        V3F_TRACE_BASE[index]++;
    }
}
