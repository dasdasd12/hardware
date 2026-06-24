#include "h417_common.h"
#include "h417_lcd_control.h"
#include "ch32h417_gd5f1g_spi1.h"
#include "ch32h417_ltdc.h"

#define LCD_WIDTH              800u
#define LCD_HEIGHT             480u
#define LCD_HSYNC              8u
#define LCD_HBP                10u
#define LCD_HFP                50u
#define LCD_VSYNC              4u
#define LCD_VBP                20u
#define LCD_VFP                16u

#define IMAGE_WIDTH            160u
#define IMAGE_HEIGHT           120u
#define IMAGE_BYTES            (IMAGE_WIDTH * IMAGE_HEIGHT * 2u)
#define IMAGE_PAGES            ((IMAGE_BYTES + GD5F1G_PAGE_SIZE - 1u) / GD5F1G_PAGE_SIZE)
#define IMAGE_TEST_START_BLOCK (GD5F1G_BLOCK_COUNT - 1u)
#define IMAGE_TEST_SCAN_BLOCKS 32u
#define BAD_BLOCK_MARK_COLUMN  2048u

#define LTDC_BYTES_PER_PIXEL   2u
#define LTDC_ACTIVE_START_X    (LCD_HSYNC + LCD_HBP)
#define LTDC_ACTIVE_START_Y    (LCD_VSYNC + LCD_VBP)
#define LTDC_LAYER_OFFSET_X    ((LCD_WIDTH - IMAGE_WIDTH) / 2u)
#define LTDC_LAYER_OFFSET_Y    ((LCD_HEIGHT - IMAGE_HEIGHT) / 2u)
#define LTDC_PIXEL_CLOCK_HZ    26666666u
#define LTDC_PLL_DIV_REGISTER  15u

#define LCD_DISP_TO_SIGNAL_DELAY  55000000u
#define LCD_SIGNAL_TO_BL_DELAY    1000000u

enum
{
    H417_ITEM_FLASH_LCD = 61,
    H417_ITEM_FLASH_BUS = 62,
    H417_ITEM_FLASH_ID = 63,
    H417_ITEM_FLASH_RESET = 64,
    H417_ITEM_FLASH_SELECT = 65,
    H417_ITEM_FLASH_UNLOCK = 66,
    H417_ITEM_FLASH_ERASE = 67,
    H417_ITEM_FLASH_PROGRAM = 68,
    H417_ITEM_FLASH_READ = 69,
    H417_ITEM_FLASH_VERIFY = 70,
    H417_ITEM_FLASH_DONE = 71
};

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t source;
    uint8_t alternate_function;
} lcd_pin_af_t;

typedef struct
{
    volatile int32_t result;
    volatile uint32_t state;
    volatile uint32_t manufacturer_id;
    volatile uint32_t device_id;
    volatile uint32_t id_mode;
    volatile uint32_t mode3_manufacturer_id;
    volatile uint32_t mode3_device_id;
    volatile uint32_t mode0_manufacturer_id;
    volatile uint32_t mode0_device_id;
    volatile uint32_t gpio_manufacturer_id;
    volatile uint32_t gpio_device_id;
    volatile uint32_t gpio_pullup_manufacturer_id;
    volatile uint32_t gpio_pullup_device_id;
    volatile uint32_t miso_probe;
    volatile uint32_t protection;
    volatile uint32_t config;
    volatile uint32_t status;
    volatile uint32_t status2;
    volatile uint32_t test_block;
    volatile uint32_t base_row;
    volatile uint32_t bad_block_marker;
    volatile uint32_t image_bytes;
    volatile uint32_t image_pages;
    volatile uint32_t expected_crc;
    volatile uint32_t read_crc;
    volatile uint32_t verify_errors;
    volatile uint32_t first_bad_offset;
    volatile uint32_t first_expected;
    volatile uint32_t first_actual;
    volatile uint32_t last_page;
    volatile uint32_t last_page_status;
    volatile uint32_t spi_timeout_count;
    volatile uint32_t layer_whpcr;
    volatile uint32_t layer_wvpcr;
    volatile uint32_t layer_cfbar;
    volatile uint32_t layer_cfblr;
} h417_flash_image_debug_t;

