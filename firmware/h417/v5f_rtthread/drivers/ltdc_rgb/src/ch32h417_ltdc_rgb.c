#include "ch32h417_ltdc_rgb.h"
#include "ch32h417_gpio.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t source;
    uint8_t alternate_function;
} ltdc_pin_af_t;

const ch32h417_ltdc_rgb_panel_t ch32h417_ltdc_rgb_panel_800x480 = {
    CH32H417_LCD_RGB_WIDTH,
    CH32H417_LCD_RGB_HEIGHT,
    CH32H417_LCD_RGB_HSYNC,
    CH32H417_LCD_RGB_HBP,
    CH32H417_LCD_RGB_HFP,
    CH32H417_LCD_RGB_VSYNC,
    CH32H417_LCD_RGB_VBP,
    CH32H417_LCD_RGB_VFP,
    CH32H417_LCD_RGB_PIXEL_CLOCK_HZ,
    RCC_LTDCClockSource_Div15,
    LTDC_HSPolarity_AH,
    LTDC_VSPolarity_AL,
    LTDC_DEPolarity_AH,
    LTDC_PCPolarity_IIPC,
};

static const ltdc_pin_af_t lcd_ltdc_pins[] = {
    {GPIOA, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF14},    /* R0 */
    {GPIOA, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF14},    /* R1 */
    {GPIOA, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF14},    /* R2 */
    {GPIOB, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF9},     /* R3 */
    {GPIOA, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF14},    /* R4 */
    {GPIOC, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF14},    /* R5 */
    {GPIOA, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF14},    /* R6 */
    {GPIOC, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF14},    /* R7 */
    {GPIOE, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF14},    /* G0 */
    {GPIOE, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF14},    /* G1 */
    {GPIOA, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF14},    /* G2 */
    {GPIOF, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF9},     /* G3 */
    {GPIOC, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF14},    /* G4 */
    {GPIOC, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF14},    /* G5 */
    {GPIOC, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF14},    /* G6 */
    {GPIOD, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF14},    /* G7 */
    {GPIOE, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF14},    /* B0 */
    {GPIOC, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF10},  /* B1 */
    {GPIOA, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF9},     /* B2 */
    {GPIOD, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF14},    /* B3 */
    {GPIOC, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF14},  /* B4 */
    {GPIOD, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF14},    /* B5 */
    {GPIOA, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF14},  /* B6 */
    {GPIOD, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF9},     /* B7 */
    {GPIOF, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF14},    /* CLK */
    {GPIOC, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF14},    /* HS */
    {GPIOA, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF14},    /* VS */
    {GPIOF, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF14},  /* DE */
};

static uint8_t panel_valid(const ch32h417_ltdc_rgb_panel_t *panel)
{
    return (uint8_t)((panel != 0) &&
                     (panel->width != 0u) &&
                     (panel->height != 0u) &&
                     (panel->hsync != 0u) &&
                     (panel->vsync != 0u));
}

static void ltdc_rgb_clut_write_gap(void)
{
    volatile uint32_t delay;

    for(delay = 0u; delay < 4000u; delay++)
    {
        __asm volatile ("nop");
    }
}

static void ltdc_rgb_wait_vertical_blank(void)
{
    uint32_t guard = 2000000u;

    while((LTDC_GetCDStatus(LTDC_CD_VDES) != RESET) && (guard != 0u))
    {
        guard--;
    }
}

static void ltdc_rgb_wait_vdes_state(uint8_t active)
{
    uint32_t guard = 8000000u;

    while((((LTDC_GetCDStatus(LTDC_CD_VDES) != RESET) ? 1u : 0u) != active) &&
          (guard != 0u))
    {
        guard--;
    }
}

static void ltdc_rgb_wait_next_vertical_blank(void)
{
    ltdc_rgb_wait_vdes_state(1u);
    ltdc_rgb_wait_vdes_state(0u);
}

