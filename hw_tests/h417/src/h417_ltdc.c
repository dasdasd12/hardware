#include "h417_common.h"
#include "h417_lcd_control.h"
#include "ch32h417_ltdc.h"

#define LCD_WIDTH              800u
#define LCD_HEIGHT             480u
#define LCD_HSYNC              8u
#define LCD_HBP                10u
#define LCD_HFP                50u
#define LCD_VSYNC              4u
#define LCD_VBP                20u
#define LCD_VFP                16u

#define LTDC_LAYER_WIDTH       320u
#define LTDC_LAYER_HEIGHT      160u
#define LTDC_BYTES_PER_PIXEL   2u
#define LTDC_ACTIVE_START_X    (LCD_HSYNC + LCD_HBP)
#define LTDC_ACTIVE_START_Y    (LCD_VSYNC + LCD_VBP)
#define LTDC_LAYER_OFFSET_X    (LCD_WIDTH - LTDC_LAYER_WIDTH)
#define LTDC_LAYER_OFFSET_Y    (LCD_HEIGHT - LTDC_LAYER_HEIGHT)
#define LTDC_REVERSE_COORDS    1u

#define LTDC_PIXEL_CLOCK_HZ    26666666u
#define LTDC_PLL_DIV_REGISTER  15u
#define LTDC_BACKGROUND_COUNT  5u

#define LCD_DISP_TO_SIGNAL_DELAY  55000000u
#define LCD_SIGNAL_TO_BL_DELAY    1000000u

enum
{
    H417_ITEM_LTDC_GPIO = 41,
    H417_ITEM_LTDC_CLOCK = 42,
    H417_ITEM_LTDC_TIMING = 43,
    H417_ITEM_LTDC_LAYER = 44,
    H417_ITEM_LTDC_RUNNING = 45
};

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t source;
    uint8_t alternate_function;
} ltdc_pin_af_t;

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ltdc_rgb_t;

typedef struct
{
    volatile uint32_t pixel_clock_hz;
    volatile uint32_t pll_divider;
    volatile uint32_t sscr;
    volatile uint32_t bpcr;
    volatile uint32_t awcr;
    volatile uint32_t twcr;
    volatile uint32_t gcr;
    volatile uint32_t bccr;
    volatile uint32_t cpsr;
    volatile uint32_t cdsr;
    volatile uint32_t isr;
    volatile uint32_t rcc_cfgr2;
    volatile uint32_t layer_whpcr;
    volatile uint32_t layer_wvpcr;
    volatile uint32_t layer_cfbar;
    volatile uint32_t layer_cfblr;
    volatile uint32_t layer_cfblnr;
    volatile uint32_t layer_offset_x;
    volatile uint32_t layer_offset_y;
} h417_ltdc_debug_t;

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

static const ltdc_rgb_t ltdc_backgrounds[LTDC_BACKGROUND_COUNT] = {
    {0x18u, 0x00u, 0x00u},
    {0x00u, 0x18u, 0x00u},
    {0x00u, 0x00u, 0x18u},
    {0x18u, 0x18u, 0x18u},
    {0x00u, 0x00u, 0x00u},
};

static uint16_t s_ltdc_layer[LTDC_LAYER_HEIGHT][LTDC_LAYER_WIDTH] __attribute__((aligned(64)));

volatile h417_ltdc_debug_t g_h417_ltdc_debug;

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)((((uint16_t)red & 0xF8u) << 8) |
                      (((uint16_t)green & 0xFCu) << 3) |
                      ((uint16_t)blue >> 3));
}

static void ltdc_gpio_af_init(void)
{
    GPIO_InitTypeDef init = {0};
    unsigned int i;

    init.GPIO_Speed = GPIO_Speed_Very_High;
    init.GPIO_Mode = GPIO_Mode_AF_PP;

    for(i = 0; i < (sizeof(lcd_ltdc_pins) / sizeof(lcd_ltdc_pins[0])); ++i)
    {
        init.GPIO_Pin = lcd_ltdc_pins[i].pin;
        GPIO_PinAFConfig(lcd_ltdc_pins[i].port,
                         lcd_ltdc_pins[i].source,
                         lcd_ltdc_pins[i].alternate_function);
        GPIO_Init(lcd_ltdc_pins[i].port, &init);
    }
}