static const lcd_pin_af_t lcd_ltdc_pins[] = {
    {GPIOA, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF14},
    {GPIOA, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF14},
    {GPIOA, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF14},
    {GPIOB, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF9},
    {GPIOA, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF14},
    {GPIOC, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF14},
    {GPIOA, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF14},
    {GPIOC, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF14},
    {GPIOE, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF14},
    {GPIOE, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF14},
    {GPIOA, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF14},
    {GPIOF, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF9},
    {GPIOC, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF14},
    {GPIOC, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF14},
    {GPIOC, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF14},
    {GPIOD, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF14},
    {GPIOE, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF14},
    {GPIOC, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF10},
    {GPIOA, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF9},
    {GPIOD, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF14},
    {GPIOC, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF14},
    {GPIOD, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF14},
    {GPIOA, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF14},
    {GPIOD, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF9},
    {GPIOF, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF14},
    {GPIOC, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF14},
    {GPIOA, GPIO_Pin_4, GPIO_PinSource4, GPIO_AF14},
    {GPIOF, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF14},
};

static uint16_t s_lcd_layer[IMAGE_HEIGHT][IMAGE_WIDTH] __attribute__((aligned(64)));
static uint8_t s_program_page[GD5F1G_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t s_read_page[GD5F1G_PAGE_SIZE] __attribute__((aligned(4)));

volatile h417_flash_image_debug_t g_h417_flash_image_debug;

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)((((uint16_t)red & 0xF8u) << 8) |
                      (((uint16_t)green & 0xFCu) << 3) |
                      ((uint16_t)blue >> 3));
}

static uint32_t checksum_update(uint32_t checksum, uint8_t value)
{
    checksum ^= value;
    checksum *= 16777619u;
    return checksum;
}

static uint16_t image_pixel(unsigned int x, unsigned int y)
{
    uint8_t red = (uint8_t)((x * 255u) / (IMAGE_WIDTH - 1u));
    uint8_t green = (uint8_t)((y * 255u) / (IMAGE_HEIGHT - 1u));
    uint8_t blue = (uint8_t)((((x / 10u) ^ (y / 10u)) & 1u) ? 0xD0u : 0x28u);
    uint16_t pixel = rgb565(red, green, blue);

    if((x < 3u) || (y < 3u) ||
       (x >= (IMAGE_WIDTH - 3u)) ||
       (y >= (IMAGE_HEIGHT - 3u)))
    {
        pixel = 0xFFFFu;
    }
    else if((x < 32u) && (y < 32u))
    {
        if((x < 8u) && (y < 8u))
        {
            pixel = 0xFFFFu;
        }
        else if(y < 7u)
        {
            pixel = 0xF800u;
        }
        else if(x < 7u)
        {
            pixel = 0x07E0u;
        }
    }
    else if((x == y) || ((x + 1u) == y) || (x == (y + 1u)))
    {
        pixel = 0xFFE0u;
    }

    return pixel;
}

static uint8_t image_byte(uint32_t offset)
{
    uint32_t pixel_index = offset >> 1;
    unsigned int x = pixel_index % IMAGE_WIDTH;
    unsigned int y = pixel_index / IMAGE_WIDTH;
    uint16_t pixel = image_pixel(x, y);

    if((offset & 1u) == 0u)
    {
        return (uint8_t)pixel;
    }
    return (uint8_t)(pixel >> 8);
}

static uint32_t image_checksum(void)
{
    uint32_t offset;
    uint32_t checksum = 2166136261u;

    for(offset = 0u; offset < IMAGE_BYTES; ++offset)
    {
        checksum = checksum_update(checksum, image_byte(offset));
    }

    return checksum;
}

static uint32_t page_length(uint32_t page)
{
    uint32_t base = page * GD5F1G_PAGE_SIZE;
    uint32_t remaining = IMAGE_BYTES - base;

    if(remaining > GD5F1G_PAGE_SIZE)
    {
        return GD5F1G_PAGE_SIZE;
    }
    return remaining;
}

static void fill_program_page(uint32_t page, uint32_t length)
{
    uint32_t i;
    uint32_t base = page * GD5F1G_PAGE_SIZE;

    for(i = 0u; i < length; ++i)
    {
        s_program_page[i] = image_byte(base + i);
    }
}

static void layer_set_pixel(unsigned int logical_x,
                            unsigned int logical_y,
                            uint16_t color)
{
    if((logical_x < IMAGE_WIDTH) && (logical_y < IMAGE_HEIGHT))
    {
        unsigned int out_x = IMAGE_WIDTH - 1u - logical_x;
        unsigned int out_y = IMAGE_HEIGHT - 1u - logical_y;
        s_lcd_layer[out_y][out_x] = color;
    }
}