static uint8_t ltdc_rgb_layer1_begin_clut_update(void)
{
    uint8_t layer_was_enabled;

    /*
     * H417 CLUT writes are not coherent while the same layer is scanning.
     * Stop Layer1 and wait for the stop to cross a frame boundary before
     * rewriting the table.
     */
    layer_was_enabled = (uint8_t)((LTDC_Layer1->CR & LTDC_CR_LEN) != 0u);
    if(layer_was_enabled != 0u)
    {
        ltdc_rgb_wait_vertical_blank();
        LTDC_LayerCmd(LTDC_Layer1, DISABLE);
        LTDC_ReloadConfig(LTDC_IMReload);
        ltdc_rgb_wait_next_vertical_blank();
    }

    LTDC_CLUTCmd(LTDC_Layer1, ENABLE);
    return layer_was_enabled;
}

static void ltdc_rgb_layer1_end_clut_update(uint8_t layer_was_enabled)
{
    LTDC_CLUTCmd(LTDC_Layer1, ENABLE);
    if(layer_was_enabled != 0u)
    {
        LTDC_LayerCmd(LTDC_Layer1, ENABLE);
    }
    LTDC_ReloadConfig(LTDC_IMReload);
}

void ch32h417_ltdc_rgb_gpio_init(void)
{
    GPIO_InitTypeDef init = {0};
    unsigned int i;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOA |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOC |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOE |
                          RCC_HB2Periph_GPIOF,
                          ENABLE);

    init.GPIO_Speed = GPIO_Speed_Very_High;
    init.GPIO_Mode = GPIO_Mode_AF_PP;

    for(i = 0u; i < (sizeof(lcd_ltdc_pins) / sizeof(lcd_ltdc_pins[0])); ++i)
    {
        init.GPIO_Pin = lcd_ltdc_pins[i].pin;
        GPIO_PinAFConfig(lcd_ltdc_pins[i].port,
                         lcd_ltdc_pins[i].source,
                         lcd_ltdc_pins[i].alternate_function);
        GPIO_Init(lcd_ltdc_pins[i].port, &init);
    }
}

void ch32h417_ltdc_rgb_clock_init(const ch32h417_ltdc_rgb_panel_t *panel)
{
    uint32_t divider = RCC_LTDCClockSource_Div15;

    if(panel != 0)
    {
        divider = panel->clock_divider;
    }

    RCC_LTDCCLKConfig(RCC_LTDCClockSource_PLL);
    RCC_LTDCClockSourceDivConfig(divider);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_LTDC, ENABLE);
}

int ch32h417_ltdc_rgb_panel_init(const ch32h417_ltdc_rgb_panel_t *panel,
                                 const ch32h417_ltdc_rgb_color_t *background)
{
    LTDC_InitTypeDef init;

    if(!panel_valid(panel))
    {
        return CH32H417_LTDC_RGB_ERR_PARAM;
    }

    ch32h417_ltdc_rgb_gpio_init();
    ch32h417_ltdc_rgb_clock_init(panel);
    LTDC_DeInit();

    LTDC_StructInit(&init);
    init.LTDC_HSPolarity = panel->hsync_polarity;
    init.LTDC_VSPolarity = panel->vsync_polarity;
    init.LTDC_DEPolarity = panel->de_polarity;
    init.LTDC_PCPolarity = panel->pixel_clock_polarity;
    init.LTDC_HorizontalSync = panel->hsync - 1u;
    init.LTDC_VerticalSync = panel->vsync - 1u;
    init.LTDC_AccumulatedHBP = panel->hsync + panel->hbp - 1u;
    init.LTDC_AccumulatedVBP = panel->vsync + panel->vbp - 1u;
    init.LTDC_AccumulatedActiveW = panel->hsync + panel->hbp + panel->width - 1u;
    init.LTDC_AccumulatedActiveH = panel->vsync + panel->vbp + panel->height - 1u;
    init.LTDC_TotalWidth = panel->hsync + panel->hbp + panel->width + panel->hfp - 1u;
    init.LTDC_TotalHeigh = panel->vsync + panel->vbp + panel->height + panel->vfp - 1u;
    if(background != 0)
    {
        init.LTDC_BackgroundRedValue = background->red;
        init.LTDC_BackgroundGreenValue = background->green;
        init.LTDC_BackgroundBlueValue = background->blue;
    }
    LTDC_Init(&init);

    return CH32H417_LTDC_RGB_OK;
}

