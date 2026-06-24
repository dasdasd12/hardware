#ifndef CH32H417_LTDC_RGB_H
#define CH32H417_LTDC_RGB_H

#include <stdint.h>
#include "ch32h417_ltdc.h"
#include "ch32h417_rcc.h"

#define CH32H417_LCD_RGB_WIDTH          800u
#define CH32H417_LCD_RGB_HEIGHT         480u
#define CH32H417_LCD_RGB_HSYNC          8u
#define CH32H417_LCD_RGB_HBP            10u
#define CH32H417_LCD_RGB_HFP            50u
#define CH32H417_LCD_RGB_VSYNC          4u
#define CH32H417_LCD_RGB_VBP            20u
#define CH32H417_LCD_RGB_VFP            16u
#define CH32H417_LCD_RGB_PIXEL_CLOCK_HZ 26666666u

#define CH32H417_LTDC_RGB_OK            0
#define CH32H417_LTDC_RGB_ERR_PARAM     (-1)

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ch32h417_ltdc_rgb_color_t;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t hsync;
    uint16_t hbp;
    uint16_t hfp;
    uint16_t vsync;
    uint16_t vbp;
    uint16_t vfp;
    uint32_t pixel_clock_hz;
    uint32_t clock_divider;
    uint32_t hsync_polarity;
    uint32_t vsync_polarity;
    uint32_t de_polarity;
    uint32_t pixel_clock_polarity;
} ch32h417_ltdc_rgb_panel_t;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t offset_x;
    uint16_t offset_y;
    uint32_t pixel_format;
    uint32_t framebuffer;
    uint32_t line_pitch;
} ch32h417_ltdc_rgb_layer_t;

typedef struct
{
    uint32_t sscr;
    uint32_t bpcr;
    uint32_t awcr;
    uint32_t twcr;
    uint32_t gcr;
    uint32_t bccr;
    uint32_t cpsr;
    uint32_t cdsr;
    uint32_t isr;
    uint32_t rcc_cfgr2;
    uint32_t layer_whpcr;
    uint32_t layer_wvpcr;
    uint32_t layer_cfbar;
    uint32_t layer_cfblr;
    uint32_t layer_cfblnr;
} ch32h417_ltdc_rgb_snapshot_t;

extern const ch32h417_ltdc_rgb_panel_t ch32h417_ltdc_rgb_panel_800x480;

void ch32h417_lcd_rgb_control_init(void);
void ch32h417_lcd_rgb_disp_enable(uint8_t enable);
void ch32h417_lcd_rgb_backlight_enable(uint8_t enable);

void ch32h417_ltdc_rgb_gpio_init(void);
void ch32h417_ltdc_rgb_clock_init(const ch32h417_ltdc_rgb_panel_t *panel);
int ch32h417_ltdc_rgb_panel_init(const ch32h417_ltdc_rgb_panel_t *panel,
                                 const ch32h417_ltdc_rgb_color_t *background);
int ch32h417_ltdc_rgb_layer1_config(const ch32h417_ltdc_rgb_panel_t *panel,
                                    const ch32h417_ltdc_rgb_layer_t *layer);
void ch32h417_ltdc_rgb_layer1_enable(uint8_t enable);
void ch32h417_ltdc_rgb_layer2_enable(uint8_t enable);
void ch32h417_ltdc_rgb_enable(uint8_t enable);
void ch32h417_ltdc_rgb_reload(void);
void ch32h417_ltdc_rgb_set_background(const ch32h417_ltdc_rgb_color_t *color);
void ch32h417_ltdc_rgb_snapshot(ch32h417_ltdc_rgb_snapshot_t *snapshot);
uint32_t ch32h417_ltdc_rgb_bytes_per_pixel(uint32_t pixel_format);

#endif
