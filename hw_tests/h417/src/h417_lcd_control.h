#ifndef H417_LCD_CONTROL_H
#define H417_LCD_CONTROL_H

#include <stdint.h>

void h417_lcd_control_gpio_init(void);
void h417_lcd_disp_enable(uint8_t enable);
void h417_lcd_backlight_enable(uint8_t enable);

#endif