uint32_t ch32h417_ltdc_rgb_bytes_per_pixel(uint32_t pixel_format)
{
    switch(pixel_format)
    {
        case LTDC_Pixelformat_ARGB8888:
            return 4u;
        case LTDC_Pixelformat_RGB888:
            return 3u;
        case LTDC_Pixelformat_RGB565:
        case LTDC_Pixelformat_ARGB1555:
        case LTDC_Pixelformat_ARGB4444:
        case LTDC_Pixelformat_AL88:
            return 2u;
        case LTDC_Pixelformat_L8:
        case LTDC_Pixelformat_AL44:
            return 1u;
        default:
            return 0u;
    }
}

int ch32h417_ltdc_rgb_layer1_config(const ch32h417_ltdc_rgb_panel_t *panel,
                                    const ch32h417_ltdc_rgb_layer_t *layer)
{
    LTDC_Layer_InitTypeDef init;
    uint32_t bytes_per_pixel;
    uint32_t line_pitch;
    uint32_t active_start_x;
    uint32_t active_start_y;

    if(!panel_valid(panel) ||
       (layer == 0) ||
       (layer->width == 0u) ||
       (layer->height == 0u) ||
       (layer->framebuffer == 0u))
    {
        return CH32H417_LTDC_RGB_ERR_PARAM;
    }
    if(((uint32_t)layer->offset_x + layer->width) > panel->width ||
       ((uint32_t)layer->offset_y + layer->height) > panel->height)
    {
        return CH32H417_LTDC_RGB_ERR_PARAM;
    }

    bytes_per_pixel = ch32h417_ltdc_rgb_bytes_per_pixel(layer->pixel_format);
    if(bytes_per_pixel == 0u)
    {
        return CH32H417_LTDC_RGB_ERR_PARAM;
    }

    line_pitch = layer->line_pitch;
    if(line_pitch == 0u)
    {
        line_pitch = (uint32_t)layer->width * bytes_per_pixel;
    }

    active_start_x = (uint32_t)panel->hsync + panel->hbp;
    active_start_y = (uint32_t)panel->vsync + panel->vbp;

    LTDC_LayerStructInit(&init);
    init.LTDC_HorizontalStart = active_start_x + layer->offset_x;
    init.LTDC_HorizontalStop = init.LTDC_HorizontalStart + layer->width - 1u;
    init.LTDC_VerticalStart = active_start_y + layer->offset_y;
    init.LTDC_VerticalStop = init.LTDC_VerticalStart + layer->height - 1u;
    init.LTDC_PixelFormat = layer->pixel_format;
    init.LTDC_ConstantAlpha = 0xFFu;
    init.LTDC_DefaultColorBlue = 0x00u;
    init.LTDC_DefaultColorGreen = 0x00u;
    init.LTDC_DefaultColorRed = 0x00u;
    init.LTDC_DefaultColorAlpha = 0x00u;
    init.LTDC_BlendingFactor_1 = LTDC_BlendingFactor1_CA;
    init.LTDC_BlendingFactor_2 = LTDC_BlendingFactor2_CA;
    init.LTDC_CFBStartAdress = layer->framebuffer;
    init.LTDC_CFBPitch = line_pitch;
    /* Match WCH's LTDC_LayerSize helper: CFBLL is active line bytes plus 3. */
    init.LTDC_CFBLineLength = line_pitch + 3u;
    init.LTDC_CFBLineNumber = layer->height;

    LTDC_LayerInit(LTDC_Layer1, &init);
    return CH32H417_LTDC_RGB_OK;
}

int ch32h417_ltdc_rgb_start_layer1(const ch32h417_ltdc_rgb_panel_t *panel,
                                   const ch32h417_ltdc_rgb_layer_t *layer,
                                   const ch32h417_ltdc_rgb_color_t *background)
{
    int result;

    result = ch32h417_ltdc_rgb_panel_init(panel, background);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }

    result = ch32h417_ltdc_rgb_layer1_config(panel, layer);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }

    ch32h417_ltdc_rgb_layer1_enable(1u);
    ch32h417_ltdc_rgb_layer2_enable(0u);
    ch32h417_ltdc_rgb_reload();
    ch32h417_ltdc_rgb_enable(1u);
    ch32h417_ltdc_rgb_reload();

    return CH32H417_LTDC_RGB_OK;
}