static uint16_t ltdc_pattern_pixel(unsigned int x, unsigned int y)
{
    static const ltdc_rgb_t colors[] = {
        {0xFFu, 0x00u, 0x00u},
        {0x00u, 0xFFu, 0x00u},
        {0x00u, 0x00u, 0xFFu},
        {0xFFu, 0xFFu, 0x00u},
        {0xFFu, 0x00u, 0xFFu},
        {0x00u, 0xFFu, 0xFFu},
        {0xFFu, 0xFFu, 0xFFu},
        {0x00u, 0x00u, 0x00u},
    };
    unsigned int bar = (x * (sizeof(colors) / sizeof(colors[0]))) / LTDC_LAYER_WIDTH;
    uint16_t pixel = rgb565(colors[bar].red, colors[bar].green, colors[bar].blue);

    if((x < 4u) || (y < 4u) ||
       (x >= (LTDC_LAYER_WIDTH - 4u)) ||
       (y >= (LTDC_LAYER_HEIGHT - 4u)))
    {
        pixel = 0xFFFFu;
    }
    else if((x < 48u) && (y < 48u))
    {
        if((x < 12u) && (y < 12u))
        {
            pixel = 0xFFFFu;
        }
        else if(y < 8u)
        {
            pixel = 0xF800u;
        }
        else if(x < 8u)
        {
            pixel = 0x07E0u;
        }
    }
    else if(((x / 16u) ^ (y / 16u)) & 0x01u)
    {
        pixel = (uint16_t)(pixel >> 1);
    }

    return pixel;
}

static void ltdc_fill_layer_pattern(void)
{
    unsigned int x;
    unsigned int y;

    for(y = 0; y < LTDC_LAYER_HEIGHT; ++y)
    {
        for(x = 0; x < LTDC_LAYER_WIDTH; ++x)
        {
            unsigned int logical_x = x;
            unsigned int logical_y = y;

            if(LTDC_REVERSE_COORDS)
            {
                logical_x = LTDC_LAYER_WIDTH - 1u - x;
                logical_y = LTDC_LAYER_HEIGHT - 1u - y;
            }

            s_ltdc_layer[y][x] = ltdc_pattern_pixel(logical_x, logical_y);
        }
    }
}

static void ltdc_clock_init(void)
{
    RCC_LTDCCLKConfig(RCC_LTDCClockSource_PLL);
    RCC_LTDCClockSourceDivConfig(RCC_LTDCClockSource_Div15);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_LTDC, ENABLE);

    g_h417_ltdc_debug.pixel_clock_hz = LTDC_PIXEL_CLOCK_HZ;
    g_h417_ltdc_debug.pll_divider = LTDC_PLL_DIV_REGISTER;
    g_h417_ltdc_debug.rcc_cfgr2 = RCC->CFGR2;
}

static void ltdc_background_set(const ltdc_rgb_t *color)
{
    LTDC->BCCR = (((uint32_t)color->red) << 16) |
                 (((uint32_t)color->green) << 8) |
                 ((uint32_t)color->blue);
    LTDC_ReloadConfig(LTDC_IMReload);
}

static void ltdc_snapshot(void)
{
    g_h417_ltdc_debug.sscr = LTDC->SSCR;
    g_h417_ltdc_debug.bpcr = LTDC->BPCR;
    g_h417_ltdc_debug.awcr = LTDC->AWCR;
    g_h417_ltdc_debug.twcr = LTDC->TWCR;
    g_h417_ltdc_debug.gcr = LTDC->GCR;
    g_h417_ltdc_debug.bccr = LTDC->BCCR;
    g_h417_ltdc_debug.cpsr = LTDC->CPSR;
    g_h417_ltdc_debug.cdsr = LTDC->CDSR;
    g_h417_ltdc_debug.isr = LTDC->ISR;
    g_h417_ltdc_debug.rcc_cfgr2 = RCC->CFGR2;
    g_h417_ltdc_debug.layer_whpcr = LTDC_Layer1->WHPCR;
    g_h417_ltdc_debug.layer_wvpcr = LTDC_Layer1->WVPCR;
    g_h417_ltdc_debug.layer_cfbar = LTDC_Layer1->CFBAR;
    g_h417_ltdc_debug.layer_cfblr = LTDC_Layer1->CFBLR;
    g_h417_ltdc_debug.layer_cfblnr = LTDC_Layer1->CFBLNR;
    g_h417_ltdc_debug.layer_offset_x = LTDC_LAYER_OFFSET_X;
    g_h417_ltdc_debug.layer_offset_y = LTDC_LAYER_OFFSET_Y;
}