static void clear_read_layer(uint16_t color)
{
    unsigned int x;
    unsigned int y;

    for(y = 0u; y < IMAGE_HEIGHT; ++y)
    {
        for(x = 0u; x < IMAGE_WIDTH; ++x)
        {
            layer_set_pixel(x, y, color);
        }
    }
}

static void fill_layer_rect(unsigned int x0,
                            unsigned int y0,
                            unsigned int x1,
                            unsigned int y1,
                            uint16_t color)
{
    unsigned int x;
    unsigned int y;

    if((x0 >= IMAGE_WIDTH) || (y0 >= IMAGE_HEIGHT))
    {
        return;
    }
    if(x1 > IMAGE_WIDTH)
    {
        x1 = IMAGE_WIDTH;
    }
    if(y1 > IMAGE_HEIGHT)
    {
        y1 = IMAGE_HEIGHT;
    }

    for(y = y0; y < y1; ++y)
    {
        for(x = x0; x < x1; ++x)
        {
            layer_set_pixel(x, y, color);
        }
    }
}

static uint16_t flash_stage_color(uint32_t item_id)
{
    switch(item_id)
    {
        case H417_ITEM_FLASH_ID:
            return 0x2104u;
        case H417_ITEM_FLASH_RESET:
            return 0xFC00u;
        case H417_ITEM_FLASH_SELECT:
            return 0x07FFu;
        case H417_ITEM_FLASH_UNLOCK:
            return 0xFFE0u;
        case H417_ITEM_FLASH_ERASE:
            return 0xE000u;
        case H417_ITEM_FLASH_PROGRAM:
            return 0x001Fu;
        case H417_ITEM_FLASH_READ:
            return 0x07E0u;
        case H417_ITEM_FLASH_VERIFY:
            return 0x8410u;
        default:
            return 0x4208u;
    }
}

static unsigned int flash_stage_number(uint32_t item_id)
{
    if((item_id < H417_ITEM_FLASH_LCD) || (item_id > H417_ITEM_FLASH_DONE))
    {
        return 0u;
    }

    return (unsigned int)(item_id - H417_ITEM_FLASH_LCD + 1u);
}

static unsigned int flash_result_number(int result)
{
    if(result < 0)
    {
        return (unsigned int)(-result);
    }
    return (unsigned int)result;
}

static void draw_count_bars(unsigned int x0,
                            unsigned int count,
                            unsigned int width,
                            uint16_t color)
{
    unsigned int i;
    unsigned int max_count = (IMAGE_HEIGHT - 12u) / 8u;

    if(count > max_count)
    {
        count = max_count;
    }

    for(i = 0u; i < count; ++i)
    {
        unsigned int y0 = 6u + (i * 8u);
        fill_layer_rect(x0, y0, x0 + width, y0 + 5u, color);
    }
}

static void draw_byte_bits(unsigned int x0,
                           unsigned int y0,
                           uint8_t value,
                           uint16_t one_color,
                           uint16_t zero_color)
{
    unsigned int bit;

    for(bit = 0u; bit < 8u; ++bit)
    {
        uint16_t color = (value & (0x80u >> bit)) ? one_color : zero_color;
        unsigned int x = x0 + (bit * 10u);
        fill_layer_rect(x, y0, x + 8u, y0 + 8u, color);
    }
}

static void draw_probe_bits(unsigned int x0, unsigned int y0, uint32_t value)
{
    unsigned int bit;

    for(bit = 0u; bit < 6u; ++bit)
    {
        uint16_t color = (value & (1u << bit)) ? 0xFFFFu : 0xF800u;
        unsigned int x = x0 + (bit * 14u);
        fill_layer_rect(x, y0, x + 10u, y0 + 7u, color);
    }
}

