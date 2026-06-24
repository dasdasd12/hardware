#include "h417_lcd_control.h"
#include "h417_common.h"

void h417_lcd_control_gpio_init(void)
{
    GPIO_InitTypeDef init = {0};

    init.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &init);

    GPIO_ResetBits(GPIOA, GPIO_Pin_9 | GPIO_Pin_10);
}

void h417_lcd_disp_enable(uint8_t enable)
{
    if(enable)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_9);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_9);
    }
}

void h417_lcd_backlight_enable(uint8_t enable)
{
    if(enable)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_10);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_10);
    }
}
