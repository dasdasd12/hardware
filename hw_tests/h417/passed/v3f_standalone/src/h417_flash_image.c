#include "h417_common.h"
#include "ch32h417_gd5f1g_spi1.h"
#include "ch32h417_ltdc_rgb.h"

#define IMAGE_WIDTH            480u
#define IMAGE_HEIGHT           272u
#define IMAGE_BYTES            (IMAGE_WIDTH * IMAGE_HEIGHT * 2u)
#define IMAGE_PAGES            ((IMAGE_BYTES + GD5F1G_PAGE_SIZE - 1u) / GD5F1G_PAGE_SIZE)
#define IMAGE_BLOCKS           ((IMAGE_PAGES + GD5F1G_PAGES_PER_BLOCK - 1u) / GD5F1G_PAGES_PER_BLOCK)
#define IMAGE_TEST_START_BLOCK (GD5F1G_BLOCK_COUNT - 1u)
#define IMAGE_TEST_SCAN_BLOCKS 32u

#define LTDC_BYTES_PER_PIXEL   2u
#define LTDC_LAYER_OFFSET_X    ((CH32H417_LCD_RGB_WIDTH - IMAGE_WIDTH) / 2u)
#define LTDC_LAYER_OFFSET_Y    ((CH32H417_LCD_RGB_HEIGHT - IMAGE_HEIGHT) / 2u)

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
    int16_t x;
    int16_t y;
} image_point_t;

typedef struct
{
    image_point_t point[4];
    uint16_t color;
} image_quad_t;

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
    volatile uint32_t image_blocks;
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