static void draw_id_debug_bits(void)
{
    draw_byte_bits(40u,
                   24u,
                   (uint8_t)g_h417_flash_image_debug.mode3_manufacturer_id,
                   0xFFFFu,
                   0xF800u);
    draw_byte_bits(40u,
                   34u,
                   (uint8_t)g_h417_flash_image_debug.mode3_device_id,
                   0xFFFFu,
                   0xF800u);
    draw_byte_bits(40u,
                   46u,
                   (uint8_t)g_h417_flash_image_debug.mode0_manufacturer_id,
                   0x07E0u,
                   0x001Fu);
    draw_byte_bits(40u,
                   56u,
                   (uint8_t)g_h417_flash_image_debug.mode0_device_id,
                   0x07E0u,
                   0x001Fu);
    draw_byte_bits(40u,
                   68u,
                   (uint8_t)g_h417_flash_image_debug.gpio_manufacturer_id,
                   0xFFE0u,
                   0x07FFu);
    draw_byte_bits(40u,
                   78u,
                   (uint8_t)g_h417_flash_image_debug.gpio_device_id,
                   0xFFE0u,
                   0x07FFu);
    draw_byte_bits(40u,
                   90u,
                   (uint8_t)g_h417_flash_image_debug.gpio_pullup_manufacturer_id,
                   0xFFFFu,
                   0x8410u);
    draw_byte_bits(40u,
                   100u,
                   (uint8_t)g_h417_flash_image_debug.gpio_pullup_device_id,
                   0xFFFFu,
                   0x8410u);
    draw_probe_bits(40u, 111u, g_h417_flash_image_debug.miso_probe);
}

/*
 * This test often runs with no text log attached. A failure therefore leaves a
 * coded LCD pattern: left bars = test stage, right bars = absolute error code.
 */
static void flash_draw_diagnostic(uint32_t item_id, int result, uint8_t failed)
{
    uint16_t background = flash_stage_color(item_id);
    uint16_t frame = failed ? 0xFFFFu : 0x8410u;
    uint16_t top = failed ? 0xF800u : 0x001Fu;
    unsigned int stage_count = flash_stage_number(item_id);
    unsigned int result_count = flash_result_number(result);

    clear_read_layer(background);
    fill_layer_rect(0u, 0u, IMAGE_WIDTH, 4u, frame);
    fill_layer_rect(0u, IMAGE_HEIGHT - 4u, IMAGE_WIDTH, IMAGE_HEIGHT, frame);
    fill_layer_rect(0u, 0u, 4u, IMAGE_HEIGHT, frame);
    fill_layer_rect(IMAGE_WIDTH - 4u, 0u, IMAGE_WIDTH, IMAGE_HEIGHT, frame);
    fill_layer_rect(8u, 8u, IMAGE_WIDTH - 8u, 20u, top);

    draw_count_bars(10u, stage_count, 24u, 0xFFFFu);
    draw_count_bars(IMAGE_WIDTH - 34u, result_count, 24u, 0x0000u);
    if(item_id == H417_ITEM_FLASH_ID)
    {
        draw_id_debug_bits();
    }
    LTDC_ReloadConfig(LTDC_IMReload);
}

static void flash_show_progress(uint32_t item_id)
{
    g_h417_flash_image_debug.state = item_id;
    h417_status_phase(60u, item_id);
    flash_draw_diagnostic(item_id, GD5F1G_OK, 0u);
}

static void copy_read_page_to_layer(uint32_t page, uint32_t length)
{
    uint32_t i;
    uint32_t base = page * GD5F1G_PAGE_SIZE;

    for(i = 0u; (i + 1u) < length; i += 2u)
    {
        uint32_t pixel_index = (base + i) >> 1;
        unsigned int logical_x = pixel_index % IMAGE_WIDTH;
        unsigned int logical_y = pixel_index / IMAGE_WIDTH;
        uint16_t pixel = (uint16_t)s_read_page[i] |
                         ((uint16_t)s_read_page[i + 1u] << 8);

        if(logical_y < IMAGE_HEIGHT)
        {
            layer_set_pixel(logical_x, logical_y, pixel);
        }
    }
}

static void ltdc_gpio_af_init(void)
{
    GPIO_InitTypeDef init = {0};
    unsigned int i;

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

static void ltdc_clock_init(void)
{
    RCC_LTDCCLKConfig(RCC_LTDCClockSource_PLL);
    RCC_LTDCClockSourceDivConfig(RCC_LTDCClockSource_Div15);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_LTDC, ENABLE);

    (void)LTDC_PIXEL_CLOCK_HZ;
    (void)LTDC_PLL_DIV_REGISTER;
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
    init.LTDC_BackgroundRedValue = 0x00u;
    init.LTDC_BackgroundGreenValue = 0x00u;
    init.LTDC_BackgroundBlueValue = 0x00u;
    LTDC_Init(&init);
}

