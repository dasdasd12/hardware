#include "ch32h417_ltdc_rgb.h"
#include "ch32h417_gpio.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} lcd_control_pin_t;

static const lcd_control_pin_t lcd_disp_pin = {GPIOA, GPIO_Pin_9};
static const lcd_control_pin_t lcd_backlight_pin = {GPIOA, GPIO_Pin_10};

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

void ch32h417_lcd_rgb_control_init(void)
{
    GPIO_InitTypeDef init = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA, ENABLE);

    init.GPIO_Pin = lcd_disp_pin.pin | lcd_backlight_pin.pin;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &init);

    control_pin_write(&lcd_disp_pin, 0u);
    control_pin_write(&lcd_backlight_pin, 0u);
}

void ch32h417_lcd_rgb_disp_enable(uint8_t enable)
{
    control_pin_write(&lcd_disp_pin, enable);
}

void ch32h417_lcd_rgb_backlight_enable(uint8_t enable)
{
    /*
     * PA10 is only the board-level LCD_CTRL/backlight enable input. The LCD
     * module's 24 V LED rail is generated outside this driver.
     */
    control_pin_write(&lcd_backlight_pin, enable);
}
