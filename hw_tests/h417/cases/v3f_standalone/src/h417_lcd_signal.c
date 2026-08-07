#include "h417_common.h"

enum
{
    H417_ITEM_LCD_ENABLE = 31,
    H417_ITEM_LCD_RED = 32,
    H417_ITEM_LCD_GREEN = 33,
    H417_ITEM_LCD_BLUE = 34,
    H417_ITEM_LCD_SYNC = 35
};

static const h417_pin_t lcd_enable_pins[] = {
    {GPIOA, GPIO_Pin_10, H417_ITEM_LCD_ENABLE},
    {GPIOA, GPIO_Pin_9, H417_ITEM_LCD_ENABLE},
};

static const h417_pin_t lcd_red_pins[] = {
    {GPIOA, GPIO_Pin_0, H417_ITEM_LCD_RED},
    {GPIOA, GPIO_Pin_2, H417_ITEM_LCD_RED},
    {GPIOA, GPIO_Pin_1, H417_ITEM_LCD_RED},
    {GPIOB, GPIO_Pin_0, H417_ITEM_LCD_RED},
    {GPIOA, GPIO_Pin_5, H417_ITEM_LCD_RED},
    {GPIOC, GPIO_Pin_0, H417_ITEM_LCD_RED},
    {GPIOA, GPIO_Pin_8, H417_ITEM_LCD_RED},
    {GPIOC, GPIO_Pin_4, H417_ITEM_LCD_RED},
};

static const h417_pin_t lcd_green_pins[] = {
    {GPIOE, GPIO_Pin_5, H417_ITEM_LCD_GREEN},
    {GPIOE, GPIO_Pin_6, H417_ITEM_LCD_GREEN},
    {GPIOA, GPIO_Pin_6, H417_ITEM_LCD_GREEN},
    {GPIOF, GPIO_Pin_4, H417_ITEM_LCD_GREEN},
    {GPIOC, GPIO_Pin_8, H417_ITEM_LCD_GREEN},
    {GPIOC, GPIO_Pin_1, H417_ITEM_LCD_GREEN},
    {GPIOC, GPIO_Pin_7, H417_ITEM_LCD_GREEN},
    {GPIOD, GPIO_Pin_3, H417_ITEM_LCD_GREEN},
};

static const h417_pin_t lcd_blue_pins[] = {
    {GPIOE, GPIO_Pin_4, H417_ITEM_LCD_BLUE},
    {GPIOC, GPIO_Pin_10, H417_ITEM_LCD_BLUE},
    {GPIOA, GPIO_Pin_3, H417_ITEM_LCD_BLUE},
    {GPIOD, GPIO_Pin_7, H417_ITEM_LCD_BLUE},
    {GPIOC, GPIO_Pin_11, H417_ITEM_LCD_BLUE},
    {GPIOD, GPIO_Pin_5, H417_ITEM_LCD_BLUE},
    {GPIOA, GPIO_Pin_14, H417_ITEM_LCD_BLUE},
    {GPIOD, GPIO_Pin_2, H417_ITEM_LCD_BLUE},
};

static const h417_pin_t lcd_sync_pins[] = {
    {GPIOF, GPIO_Pin_1, H417_ITEM_LCD_SYNC},
    {GPIOC, GPIO_Pin_6, H417_ITEM_LCD_SYNC},
    {GPIOA, GPIO_Pin_4, H417_ITEM_LCD_SYNC},
    {GPIOF, GPIO_Pin_10, H417_ITEM_LCD_SYNC},
};

static void init_group(const h417_pin_t *pins, unsigned int count)
{
    unsigned int i;
    for(i = 0; i < count; ++i)
    {
        h417_pin_output(&pins[i]);
        h417_pin_set(&pins[i], 0);
    }
}

static void drive_group(const h417_pin_t *pins, unsigned int count, uint8_t high)
{
    unsigned int i;
    for(i = 0; i < count; ++i)
    {
        h417_pin_set(&pins[i], high);
    }
}

static void walk_group(const h417_pin_t *pins, unsigned int count, uint32_t item)
{
    unsigned int i;
    drive_group(pins, count, 0);
    for(i = 0; i < count; ++i)
    {
        h417_status_phase(30, item + i);
        h417_pin_set(&pins[i], 1);
        h417_delay_cycles(350000u);
        h417_pin_set(&pins[i], 0);
        h417_delay_cycles(120000u);
    }
    h417_status_pass(item);
}

void h417_lcd_signal_run(void)
{
    init_group(lcd_enable_pins, sizeof(lcd_enable_pins) / sizeof(lcd_enable_pins[0]));
    init_group(lcd_red_pins, sizeof(lcd_red_pins) / sizeof(lcd_red_pins[0]));
    init_group(lcd_green_pins, sizeof(lcd_green_pins) / sizeof(lcd_green_pins[0]));
    init_group(lcd_blue_pins, sizeof(lcd_blue_pins) / sizeof(lcd_blue_pins[0]));
    init_group(lcd_sync_pins, sizeof(lcd_sync_pins) / sizeof(lcd_sync_pins[0]));

    while(1)
    {
        drive_group(lcd_enable_pins, sizeof(lcd_enable_pins) / sizeof(lcd_enable_pins[0]), 1);
        h417_status_pass(H417_ITEM_LCD_ENABLE);

        walk_group(lcd_red_pins, sizeof(lcd_red_pins) / sizeof(lcd_red_pins[0]), H417_ITEM_LCD_RED);
        walk_group(lcd_green_pins, sizeof(lcd_green_pins) / sizeof(lcd_green_pins[0]), H417_ITEM_LCD_GREEN);
        walk_group(lcd_blue_pins, sizeof(lcd_blue_pins) / sizeof(lcd_blue_pins[0]), H417_ITEM_LCD_BLUE);

        drive_group(lcd_sync_pins, sizeof(lcd_sync_pins) / sizeof(lcd_sync_pins[0]), 1);
        h417_delay_cycles(300000u);
        drive_group(lcd_sync_pins, sizeof(lcd_sync_pins) / sizeof(lcd_sync_pins[0]), 0);
        h417_status_pass(H417_ITEM_LCD_SYNC);
        g_h417_status.cycle++;
    }
}