static void ltdc_layer_init(void)
{
    LTDC_Layer_InitTypeDef layer;
    const uint32_t line_pitch = IMAGE_WIDTH * LTDC_BYTES_PER_PIXEL;

    LTDC_LayerStructInit(&layer);
    layer.LTDC_HorizontalStart = LTDC_ACTIVE_START_X + LTDC_LAYER_OFFSET_X;
    layer.LTDC_HorizontalStop = layer.LTDC_HorizontalStart + IMAGE_WIDTH - 1u;
    layer.LTDC_VerticalStart = LTDC_ACTIVE_START_Y + LTDC_LAYER_OFFSET_Y;
    layer.LTDC_VerticalStop = layer.LTDC_VerticalStart + IMAGE_HEIGHT - 1u;
    layer.LTDC_PixelFormat = LTDC_Pixelformat_RGB565;
    layer.LTDC_ConstantAlpha = 0xFFu;
    layer.LTDC_DefaultColorBlue = 0x00u;
    layer.LTDC_DefaultColorGreen = 0x00u;
    layer.LTDC_DefaultColorRed = 0x00u;
    layer.LTDC_DefaultColorAlpha = 0x00u;
    layer.LTDC_BlendingFactor_1 = LTDC_BlendingFactor1_CA;
    layer.LTDC_BlendingFactor_2 = LTDC_BlendingFactor2_CA;
    layer.LTDC_CFBStartAdress = (uint32_t)&s_lcd_layer[0][0];
    layer.LTDC_CFBPitch = line_pitch;
    layer.LTDC_CFBLineLength = line_pitch + 31u;
    layer.LTDC_CFBLineNumber = IMAGE_HEIGHT;

    LTDC_LayerInit(LTDC_Layer1, &layer);
    LTDC_LayerCmd(LTDC_Layer1, ENABLE);
    LTDC_LayerCmd(LTDC_Layer2, DISABLE);
    LTDC_ReloadConfig(LTDC_IMReload);

    g_h417_flash_image_debug.layer_whpcr = LTDC_Layer1->WHPCR;
    g_h417_flash_image_debug.layer_wvpcr = LTDC_Layer1->WVPCR;
    g_h417_flash_image_debug.layer_cfbar = LTDC_Layer1->CFBAR;
    g_h417_flash_image_debug.layer_cfblr = LTDC_Layer1->CFBLR;
}

static void flash_lcd_init(void)
{
    h417_lcd_control_gpio_init();
    h417_lcd_disp_enable(1u);
    h417_delay_cycles(LCD_DISP_TO_SIGNAL_DELAY);

    ltdc_gpio_af_init();
    ltdc_clock_init();
    LTDC_DeInit();
    ltdc_timing_init();
    ltdc_layer_init();
    LTDC_Cmd(ENABLE);
    LTDC_ReloadConfig(LTDC_IMReload);

    h417_delay_cycles(LCD_SIGNAL_TO_BL_DELAY);
    h417_lcd_backlight_enable(1u);
}

static void debug_read_info(const gd5f1g_spi_bus_t *bus)
{
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;
    uint8_t value = 0u;

    (void)gd5f1g_read_id(bus, &manufacturer_id, &device_id);
    g_h417_flash_image_debug.manufacturer_id = manufacturer_id;
    g_h417_flash_image_debug.device_id = device_id;

    if(gd5f1g_get_feature(bus, GD5F1G_FEATURE_PROTECTION, &value) == GD5F1G_OK)
    {
        g_h417_flash_image_debug.protection = value;
    }
    if(gd5f1g_get_feature(bus, GD5F1G_FEATURE_CONFIG, &value) == GD5F1G_OK)
    {
        g_h417_flash_image_debug.config = value;
    }
    if(gd5f1g_get_feature(bus, GD5F1G_FEATURE_STATUS, &value) == GD5F1G_OK)
    {
        g_h417_flash_image_debug.status = value;
    }
    if(gd5f1g_get_feature(bus, GD5F1G_FEATURE_STATUS2, &value) == GD5F1G_OK)
    {
        g_h417_flash_image_debug.status2 = value;
    }
}

static void flash_stop_failed(const gd5f1g_spi_bus_t *bus,
                              const ch32h417_gd5f1g_spi1_context_t *spi_context,
                              uint32_t item_id,
                              int result)
{
    if(bus != 0)
    {
        debug_read_info(bus);
    }
    if(spi_context != 0)
    {
        g_h417_flash_image_debug.spi_timeout_count = spi_context->timeout_count;
    }

    g_h417_flash_image_debug.state = item_id;
    g_h417_flash_image_debug.result = result;
    h417_status_fail(item_id);
    flash_draw_diagnostic(item_id, result, 1u);

    while(1)
    {
        g_h417_status.cycle++;
        h417_delay_cycles(200000u);
    }
}

