#include "h417_common.h"
#include "ch32h417_ltdc_rgb.h"

#define LTDC_LAYER_WIDTH       320u
#define LTDC_LAYER_HEIGHT      160u
#define LTDC_BYTES_PER_PIXEL   2u
#define LTDC_LAYER_OFFSET_X    (CH32H417_LCD_RGB_WIDTH - LTDC_LAYER_WIDTH)
#define LTDC_LAYER_OFFSET_Y    (CH32H417_LCD_RGB_HEIGHT - LTDC_LAYER_HEIGHT)
#define LTDC_REVERSE_COORDS    1u
#define LTDC_PLL_DIVIDER       15u

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
    volatile int32_t driver_result;
} h417_ltdc_debug_t;

static const ch32h417_ltdc_rgb_color_t ltdc_backgrounds[] = {
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

static uint16_t ltdc_pattern_pixel(unsigned int x, unsigned int y)
{
    static const ch32h417_ltdc_rgb_color_t colors[] = {
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

    for(y = 0u; y < LTDC_LAYER_HEIGHT; ++y)
    {
        for(x = 0u; x < LTDC_LAYER_WIDTH; ++x)
        {
            unsigned int logical_x = x;
            unsigned int logical_y = y;

            if(LTDC_REVERSE_COORDS != 0u)
            {
                logical_x = LTDC_LAYER_WIDTH - 1u - x;
                logical_y = LTDC_LAYER_HEIGHT - 1u - y;
            }

            s_ltdc_layer[y][x] = ltdc_pattern_pixel(logical_x, logical_y);
        }
    }
}

static void ltdc_snapshot(void)
{
    ch32h417_ltdc_rgb_snapshot_t snapshot;

    ch32h417_ltdc_rgb_snapshot(&snapshot);
    g_h417_ltdc_debug.pixel_clock_hz = ch32h417_ltdc_rgb_panel_800x480.pixel_clock_hz;
    g_h417_ltdc_debug.pll_divider = LTDC_PLL_DIVIDER;
    g_h417_ltdc_debug.sscr = snapshot.sscr;
    g_h417_ltdc_debug.bpcr = snapshot.bpcr;
    g_h417_ltdc_debug.awcr = snapshot.awcr;
    g_h417_ltdc_debug.twcr = snapshot.twcr;
    g_h417_ltdc_debug.gcr = snapshot.gcr;
    g_h417_ltdc_debug.bccr = snapshot.bccr;
    g_h417_ltdc_debug.cpsr = snapshot.cpsr;
    g_h417_ltdc_debug.cdsr = snapshot.cdsr;
    g_h417_ltdc_debug.isr = snapshot.isr;
    g_h417_ltdc_debug.rcc_cfgr2 = snapshot.rcc_cfgr2;
    g_h417_ltdc_debug.layer_whpcr = snapshot.layer_whpcr;
    g_h417_ltdc_debug.layer_wvpcr = snapshot.layer_wvpcr;
    g_h417_ltdc_debug.layer_cfbar = snapshot.layer_cfbar;
    g_h417_ltdc_debug.layer_cfblr = snapshot.layer_cfblr;
    g_h417_ltdc_debug.layer_cfblnr = snapshot.layer_cfblnr;
    g_h417_ltdc_debug.layer_offset_x = LTDC_LAYER_OFFSET_X;
    g_h417_ltdc_debug.layer_offset_y = LTDC_LAYER_OFFSET_Y;
}

static void ltdc_stop_failed(uint32_t item_id, int result)
{
    g_h417_ltdc_debug.driver_result = result;
    h417_status_fail(item_id);
    while(1)
    {
        g_h417_status.cycle++;
        h417_delay_cycles(200000u);
    }
}

void h417_ltdc_run(void)
{
    ch32h417_ltdc_rgb_layer_t layer;
    unsigned int background_index = 0u;
    int result;

    ch32h417_lcd_rgb_control_init();
    ch32h417_lcd_rgb_disp_enable(1u);
    h417_delay_cycles(LCD_DISP_TO_SIGNAL_DELAY);

    ltdc_fill_layer_pattern();
    result = ch32h417_ltdc_rgb_panel_init(&ch32h417_ltdc_rgb_panel_800x480,
                                          &ltdc_backgrounds[0]);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        ltdc_stop_failed(H417_ITEM_LTDC_TIMING, result);
    }
    h417_status_pass(H417_ITEM_LTDC_GPIO);
    h417_status_pass(H417_ITEM_LTDC_CLOCK);
    h417_status_pass(H417_ITEM_LTDC_TIMING);

    layer.width = LTDC_LAYER_WIDTH;
    layer.height = LTDC_LAYER_HEIGHT;
    layer.offset_x = LTDC_LAYER_OFFSET_X;
    layer.offset_y = LTDC_LAYER_OFFSET_Y;
    layer.pixel_format = LTDC_Pixelformat_RGB565;
    layer.framebuffer = (uint32_t)&s_ltdc_layer[0][0];
    layer.line_pitch = LTDC_LAYER_WIDTH * LTDC_BYTES_PER_PIXEL;
    result = ch32h417_ltdc_rgb_layer1_config(&ch32h417_ltdc_rgb_panel_800x480, &layer);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        ltdc_stop_failed(H417_ITEM_LTDC_LAYER, result);
    }
    ch32h417_ltdc_rgb_layer1_enable(1u);
    ch32h417_ltdc_rgb_layer2_enable(0u);
    ch32h417_ltdc_rgb_reload();
    h417_status_pass(H417_ITEM_LTDC_LAYER);

    ch32h417_ltdc_rgb_enable(1u);
    ch32h417_ltdc_rgb_reload();
    ltdc_snapshot();

    h417_delay_cycles(LCD_SIGNAL_TO_BL_DELAY);
    ch32h417_lcd_rgb_backlight_enable(1u);

    while(1)
    {
        ch32h417_ltdc_rgb_set_background(&ltdc_backgrounds[background_index]);
        ltdc_snapshot();
        h417_status_phase(40u, H417_ITEM_LTDC_RUNNING);
        h417_delay_cycles(30000000u);

        background_index++;
        if(background_index >= (sizeof(ltdc_backgrounds) / sizeof(ltdc_backgrounds[0])))
        {
            background_index = 0u;
            g_h417_status.cycle++;
            h417_status_pass(H417_ITEM_LTDC_RUNNING);
        }
    }
}
