#include "h417_common.h"
#include "ch32h417_pwr.h"

volatile h417_status_t g_h417_status;
volatile uint32_t g_h417_vio18_initial_status;
volatile uint32_t g_h417_vio18_ctlr_after_init;

void h417_board_clock_gpio_init(void)
{
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOA |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOC |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOE |
                          RCC_HB2Periph_GPIOF, ENABLE);

    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    g_h417_vio18_initial_status = (uint32_t)PWR_GetVIO18InitialStatus();

    /* EVT SEL_VIO18 uses MODE3 for software-selected 3.3V VIO18. */
    PWR_VIO18ModeCfg(PWR_VIO18CFGMODE_SW);
    PWR_VIO18LevelCfg(PWR_VIO18Level_MODE3);
    h417_delay_cycles(10000u);
    g_h417_vio18_ctlr_after_init = PWR->CTLR;
}

void h417_status_begin(uint32_t test_id)
{
    g_h417_status.magic = H417_STATUS_MAGIC;
    g_h417_status.test_id = test_id;
    g_h417_status.phase = 1;
    g_h417_status.cycle = 0;
    g_h417_status.last_item = 0;
    g_h417_status.pass_count = 0;
    g_h417_status.fail_count = 0;
}

void h417_status_pass(uint32_t item_id)
{
    g_h417_status.phase = 2;
    g_h417_status.last_item = item_id;
    g_h417_status.pass_count++;
}

void h417_status_fail(uint32_t item_id)
{
    g_h417_status.phase = 3;
    g_h417_status.last_item = item_id;
    g_h417_status.fail_count++;
}

void h417_status_phase(uint32_t phase, uint32_t item_id)
{
    g_h417_status.phase = phase;
    g_h417_status.last_item = item_id;
}

void h417_delay_cycles(uint32_t cycles)
{
    volatile uint32_t i;
    for(i = 0; i < cycles; ++i)
    {
        __asm__ volatile("nop");
    }
}

void h417_pin_output(const h417_pin_t *pin)
{
    GPIO_InitTypeDef init = {0};
    init.GPIO_Pin = pin->pin;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(pin->port, &init);
}

void h417_pin_set(const h417_pin_t *pin, uint8_t high)
{
    if(high)
    {
        GPIO_SetBits(pin->port, pin->pin);
    }
    else
    {
        GPIO_ResetBits(pin->port, pin->pin);
    }
}

void h417_pin_toggle(const h417_pin_t *pin)
{
    if(GPIO_ReadOutputDataBit(pin->port, pin->pin))
    {
        GPIO_ResetBits(pin->port, pin->pin);
    }
    else
    {
        GPIO_SetBits(pin->port, pin->pin);
    }
}