static void flash_stop_id_failed(gd5f1g_spi_bus_t *bus,
                                 ch32h417_gd5f1g_spi1_context_t *spi_context,
                                 int result)
{
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;

    g_h417_flash_image_debug.spi_timeout_count = spi_context->timeout_count;
    g_h417_flash_image_debug.state = H417_ITEM_FLASH_ID;
    g_h417_flash_image_debug.result = result;
    h417_status_fail(H417_ITEM_FLASH_ID);
    flash_draw_diagnostic(H417_ITEM_FLASH_ID, result, 1u);

    ch32h417_gd5f1g_gpio_init(spi_context, bus);
    while(1)
    {
        ch32h417_gd5f1g_gpio_read_id_slow(&manufacturer_id, &device_id);
        g_h417_flash_image_debug.gpio_manufacturer_id = manufacturer_id;
        g_h417_flash_image_debug.gpio_device_id = device_id;
        g_h417_status.cycle++;
        h417_delay_cycles(2000000u);
    }
}

static int flash_read_id_with_mode(ch32h417_gd5f1g_spi1_context_t *spi_context,
                                   const gd5f1g_spi_bus_t *bus,
                                   uint8_t mode,
                                   uint8_t *manufacturer_id,
                                   uint8_t *device_id)
{
    int result;

    ch32h417_gd5f1g_spi1_set_mode(spi_context, mode);
    if(bus->delay_us != 0)
    {
        bus->delay_us(bus->context, 20u);
    }

    result = gd5f1g_read_id(bus, manufacturer_id, device_id);
    if(mode == CH32H417_GD5F1G_SPI_MODE0)
    {
        g_h417_flash_image_debug.mode0_manufacturer_id = *manufacturer_id;
        g_h417_flash_image_debug.mode0_device_id = *device_id;
    }
    else
    {
        g_h417_flash_image_debug.mode3_manufacturer_id = *manufacturer_id;
        g_h417_flash_image_debug.mode3_device_id = *device_id;
    }

    return result;
}

static int flash_read_id_with_gpio(ch32h417_gd5f1g_spi1_context_t *spi_context,
                                   gd5f1g_spi_bus_t *bus,
                                   uint8_t *manufacturer_id,
                                   uint8_t *device_id)
{
    int result;

    ch32h417_gd5f1g_gpio_init(spi_context, bus);
    if(bus->delay_us != 0)
    {
        bus->delay_us(bus->context, 20u);
    }

    result = gd5f1g_read_id(bus, manufacturer_id, device_id);
    g_h417_flash_image_debug.gpio_manufacturer_id = *manufacturer_id;
    g_h417_flash_image_debug.gpio_device_id = *device_id;

    return result;
}

static int flash_read_id_with_gpio_pullup(ch32h417_gd5f1g_spi1_context_t *spi_context,
                                          gd5f1g_spi_bus_t *bus,
                                          uint8_t *manufacturer_id,
                                          uint8_t *device_id)
{
    int result;

    ch32h417_gd5f1g_gpio_pullup_init(spi_context, bus);
    if(bus->delay_us != 0)
    {
        bus->delay_us(bus->context, 20u);
    }

    result = gd5f1g_read_id(bus, manufacturer_id, device_id);
    g_h417_flash_image_debug.gpio_pullup_manufacturer_id = *manufacturer_id;
    g_h417_flash_image_debug.gpio_pullup_device_id = *device_id;

    return result;
}

static int record_step(int result, uint32_t item_id)
{
    g_h417_flash_image_debug.result = result;
    if(result == GD5F1G_OK)
    {
        h417_status_pass(item_id);
        return 1;
    }

    h417_status_fail(item_id);
    return 0;
}

static int select_scratch_block(const gd5f1g_spi_bus_t *bus,
                                uint32_t *block_out,
                                uint32_t *row_out)
{
    uint32_t i;

    for(i = 0u; i < IMAGE_TEST_SCAN_BLOCKS; ++i)
    {
        uint32_t block = IMAGE_TEST_START_BLOCK - i;
        uint32_t row = gd5f1g_block_to_row(block);
        uint8_t status = 0u;
        int result;

        result = gd5f1g_read_page(bus, row, BAD_BLOCK_MARK_COLUMN, s_read_page, 1u, &status);
        g_h417_flash_image_debug.last_page = row;
        g_h417_flash_image_debug.last_page_status = status;

        if((result == GD5F1G_OK) && (s_read_page[0] == 0xFFu))
        {
            *block_out = block;
            *row_out = row;
            g_h417_flash_image_debug.bad_block_marker = s_read_page[0];
            return GD5F1G_OK;
        }
    }

    g_h417_flash_image_debug.bad_block_marker = s_read_page[0];
    return GD5F1G_ERR_NO_SCRATCH_BLOCK;
}