void ch32h417_ltdc_rgb_layer1_enable(uint8_t enable)
{
    LTDC_LayerCmd(LTDC_Layer1, (enable != 0u) ? ENABLE : DISABLE);
}

void ch32h417_ltdc_rgb_layer2_enable(uint8_t enable)
{
    LTDC_LayerCmd(LTDC_Layer2, (enable != 0u) ? ENABLE : DISABLE);
}

void ch32h417_ltdc_rgb_layer1_clut_enable(uint8_t enable)
{
    LTDC_CLUTCmd(LTDC_Layer1, (enable != 0u) ? ENABLE : DISABLE);
}

void ch32h417_ltdc_rgb_layer1_load_grayscale_clut(void)
{
    LTDC_CLUT_InitTypeDef init;
    uint16_t index;
    uint8_t layer_was_enabled;

    layer_was_enabled = ltdc_rgb_layer1_begin_clut_update();
    for(index = 0u; index < 256u; index++)
    {
        LTDC_CLUTStructInit(&init);
        init.LTDC_CLUTAdress = (uint32_t)index;
        init.LTDC_RedValue = (uint32_t)index;
        init.LTDC_GreenValue = (uint32_t)index;
        init.LTDC_BlueValue = (uint32_t)index;
        LTDC_CLUTInit(LTDC_Layer1, &init);
        ltdc_rgb_clut_write_gap();
    }

    ltdc_rgb_layer1_end_clut_update(layer_was_enabled);
}

int ch32h417_ltdc_rgb_layer1_load_clut_rgb888(const uint8_t *rgb888,
                                              uint16_t entry_count)
{
    LTDC_CLUT_InitTypeDef init;
    uint16_t index;
    uint8_t layer_was_enabled;

    if((rgb888 == 0) || (entry_count == 0u) ||
       (entry_count > CH32H417_LTDC_RGB_CLUT_ENTRIES))
    {
        return CH32H417_LTDC_RGB_ERR_PARAM;
    }

    layer_was_enabled = ltdc_rgb_layer1_begin_clut_update();
    for(index = 0u; index < CH32H417_LTDC_RGB_CLUT_ENTRIES; index++)
    {
        uint8_t red = 0u;
        uint8_t green = 0u;
        uint8_t blue = 0u;

        if(index < entry_count)
        {
            red = rgb888[(uint32_t)index * 3u + 0u];
            green = rgb888[(uint32_t)index * 3u + 1u];
            blue = rgb888[(uint32_t)index * 3u + 2u];
        }

        LTDC_CLUTStructInit(&init);
        init.LTDC_CLUTAdress = (uint32_t)index;
        init.LTDC_RedValue = red;
        init.LTDC_GreenValue = green;
        init.LTDC_BlueValue = blue;
        LTDC_CLUTInit(LTDC_Layer1, &init);
        ltdc_rgb_clut_write_gap();
    }

    ltdc_rgb_layer1_end_clut_update(layer_was_enabled);
    return CH32H417_LTDC_RGB_OK;
}

void ch32h417_ltdc_rgb_enable(uint8_t enable)
{
    LTDC_Cmd((enable != 0u) ? ENABLE : DISABLE);
}

void ch32h417_ltdc_rgb_reload(void)
{
    LTDC_ReloadConfig(LTDC_IMReload);
}

void ch32h417_ltdc_rgb_set_background(const ch32h417_ltdc_rgb_color_t *color)
{
    if(color == 0)
    {
        return;
    }

    LTDC->BCCR = (((uint32_t)color->red) << 16) |
                 (((uint32_t)color->green) << 8) |
                 ((uint32_t)color->blue);
    ch32h417_ltdc_rgb_reload();
}

void ch32h417_ltdc_rgb_snapshot(ch32h417_ltdc_rgb_snapshot_t *snapshot)
{
    if(snapshot == 0)
    {
        return;
    }

    snapshot->sscr = LTDC->SSCR;
    snapshot->bpcr = LTDC->BPCR;
    snapshot->awcr = LTDC->AWCR;
    snapshot->twcr = LTDC->TWCR;
    snapshot->gcr = LTDC->GCR;
    snapshot->bccr = LTDC->BCCR;
    snapshot->cpsr = LTDC->CPSR;
    snapshot->cdsr = LTDC->CDSR;
    snapshot->isr = LTDC->ISR;
    snapshot->rcc_cfgr2 = RCC->CFGR2;
    snapshot->layer_whpcr = LTDC_Layer1->WHPCR;
    snapshot->layer_wvpcr = LTDC_Layer1->WVPCR;
    snapshot->layer_cfbar = LTDC_Layer1->CFBAR;
    snapshot->layer_cfblr = LTDC_Layer1->CFBLR;
    snapshot->layer_cfblnr = LTDC_Layer1->CFBLNR;
}