static uint16_t s_lcd_layer[IMAGE_HEIGHT][IMAGE_WIDTH] __attribute__((aligned(64)));
static uint8_t s_program_page[GD5F1G_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t s_read_page[GD5F1G_PAGE_SIZE] __attribute__((aligned(4)));

volatile h417_flash_image_debug_t g_h417_flash_image_debug;

#define IMAGE_COLOR_BG          0xFF7Bu
#define IMAGE_COLOR_FRAME       0xFFFFu
#define IMAGE_COLOR_SHADOW      0xF6D6u
#define IMAGE_COLOR_ORANGE      0xE3A6u
#define IMAGE_COLOR_ORANGE_DARK 0xD240u
#define IMAGE_COLOR_YELLOW      0xFEC0u
#define IMAGE_COLOR_CODE        0x3186u

static const image_quad_t image_star_rays[] = {
    {{{-12, -20}, {-4, -126}, {18, -120}, {12, -18}}, IMAGE_COLOR_ORANGE},
    {{{8, -20}, {62, -112}, {82, -100}, {20, -10}}, IMAGE_COLOR_ORANGE_DARK},
    {{{18, -10}, {142, -52}, {150, -34}, {24, 4}}, IMAGE_COLOR_ORANGE},
    {{{24, 2}, {152, 8}, {150, 28}, {22, 18}}, IMAGE_COLOR_ORANGE_DARK},
    {{{18, 16}, {106, 66}, {96, 84}, {10, 28}}, IMAGE_COLOR_ORANGE},
    {{{8, 24}, {38, 126}, {16, 132}, {-2, 30}}, IMAGE_COLOR_ORANGE_DARK},
    {{{-8, 22}, {-44, 124}, {-64, 116}, {-18, 20}}, IMAGE_COLOR_ORANGE},
    {{{-20, 12}, {-122, 74}, {-136, 58}, {-24, 0}}, IMAGE_COLOR_ORANGE_DARK},
    {{{-24, -2}, {-154, -4}, {-154, -24}, {-22, -14}}, IMAGE_COLOR_ORANGE},
    {{{-18, -16}, {-118, -66}, {-106, -84}, {-8, -24}}, IMAGE_COLOR_ORANGE_DARK},
    {{{-8, -24}, {-62, -122}, {-40, -130}, {4, -26}}, IMAGE_COLOR_ORANGE},
    {{{14, 18}, {126, 92}, {110, 108}, {2, 28}}, IMAGE_COLOR_ORANGE_DARK},
};

static uint32_t checksum_update(uint32_t checksum, uint8_t value)
{
    checksum ^= value;
    checksum *= 16777619u;
    return checksum;
}

static int32_t image_cross(const image_point_t *a,
                           const image_point_t *b,
                           int32_t x,
                           int32_t y)
{
    return ((int32_t)b->x - a->x) * (y - a->y) -
           ((int32_t)b->y - a->y) * (x - a->x);
}

static uint8_t image_point_in_quad(int32_t x,
                                   int32_t y,
                                   const image_quad_t *quad)
{
    uint8_t i;
    uint8_t has_negative = 0u;
    uint8_t has_positive = 0u;

    for(i = 0u; i < 4u; ++i)
    {
        int32_t cross = image_cross(&quad->point[i],
                                    &quad->point[(i + 1u) & 3u],
                                    x,
                                    y);
        if(cross < 0)
        {
            has_negative = 1u;
        }
        else if(cross > 0)
        {
            has_positive = 1u;
        }
    }

    return (uint8_t)((has_negative == 0u) || (has_positive == 0u));
}

static uint8_t image_inside_ellipse(int32_t x,
                                    int32_t y,
                                    int32_t rx,
                                    int32_t ry)
{
    int32_t lhs = (x * x * ry * ry) + (y * y * rx * rx);
    int32_t rhs = rx * rx * ry * ry;

    return (uint8_t)(lhs <= rhs);
}

static uint8_t image_line_hit(int32_t x,
                              int32_t y,
                              int32_t x0,
                              int32_t y0,
                              int32_t x1,
                              int32_t y1,
                              int32_t width)
{
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t px = x - x0;
    int32_t py = y - y0;
    int32_t seg_len2 = (dx * dx) + (dy * dy);
    int32_t dot = (px * dx) + (py * dy);
    int32_t cross = (px * dy) - (py * dx);

    if(seg_len2 == 0)
    {
        return 0u;
    }
    if((dot < 0) || (dot > seg_len2))
    {
        return 0u;
    }
    if(cross < 0)
    {
        cross = -cross;
    }

    return (uint8_t)((cross * cross) <= (width * width * seg_len2));
}

static uint8_t image_code_mark(int32_t x, int32_t y)
{
    if(image_line_hit(x, y, -190, -38, -230, 0, 5) ||
       image_line_hit(x, y, -230, 0, -190, 38, 5) ||
       image_line_hit(x, y, 190, -38, 230, 0, 5) ||
       image_line_hit(x, y, 230, 0, 190, 38, 5) ||
       image_line_hit(x, y, 166, 56, 216, 56, 6))
    {
        return 1u;
    }

    return 0u;
}

static uint16_t image_pixel(unsigned int x, unsigned int y)
{
    int32_t cx = (int32_t)x - ((int32_t)IMAGE_WIDTH / 2);
    int32_t cy = (int32_t)y - ((int32_t)IMAGE_HEIGHT / 2);
    uint16_t pixel = IMAGE_COLOR_BG;
    unsigned int i;

    if((x < 3u) || (y < 3u) ||
       (x >= (IMAGE_WIDTH - 3u)) ||
       (y >= (IMAGE_HEIGHT - 3u)))
    {
        pixel = IMAGE_COLOR_FRAME;
    }
    else
    {
        if(image_inside_ellipse(cx + 8, cy + 8, 152, 112))
        {
            pixel = IMAGE_COLOR_SHADOW;
        }
        for(i = 0u; i < (sizeof(image_star_rays) / sizeof(image_star_rays[0])); ++i)
        {
            if(image_point_in_quad(cx, cy, &image_star_rays[i]) != 0u)
            {
                pixel = image_star_rays[i].color;
            }
        }
        if(image_inside_ellipse(cx, cy, 42, 36))
        {
            pixel = IMAGE_COLOR_ORANGE;
        }
        if(image_inside_ellipse(cx - 10, cy + 8, 20, 14))
        {
            pixel = IMAGE_COLOR_YELLOW;
        }
        if(image_code_mark(cx, cy) != 0u)
        {
            pixel = IMAGE_COLOR_CODE;
        }
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
    ch32h417_ltdc_rgb_reload();
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

static void flash_lcd_snapshot(void)
{
    ch32h417_ltdc_rgb_snapshot_t snapshot;

    ch32h417_ltdc_rgb_snapshot(&snapshot);
    g_h417_flash_image_debug.layer_whpcr = snapshot.layer_whpcr;
    g_h417_flash_image_debug.layer_wvpcr = snapshot.layer_wvpcr;
    g_h417_flash_image_debug.layer_cfbar = snapshot.layer_cfbar;
    g_h417_flash_image_debug.layer_cfblr = snapshot.layer_cfblr;
}

static int flash_lcd_layer_init(void)
{
    ch32h417_ltdc_rgb_layer_t layer;
    int result;

    layer.width = IMAGE_WIDTH;
    layer.height = IMAGE_HEIGHT;
    layer.offset_x = LTDC_LAYER_OFFSET_X;
    layer.offset_y = LTDC_LAYER_OFFSET_Y;
    layer.pixel_format = LTDC_Pixelformat_RGB565;
    layer.framebuffer = (uint32_t)&s_lcd_layer[0][0];
    layer.line_pitch = IMAGE_WIDTH * LTDC_BYTES_PER_PIXEL;

    result = ch32h417_ltdc_rgb_layer1_config(&ch32h417_ltdc_rgb_panel_800x480, &layer);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }

    ch32h417_ltdc_rgb_layer1_enable(1u);
    ch32h417_ltdc_rgb_layer2_enable(0u);
    ch32h417_ltdc_rgb_reload();
    flash_lcd_snapshot();

    return CH32H417_LTDC_RGB_OK;
}

static int flash_lcd_init(void)
{
    static const ch32h417_ltdc_rgb_color_t black = {0x00u, 0x00u, 0x00u};
    int result;

    ch32h417_lcd_rgb_control_init();
    ch32h417_lcd_rgb_disp_enable(1u);
    h417_delay_cycles(LCD_DISP_TO_SIGNAL_DELAY);

    result = ch32h417_ltdc_rgb_panel_init(&ch32h417_ltdc_rgb_panel_800x480, &black);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    result = flash_lcd_layer_init();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }

    ch32h417_ltdc_rgb_enable(1u);
    ch32h417_ltdc_rgb_reload();

    h417_delay_cycles(LCD_SIGNAL_TO_BL_DELAY);
    ch32h417_lcd_rgb_backlight_enable(1u);

    return CH32H417_LTDC_RGB_OK;
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
        uint32_t last_block = IMAGE_TEST_START_BLOCK - i;
        uint32_t first_block;
        uint32_t block;
        uint8_t range_ok = 1u;

        if(last_block < (IMAGE_BLOCKS - 1u))
        {
            break;
        }

        first_block = last_block - (IMAGE_BLOCKS - 1u);
        for(block = first_block; block <= last_block; ++block)
        {
            uint32_t row = gd5f1g_block_to_row(block);
            uint8_t status = 0u;
            uint8_t marker = 0u;
            int result;

            result = gd5f1g_read_bad_block_marker(bus, block, &marker, &status);
            g_h417_flash_image_debug.last_page = row;
            g_h417_flash_image_debug.last_page_status = status;
            g_h417_flash_image_debug.bad_block_marker = marker;

            if((result != GD5F1G_OK) || (marker != 0xFFu))
            {
                range_ok = 0u;
                break;
            }
        }

        if(range_ok != 0u)
        {
            *block_out = first_block;
            *row_out = gd5f1g_block_to_row(first_block);
            return GD5F1G_OK;
        }
    }

    return GD5F1G_ERR_NO_SCRATCH_BLOCK;
}