static void ltdc_timing_init(void)
{
    LTDC_InitTypeDef init;

    LTDC_StructInit(&init);
    init.LTDC_HSPolarity = LTDC_HSPolarity_AH;
    init.LTDC_VSPolarity = LTDC_VSPolarity_AL;
    init.LTDC_DEPolarity = LTDC_DEPolarity_AH;
    init.LTDC_PCPolarity = LTDC_PCPolarity_IIPC;
    init.LTDC_HorizontalSync = LCD_HSYNC - 1u;
    init.LTDC_VerticalSync = LCD_VSYNC - 1u;
    init.LTDC_AccumulatedHBP = LCD_HSYNC + LCD_HBP - 1u;
    init.LTDC_AccumulatedVBP = LCD_VSYNC + LCD_VBP - 1u;
    init.LTDC_AccumulatedActiveW = LCD_HSYNC + LCD_HBP + LCD_WIDTH - 1u;
    init.LTDC_AccumulatedActiveH = LCD_VSYNC + LCD_VBP + LCD_HEIGHT - 1u;
    init.LTDC_TotalWidth = LCD_HSYNC + LCD_HBP + LCD_WIDTH + LCD_HFP - 1u;
    init.LTDC_TotalHeigh = LCD_VSYNC + LCD_VBP + LCD_HEIGHT + LCD_VFP - 1u;
    init.LTDC_BackgroundRedValue = ltdc_backgrounds[0].red;
    init.LTDC_BackgroundGreenValue = ltdc_backgrounds[0].green;
    init.LTDC_BackgroundBlueValue = ltdc_backgrounds[0].blue;
    LTDC_Init(&init);
}

static void ltdc_layer_init(void)
{
    LTDC_Layer_InitTypeDef layer;
    const uint32_t line_pitch = LTDC_LAYER_WIDTH * LTDC_BYTES_PER_PIXEL;

    LTDC_LayerStructInit(&layer);
    /* CH32H417 layer window registers use absolute LTDC timing coordinates. */
    layer.LTDC_HorizontalStart = LTDC_ACTIVE_START_X + LTDC_LAYER_OFFSET_X;
    layer.LTDC_HorizontalStop = layer.LTDC_HorizontalStart + LTDC_LAYER_WIDTH - 1u;
    layer.LTDC_VerticalStart = LTDC_ACTIVE_START_Y + LTDC_LAYER_OFFSET_Y;
    layer.LTDC_VerticalStop = layer.LTDC_VerticalStart + LTDC_LAYER_HEIGHT - 1u;
    layer.LTDC_PixelFormat = LTDC_Pixelformat_RGB565;
    layer.LTDC_ConstantAlpha = 0xFFu;
    layer.LTDC_DefaultColorBlue = 0x00u;
    layer.LTDC_DefaultColorGreen = 0x00u;
    layer.LTDC_DefaultColorRed = 0x00u;
    layer.LTDC_DefaultColorAlpha = 0x00u;
    layer.LTDC_BlendingFactor_1 = LTDC_BlendingFactor1_CA;
    layer.LTDC_BlendingFactor_2 = LTDC_BlendingFactor2_CA;
    layer.LTDC_CFBStartAdress = (uint32_t)&s_ltdc_layer[0][0];
    layer.LTDC_CFBPitch = line_pitch;
    layer.LTDC_CFBLineLength = line_pitch + 31u;
    layer.LTDC_CFBLineNumber = LTDC_LAYER_HEIGHT;

    LTDC_LayerInit(LTDC_Layer1, &layer);
    LTDC_LayerCmd(LTDC_Layer1, ENABLE);
    LTDC_LayerCmd(LTDC_Layer2, DISABLE);
    LTDC_ReloadConfig(LTDC_IMReload);
}

void h417_ltdc_run(void)
{
    unsigned int background_index = 0;

    h417_lcd_control_gpio_init();
    h417_lcd_disp_enable(1u);
    h417_delay_cycles(LCD_DISP_TO_SIGNAL_DELAY);

    ltdc_gpio_af_init();
    h417_status_pass(H417_ITEM_LTDC_GPIO);

    ltdc_fill_layer_pattern();
    ltdc_clock_init();
    h417_status_pass(H417_ITEM_LTDC_CLOCK);

    LTDC_DeInit();
    ltdc_timing_init();
    h417_status_pass(H417_ITEM_LTDC_TIMING);

    ltdc_layer_init();
    h417_status_pass(H417_ITEM_LTDC_LAYER);

    LTDC_Cmd(ENABLE);
    LTDC_ReloadConfig(LTDC_IMReload);
    ltdc_snapshot();

    h417_delay_cycles(LCD_SIGNAL_TO_BL_DELAY);
    h417_lcd_backlight_enable(1u);

    while(1)
    {
        ltdc_background_set(&ltdc_backgrounds[background_index]);
        ltdc_snapshot();
        h417_status_phase(40u, H417_ITEM_LTDC_RUNNING);
        h417_delay_cycles(30000000u);

        background_index++;
        if(background_index >= LTDC_BACKGROUND_COUNT)
        {
            background_index = 0;
            g_h417_status.cycle++;
            h417_status_pass(H417_ITEM_LTDC_RUNNING);
        }
    }
}