void h417_flash_image_run(void)
{
    gd5f1g_spi_bus_t bus;
    ch32h417_gd5f1g_spi1_context_t spi_context;
    uint32_t page;
    uint32_t read_checksum = 2166136261u;
    uint32_t verify_errors = 0u;
    uint32_t test_block = IMAGE_TEST_START_BLOCK;
    uint32_t base_row = gd5f1g_block_to_row(IMAGE_TEST_START_BLOCK);
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;
    int result;

    clear_read_layer(0x001Fu);
    flash_lcd_init();
    h417_status_pass(H417_ITEM_FLASH_LCD);

    g_h417_flash_image_debug.state = H417_ITEM_FLASH_BUS;
    g_h417_flash_image_debug.test_block = test_block;
    g_h417_flash_image_debug.base_row = base_row;
    g_h417_flash_image_debug.image_bytes = IMAGE_BYTES;
    g_h417_flash_image_debug.image_pages = IMAGE_PAGES;
    g_h417_flash_image_debug.expected_crc = image_checksum();
    g_h417_flash_image_debug.first_bad_offset = 0xFFFFFFFFu;
    g_h417_flash_image_debug.id_mode = 0xFFFFFFFFu;
    g_h417_flash_image_debug.miso_probe = 0u;
    g_h417_flash_image_debug.gpio_pullup_manufacturer_id = 0u;
    g_h417_flash_image_debug.gpio_pullup_device_id = 0u;

    flash_show_progress(H417_ITEM_FLASH_BUS);
    ch32h417_gd5f1g_spi1_init(&spi_context, &bus);
    h417_status_pass(H417_ITEM_FLASH_BUS);

    flash_show_progress(H417_ITEM_FLASH_ID);
    g_h417_flash_image_debug.state = H417_ITEM_FLASH_ID;
    result = flash_read_id_with_mode(&spi_context,
                                     &bus,
                                     CH32H417_GD5F1G_SPI_MODE3,
                                     &manufacturer_id,
                                     &device_id);
    g_h417_flash_image_debug.manufacturer_id = manufacturer_id;
    g_h417_flash_image_debug.device_id = device_id;
    g_h417_flash_image_debug.id_mode = spi_context.active_mode;
    if(result != GD5F1G_OK)
    {
        result = flash_read_id_with_mode(&spi_context,
                                         &bus,
                                         CH32H417_GD5F1G_SPI_MODE0,
                                         &manufacturer_id,
                                         &device_id);
        g_h417_flash_image_debug.manufacturer_id = manufacturer_id;
        g_h417_flash_image_debug.device_id = device_id;
        g_h417_flash_image_debug.id_mode = spi_context.active_mode;
    }
    if(result != GD5F1G_OK)
    {
        result = flash_read_id_with_gpio(&spi_context,
                                         &bus,
                                         &manufacturer_id,
                                         &device_id);
        g_h417_flash_image_debug.manufacturer_id = manufacturer_id;
        g_h417_flash_image_debug.device_id = device_id;
        g_h417_flash_image_debug.id_mode = spi_context.active_mode;
    }
    if(result != GD5F1G_OK)
    {
        result = flash_read_id_with_gpio_pullup(&spi_context,
                                                &bus,
                                                &manufacturer_id,
                                                &device_id);
        g_h417_flash_image_debug.manufacturer_id = manufacturer_id;
        g_h417_flash_image_debug.device_id = device_id;
        g_h417_flash_image_debug.id_mode = spi_context.active_mode;
    }
    if(!record_step(result, H417_ITEM_FLASH_ID))
    {
        g_h417_flash_image_debug.miso_probe = ch32h417_gd5f1g_miso_probe();
        flash_stop_id_failed(&bus, &spi_context, result);
    }

    flash_show_progress(H417_ITEM_FLASH_RESET);
    result = gd5f1g_reset(&bus);
    debug_read_info(&bus);
    if(!record_step(result, H417_ITEM_FLASH_RESET))
    {
        flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_RESET, result);
    }

    flash_show_progress(H417_ITEM_FLASH_SELECT);
    g_h417_flash_image_debug.state = H417_ITEM_FLASH_SELECT;
    result = select_scratch_block(&bus, &test_block, &base_row);
    g_h417_flash_image_debug.test_block = test_block;
    g_h417_flash_image_debug.base_row = base_row;
    if(!record_step(result, H417_ITEM_FLASH_SELECT))
    {
        flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_SELECT, result);
    }

    flash_show_progress(H417_ITEM_FLASH_UNLOCK);
    g_h417_flash_image_debug.state = H417_ITEM_FLASH_UNLOCK;
    result = gd5f1g_unlock_all_blocks(&bus);
    debug_read_info(&bus);
    if(!record_step(result, H417_ITEM_FLASH_UNLOCK))
    {
        flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_UNLOCK, result);
    }

    flash_show_progress(H417_ITEM_FLASH_ERASE);
    g_h417_flash_image_debug.state = H417_ITEM_FLASH_ERASE;
    result = gd5f1g_block_erase(&bus, test_block);
    debug_read_info(&bus);
    if(!record_step(result, H417_ITEM_FLASH_ERASE))
    {
        flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_ERASE, result);
    }

    flash_show_progress(H417_ITEM_FLASH_PROGRAM);
    g_h417_flash_image_debug.state = H417_ITEM_FLASH_PROGRAM;
    for(page = 0u; page < IMAGE_PAGES; ++page)
    {
        uint32_t length = page_length(page);
        fill_program_page(page, length);
        result = gd5f1g_program_page(&bus, base_row + page, 0u, s_program_page, length);
        g_h417_flash_image_debug.last_page = page;
        debug_read_info(&bus);
        if(!record_step(result, H417_ITEM_FLASH_PROGRAM))
        {
            flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_PROGRAM, result);
        }
    }

    clear_read_layer(0x0000u);

    flash_show_progress(H417_ITEM_FLASH_READ);
    clear_read_layer(0x0000u);
    g_h417_flash_image_debug.state = H417_ITEM_FLASH_READ;
    for(page = 0u; page < IMAGE_PAGES; ++page)
    {
        uint32_t i;
        uint8_t page_status = 0u;
        uint32_t length = page_length(page);
        uint32_t page_base = page * GD5F1G_PAGE_SIZE;

        result = gd5f1g_read_page(&bus, base_row + page, 0u, s_read_page, length, &page_status);
        g_h417_flash_image_debug.last_page = page;
        g_h417_flash_image_debug.last_page_status = page_status;
        if(!record_step(result, H417_ITEM_FLASH_READ))
        {
            flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_READ, result);
        }

        for(i = 0u; i < length; ++i)
        {
            uint8_t expected = image_byte(page_base + i);
            uint8_t actual = s_read_page[i];

            read_checksum = checksum_update(read_checksum, actual);
            if(actual != expected)
            {
                verify_errors++;
                if(g_h417_flash_image_debug.first_bad_offset == 0xFFFFFFFFu)
                {
                    g_h417_flash_image_debug.first_bad_offset = page_base + i;
                    g_h417_flash_image_debug.first_expected = expected;
                    g_h417_flash_image_debug.first_actual = actual;
                }
            }
        }

        copy_read_page_to_layer(page, length);
        LTDC_ReloadConfig(LTDC_IMReload);
    }

    g_h417_flash_image_debug.state = H417_ITEM_FLASH_VERIFY;
    g_h417_flash_image_debug.read_crc = read_checksum;
    g_h417_flash_image_debug.verify_errors = verify_errors;
    g_h417_flash_image_debug.spi_timeout_count = spi_context.timeout_count;
    debug_read_info(&bus);

    if((verify_errors != 0u) ||
       (read_checksum != g_h417_flash_image_debug.expected_crc) ||
       (spi_context.timeout_count != 0u))
    {
        flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_VERIFY, GD5F1G_ERR_VERIFY);
    }
    else
    {
        record_step(GD5F1G_OK, H417_ITEM_FLASH_VERIFY);
        h417_status_pass(H417_ITEM_FLASH_DONE);
    }

    while(1)
    {
        g_h417_flash_image_debug.state = H417_ITEM_FLASH_DONE;
        g_h417_status.cycle++;
        h417_delay_cycles(200000u);
    }
}