void h417_flash_image_run(void)
{
    gd5f1g_spi_bus_t bus;
    ch32h417_gd5f1g_spi1_context_t spi_context;
    uint32_t page;
    uint32_t block;
    uint32_t read_checksum = 2166136261u;
    uint32_t verify_errors = 0u;
    uint32_t test_block = IMAGE_TEST_START_BLOCK;
    uint32_t base_row = gd5f1g_block_to_row(IMAGE_TEST_START_BLOCK);
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;
    int result;

    clear_read_layer(0x001Fu);
    result = flash_lcd_init();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        g_h417_flash_image_debug.state = H417_ITEM_FLASH_LCD;
        g_h417_flash_image_debug.result = result;
        h417_status_fail(H417_ITEM_FLASH_LCD);
        while(1)
        {
            g_h417_status.cycle++;
            h417_delay_cycles(200000u);
        }
    }
    h417_status_pass(H417_ITEM_FLASH_LCD);

    g_h417_flash_image_debug.state = H417_ITEM_FLASH_BUS;
    g_h417_flash_image_debug.test_block = test_block;
    g_h417_flash_image_debug.base_row = base_row;
    g_h417_flash_image_debug.image_bytes = IMAGE_BYTES;
    g_h417_flash_image_debug.image_pages = IMAGE_PAGES;
    g_h417_flash_image_debug.image_blocks = IMAGE_BLOCKS;
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
    for(block = 0u; block < IMAGE_BLOCKS; ++block)
    {
        result = gd5f1g_block_erase(&bus, test_block + block);
        g_h417_flash_image_debug.last_page = gd5f1g_block_to_row(test_block + block);
        debug_read_info(&bus);
        if(!record_step(result, H417_ITEM_FLASH_ERASE))
        {
            flash_stop_failed(&bus, &spi_context, H417_ITEM_FLASH_ERASE, result);
        }
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
        ch32h417_ltdc_rgb_reload();
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
