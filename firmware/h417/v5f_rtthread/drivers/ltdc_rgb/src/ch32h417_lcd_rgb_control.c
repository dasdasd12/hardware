#include "ch32h417_ltdc_rgb.h"
#include "ch32h417_gpio.h"
#include "ch32h417_tim.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} lcd_control_pin_t;

static const lcd_control_pin_t lcd_disp_pin = {GPIOA, GPIO_Pin_9};
static const lcd_control_pin_t lcd_backlight_pin = {GPIOA, GPIO_Pin_10};
static uint16_t lcd_backlight_on_pulse;

static void control_pin_write(const lcd_control_pin_t *pin, uint8_t enable)
{
    if(enable != 0u)
    {
        GPIO_SetBits(pin->port, pin->pin);
    }
    else
    {
        GPIO_ResetBits(pin->port, pin->pin);
    }
}

static void backlight_pwm_init(void)
{
    RCC_ClocksTypeDef clocks = {0};
    TIM_TimeBaseInitTypeDef timer = {0};
    TIM_OCInitTypeDef output = {0};
    GPIO_InitTypeDef gpio = {0};
    uint32_t period_counts;

    RCC_GetClocksFreq(&clocks);
    period_counts = clocks.HCLK_Frequency / CH32H417_LCD_BACKLIGHT_PWM_HZ;
    if(period_counts < 2u)
    {
        period_counts = 2u;
    }
    else if(period_counts > 65536u)
    {
        period_counts = 65536u;
    }

    lcd_backlight_on_pulse = (uint16_t)(
        (period_counts * CH32H417_LCD_BACKLIGHT_DUTY_PERCENT) / 100u);

    /* Keep CTRL low while TIM1_CH3 is prepared, so the backlight stays off. */
    GPIO_ResetBits(lcd_backlight_pin.port, lcd_backlight_pin.pin);

    TIM_Cmd(TIM1, DISABLE);
    TIM_DeInit(TIM1);
    timer.TIM_Period = (uint16_t)(period_counts - 1u);
    timer.TIM_Prescaler = 0u;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_RepetitionCounter = 0u;
    TIM_TimeBaseInit(TIM1, &timer);

    TIM_OCStructInit(&output);
    output.TIM_OCMode = TIM_OCMode_PWM1;
    output.TIM_OutputState = TIM_OutputState_Enable;
    output.TIM_Pulse = 0u;
    output.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM1, &output);
    TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_GenerateEvent(TIM1, TIM_EventSource_Update);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF1);
    gpio.GPIO_Pin = lcd_backlight_pin.pin;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(lcd_backlight_pin.port, &gpio);
}

void ch32h417_lcd_rgb_control_init(void)
{
    GPIO_InitTypeDef init = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOA |
                          RCC_HB2Periph_TIM1,
                          ENABLE);

    control_pin_write(&lcd_disp_pin, 0u);
    GPIO_ResetBits(lcd_backlight_pin.port, lcd_backlight_pin.pin);

    init.GPIO_Pin = lcd_disp_pin.pin;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &init);

    backlight_pwm_init();
}

void ch32h417_lcd_rgb_disp_enable(uint8_t enable)
{
    control_pin_write(&lcd_disp_pin, enable);
}

void ch32h417_lcd_rgb_backlight_enable(uint8_t enable)
{
    /*
     * TPS61169 converts CTRL duty cycle into its FB reference voltage. The
     * configured 50% high-time therefore gives about half the former LED
     * current; a steady low still invokes the chip's normal shutdown path.
     */
    TIM_SetCompare3(TIM1, (enable != 0u) ? lcd_backlight_on_pulse : 0u);
}