uint16_t ch32h417_ltdc_rgb_pack_rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)((((uint16_t)red & 0xF8u) << 8) |
                      (((uint16_t)green & 0xFCu) << 3) |
                      (((uint16_t)blue) >> 3));
}

void ch32h417_ltdc_rgb_framebuffer_barrier(void)
{
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

void ch32h417_ltdc_rgb_fb_fill_rgb565(uint16_t *framebuffer,
                                      uint16_t width,
                                      uint16_t height,
                                      uint16_t color)
{
    uint32_t i;
    uint32_t pixels;

    if((framebuffer == 0) || (width == 0u) || (height == 0u))
    {
        return;
    }

    pixels = (uint32_t)width * height;
    for(i = 0u; i < pixels; i++)
    {
        framebuffer[i] = color;
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

void ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(uint16_t *framebuffer,
                                             uint16_t width,
                                             uint16_t height,
                                             uint16_t x,
                                             uint16_t y,
                                             uint16_t color)
{
    if((framebuffer == 0) || (x >= width) || (y >= height))
    {
        return;
    }

    framebuffer[((uint32_t)(height - 1u - y) * width) +
                (width - 1u - x)] = color;
}

void ch32h417_ltdc_rgb_fb_fill_rect_rgb565_rot180(uint16_t *framebuffer,
                                                  uint16_t width,
                                                  uint16_t height,
                                                  uint16_t x,
                                                  uint16_t y,
                                                  uint16_t rect_width,
                                                  uint16_t rect_height,
                                                  uint16_t color)
{
    uint16_t row;
    uint16_t col;

    if((framebuffer == 0) ||
       (rect_width == 0u) ||
       (rect_height == 0u) ||
       (((uint32_t)x + rect_width) > width) ||
       (((uint32_t)y + rect_height) > height))
    {
        return;
    }

    for(row = 0u; row < rect_height; row++)
    {
        for(col = 0u; col < rect_width; col++)
        {
            ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(framebuffer,
                                                    width,
                                                    height,
                                                    (uint16_t)(x + col),
                                                    (uint16_t)(y + row),
                                                    color);
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

void ch32h417_ltdc_rgb_fb_draw_border_rgb565_rot180(uint16_t *framebuffer,
                                                    uint16_t width,
                                                    uint16_t height,
                                                    uint16_t color)
{
    uint16_t x;
    uint16_t y;

    if((framebuffer == 0) || (width == 0u) || (height == 0u))
    {
        return;
    }

    for(x = 0u; x < width; x++)
    {
        ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(framebuffer, width, height, x, 0u, color);
        ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(framebuffer,
                                                width,
                                                height,
                                                x,
                                                (uint16_t)(height - 1u),
                                                color);
    }
    for(y = 0u; y < height; y++)
    {
        ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(framebuffer, width, height, 0u, y, color);
        ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(framebuffer,
                                                width,
                                                height,
                                                (uint16_t)(width - 1u),
                                                y,
                                                color);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

void ch32h417_ltdc_rgb_fb_fill_l8(uint8_t *framebuffer,
                                  uint32_t bytes,
                                  uint8_t index)
{
    uint32_t i;

    if(framebuffer == 0)
    {
        return;
    }

    for(i = 0u; i < bytes; i++)
    {
        framebuffer[i] = index;
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

void ch32h417_ltdc_rgb_fb_plot_l8_rot180(uint8_t *framebuffer,
                                         uint16_t width,
                                         uint16_t height,
                                         uint16_t x,
                                         uint16_t y,
                                         uint8_t index)
{
    if((framebuffer == 0) || (x >= width) || (y >= height))
    {
        return;
    }

    framebuffer[((uint32_t)(height - 1u - y) * width) +
                (width - 1u - x)] = index;
}
