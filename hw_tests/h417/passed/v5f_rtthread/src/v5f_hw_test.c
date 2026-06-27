#include "v5f_hw_test.h"

#include <rtthread.h>

#include "ch32h417_gpha_2d.h"
#include "ch32h417_ltdc_rgb.h"

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
#include "v5f_ltdc_gray_image.h"
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
#include "v5f_ltdc_palette_image.h"
#endif

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS)
#include "ch32h417_gd5f1g_spi1.h"
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
#include "gd5f1g_l8_asset_store.h"
#include "v5f_ltdc_flash_assets.h"
#endif

#ifndef APP_V5F_HW_TEST
#define APP_V5F_HW_TEST APP_V5F_HW_TEST_NONE
#endif

#ifndef APP_V5F_HW_TEST_NAME
#define APP_V5F_HW_TEST_NAME "unknown"
#endif

extern uint32_t SystemCoreClock;

#define V5F_L8_FB_WIDTH        800u
#define V5F_L8_FB_HEIGHT       480u
#define V5F_L8_FB_BYTES        (V5F_L8_FB_WIDTH * V5F_L8_FB_HEIGHT)
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
#define V5F_RGB_FB_WIDTH       800u
#define V5F_RGB_FB_HEIGHT      160u
#else
#define V5F_RGB_FB_WIDTH       320u
#define V5F_RGB_FB_HEIGHT      160u
#endif
#define V5F_RGB_FB_PIXELS      (V5F_RGB_FB_WIDTH * V5F_RGB_FB_HEIGHT)
#define V5F_RGB_FB_BYTES       (V5F_RGB_FB_PIXELS * 2u)
#define V5F_GPHA_L8_SRC_BYTES  V5F_RGB_FB_PIXELS
#define V5F_GPHA_L8_SRC_OFFSET V5F_RGB_FB_BYTES
#define V5F_GPHA_L8_CLUT_ENTRIES 256u
#define V5F_GPHA_L8_CLUT_BYTES   (V5F_GPHA_L8_CLUT_ENTRIES * 4u)
#define V5F_GPHA_L8_CLUT_OFFSET  (V5F_GPHA_L8_SRC_OFFSET + V5F_GPHA_L8_SRC_BYTES)
#define V5F_GPHA_BLEND_BG_BYTES  V5F_RGB_FB_BYTES
#define V5F_GPHA_BLEND_BG_OFFSET V5F_RGB_FB_BYTES
#define V5F_GPHA_BLEND_FG_BYTES  (V5F_RGB_FB_PIXELS * 2u)
#define V5F_GPHA_BLEND_FG_OFFSET (V5F_GPHA_BLEND_BG_OFFSET + V5F_GPHA_BLEND_BG_BYTES)
#define V5F_LCD_FB_REGION_SIZE (384u * 1024u)
#define V5F_MAYBE_UNUSED       __attribute__((unused))

#if V5F_L8_FB_BYTES > V5F_LCD_FB_REGION_SIZE
#error V5F L8 framebuffer exceeds reserved LCD_FB memory.
#endif

#if V5F_RGB_FB_BYTES > V5F_LCD_FB_REGION_SIZE
#error V5F RGB565 framebuffer exceeds reserved LCD_FB memory.
#endif

#if (V5F_GPHA_L8_SRC_OFFSET + V5F_GPHA_L8_SRC_BYTES) > V5F_LCD_FB_REGION_SIZE
#error V5F GPHA L8 source buffer exceeds reserved LCD_FB memory.
#endif

#if (V5F_GPHA_L8_CLUT_OFFSET + V5F_GPHA_L8_CLUT_BYTES) > V5F_LCD_FB_REGION_SIZE
#error V5F GPHA L8 CLUT buffer exceeds reserved LCD_FB memory.
#endif

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565) && \
    ((V5F_GPHA_BLEND_FG_OFFSET + V5F_GPHA_BLEND_FG_BYTES) > V5F_LCD_FB_REGION_SIZE)
#error V5F GPHA blend source buffers exceed reserved LCD_FB memory.
#endif

typedef enum
{
    V5F_HW_PHASE_BOOT = 0,
    V5F_HW_PHASE_LCD_READY = 1,
    V5F_HW_PHASE_RUNNING = 2,
    V5F_HW_PHASE_FAILED = 3,
} v5f_hw_phase_t;

static struct rt_thread s_test_thread;
static rt_uint8_t s_test_thread_stack[4096] __attribute__((aligned(8)));
static uint8_t s_lcd_fb[V5F_LCD_FB_REGION_SIZE] __attribute__((section(".lcd_fb"), aligned(64)));
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
static uint8_t s_gpha_l8_ltdc_clut_rgb888[CH32H417_LTDC_RGB_CLUT_ENTRIES * 3u];
#endif
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
static uint8_t s_flash_page[GD5F1G_PAGE_SIZE] __attribute__((aligned(4)));
static gd5f1g_l8_asset_manifest_t s_flash_manifest;
#endif

volatile v5f_hw_test_diag_t g_v5f_hw_test_diag;

static void V5F_MAYBE_UNUSED memory_barrier(void)
{
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static uint16_t *rgb_fb(void)
{
    return (uint16_t *)&s_lcd_fb[0];
}

static uint8_t *l8_fb(void) V5F_MAYBE_UNUSED;
static uint8_t *l8_fb(void)
{
    return &s_lcd_fb[0];
}

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
static uint8_t *gpha_l8_src(void)
{
    return &s_lcd_fb[V5F_GPHA_L8_SRC_OFFSET];
}

static uint32_t *gpha_l8_clut(void)
{
    return (uint32_t *)&s_lcd_fb[V5F_GPHA_L8_CLUT_OFFSET];
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565
static uint16_t *gpha_blend_bg(void)
{
    return (uint16_t *)&s_lcd_fb[V5F_GPHA_BLEND_BG_OFFSET];
}

static uint16_t *gpha_blend_fg_argb4444(void)
{
    return (uint16_t *)&s_lcd_fb[V5F_GPHA_BLEND_FG_OFFSET];
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
static void gpha_l8_ltdc_set_clut(uint8_t index,
                                  uint8_t red,
                                  uint8_t green,
                                  uint8_t blue)
{
    s_gpha_l8_ltdc_clut_rgb888[((uint32_t)index * 3u) + 0u] = red;
    s_gpha_l8_ltdc_clut_rgb888[((uint32_t)index * 3u) + 1u] = green;
    s_gpha_l8_ltdc_clut_rgb888[((uint32_t)index * 3u) + 2u] = blue;
}

static void gpha_l8_ltdc_build_clut(void)
{
    uint16_t i;

    for(i = 0u; i < CH32H417_LTDC_RGB_CLUT_ENTRIES; i++)
    {
        uint8_t level = (uint8_t)i;
        gpha_l8_ltdc_set_clut((uint8_t)i, level, level, level);
    }

    gpha_l8_ltdc_set_clut(0u, 0u, 0u, 0u);
    gpha_l8_ltdc_set_clut(1u, 255u, 0u, 0u);
    gpha_l8_ltdc_set_clut(2u, 0u, 255u, 0u);
    gpha_l8_ltdc_set_clut(3u, 0u, 0u, 255u);
    gpha_l8_ltdc_set_clut(4u, 255u, 255u, 255u);
    gpha_l8_ltdc_set_clut(5u, 0u, 255u, 255u);
    gpha_l8_ltdc_set_clut(6u, 255u, 255u, 0u);
    gpha_l8_ltdc_set_clut(7u, 255u, 0u, 255u);
    gpha_l8_ltdc_set_clut(8u, 255u, 128u, 0u);
    gpha_l8_ltdc_set_clut(9u, 128u, 64u, 255u);
    gpha_l8_ltdc_set_clut(10u, 32u, 32u, 32u);
    gpha_l8_ltdc_set_clut(11u, 192u, 192u, 192u);
}
#endif

static void load_l8_clut_after_layer_start(void)
{
    /*
     * On this H417 board, LTDC L8 color lookup writes are reliable only after
     * the controller and layer are running. Pre-start CLUT writes produced a
     * stable but shifted color mapping during hardware validation.
     */
    rt_thread_mdelay(100);
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
    (void)ch32h417_ltdc_rgb_layer1_load_clut_rgb888(
        v5f_ltdc_palette_800x480_clut_rgb888,
        V5F_LTDC_PALETTE_CLUT_ENTRIES);
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
    gpha_l8_ltdc_build_clut();
    (void)ch32h417_ltdc_rgb_layer1_load_clut_rgb888(
        s_gpha_l8_ltdc_clut_rgb888,
        CH32H417_LTDC_RGB_CLUT_ENTRIES);
#else
    ch32h417_ltdc_rgb_layer1_load_grayscale_clut();
#endif
}

static void fb_fill_rgb565(uint16_t color)
{
    ch32h417_ltdc_rgb_fb_fill_rgb565(rgb_fb(),
                                     V5F_RGB_FB_WIDTH,
                                     V5F_RGB_FB_HEIGHT,
                                     color);
}

static void V5F_MAYBE_UNUSED fb_plot_user_rgb565(uint16_t x, uint16_t y, uint16_t color)
{
    /*
     * The mounted panel is rotated 180 degrees. Keep test coordinates in
     * the user's visual direction and mirror them into framebuffer memory.
     */
    ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(rgb_fb(),
                                            V5F_RGB_FB_WIDTH,
                                            V5F_RGB_FB_HEIGHT,
                                            x,
                                            y,
                                            color);
}

static void V5F_MAYBE_UNUSED fb_fill_user_rect_rgb565(uint16_t x,
                                                      uint16_t y,
                                                      uint16_t width,
                                                      uint16_t height,
                                                      uint16_t color)
{
    ch32h417_ltdc_rgb_fb_fill_rect_rgb565_rot180(rgb_fb(),
                                                 V5F_RGB_FB_WIDTH,
                                                 V5F_RGB_FB_HEIGHT,
                                                 x,
                                                 y,
                                                 width,
                                                 height,
                                                 color);
}

static void V5F_MAYBE_UNUSED fb_draw_border_rgb565(uint16_t color)
{
    ch32h417_ltdc_rgb_fb_draw_border_rgb565_rot180(rgb_fb(),
                                                   V5F_RGB_FB_WIDTH,
                                                   V5F_RGB_FB_HEIGHT,
                                                   color);
}

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_TICK_DIAG
static uint32_t v5f_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static void fb_draw_tick_diag_half(uint8_t rt_side, uint8_t state)
{
    uint16_t x = (rt_side != 0u) ? 0u : (V5F_RGB_FB_WIDTH / 2u);
    uint16_t width = V5F_RGB_FB_WIDTH / 2u;
    uint16_t color;

    if(rt_side != 0u)
    {
        color = (state != 0u) ? ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u) : ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 0u);
    }
    else
    {
        color = (state != 0u) ? ch32h417_ltdc_rgb_pack_rgb565(0u, 0u, 255u) : ch32h417_ltdc_rgb_pack_rgb565(0u, 255u, 0u);
    }

    fb_fill_user_rect_rgb565(x, 0u, width, V5F_RGB_FB_HEIGHT, color);
    fb_fill_user_rect_rgb565((V5F_RGB_FB_WIDTH / 2u) - 1u,
                             0u,
                             2u,
                             V5F_RGB_FB_HEIGHT,
                             ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));
}

static void run_tick_diag_test(void)
{
    rt_tick_t last_rt_tick = rt_tick_get();
    uint32_t last_cycle = v5f_cycle_now();
    uint32_t cycle_interval = (SystemCoreClock != 0u) ? SystemCoreClock : 400000000u;
    uint8_t rt_state = 0u;
    uint8_t cycle_state = 0u;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    fb_draw_tick_diag_half(1u, rt_state);
    fb_draw_tick_diag_half(0u, cycle_state);
    fb_draw_border_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));

    while(1)
    {
        if(rt_tick_get_delta(last_rt_tick) >= RT_TICK_PER_SECOND)
        {
            last_rt_tick += RT_TICK_PER_SECOND;
            rt_state ^= 1u;
            fb_draw_tick_diag_half(1u, rt_state);
        }

        if((uint32_t)(v5f_cycle_now() - last_cycle) >= cycle_interval)
        {
            last_cycle += cycle_interval;
            cycle_state ^= 1u;
            fb_draw_tick_diag_half(0u, cycle_state);
        }

        g_v5f_hw_test_diag.frame_count++;
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
static void fb_draw_rgb565_channel_diag(void)
{
    uint16_t x;
    uint16_t y;

    for(y = 0u; y < V5F_RGB_FB_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_RGB_FB_WIDTH; x++)
        {
            uint8_t level = (uint8_t)(((uint32_t)x * 255u) / (V5F_RGB_FB_WIDTH - 1u));
            uint16_t color;

            if(y < (V5F_RGB_FB_HEIGHT / 4u))
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(level, 0u, 0u);
            }
            else if(y < (V5F_RGB_FB_HEIGHT / 2u))
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(0u, level, 0u);
            }
            else if(y < ((V5F_RGB_FB_HEIGHT * 3u) / 4u))
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(0u, 0u, level);
            }
            else
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(level, level, level);
            }
            fb_plot_user_rgb565(x, y, color);
        }
    }
    memory_barrier();
}

static void run_ltdc_rgb565_diag_test(void)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

static int lcd_start_layer(uint16_t width,
                           uint16_t height,
                           uint32_t pixel_format,
                           uint32_t line_pitch)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};
    int result;

    ch32h417_lcd_rgb_control_init();
    ch32h417_lcd_rgb_disp_enable(1u);

    layer.width = width;
    layer.height = height;
    layer.offset_x = (uint16_t)((CH32H417_LCD_RGB_WIDTH - width) / 2u);
    layer.offset_y = (uint16_t)((CH32H417_LCD_RGB_HEIGHT - height) / 2u);
    layer.pixel_format = pixel_format;
    layer.framebuffer = (uint32_t)&s_lcd_fb[0];
    layer.line_pitch = line_pitch;

    result = ch32h417_ltdc_rgb_start_layer1(&ch32h417_ltdc_rgb_panel_800x480,
                                            &layer,
                                            &black);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    ch32h417_lcd_rgb_backlight_enable(1u);
    if(pixel_format == LTDC_Pixelformat_L8)
    {
        load_l8_clut_after_layer_start();
    }
    return CH32H417_LTDC_RGB_OK;
}

static int V5F_MAYBE_UNUSED lcd_start_l8_fullscreen(void)
{
    return lcd_start_layer(V5F_L8_FB_WIDTH,
                           V5F_L8_FB_HEIGHT,
                           LTDC_Pixelformat_L8,
                           V5F_L8_FB_WIDTH);
}

static int V5F_MAYBE_UNUSED lcd_start_rgb565_window(void)
{
    fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(0u, 0u, 0u));
    return lcd_start_layer(V5F_RGB_FB_WIDTH,
                           V5F_RGB_FB_HEIGHT,
                           LTDC_Pixelformat_RGB565,
                           V5F_RGB_FB_WIDTH * 2u);
}

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
static int ltdc_gray_image_valid(void)
{
    uint32_t image_size = (uint32_t)(v5f_ltdc_gray_800x480_end - v5f_ltdc_gray_800x480);

    return (image_size == V5F_L8_FB_BYTES) &&
           (V5F_LTDC_GRAY_IMAGE_WIDTH == V5F_L8_FB_WIDTH) &&
           (V5F_LTDC_GRAY_IMAGE_HEIGHT == V5F_L8_FB_HEIGHT) &&
           (V5F_LTDC_GRAY_IMAGE_BYTES == V5F_L8_FB_BYTES);
}

static void fb_load_ltdc_gray_image(void)
{
    uint32_t i;

    /*
     * The generated asset is already cropped to 800x480 grayscale and stored
     * in 180-degree rotated framebuffer order for the mounted panel.
     */
    for(i = 0u; i < V5F_L8_FB_BYTES; i++)
    {
        s_lcd_fb[i] = v5f_ltdc_gray_800x480[i];
    }
    memory_barrier();
}

static void run_ltdc_test(void)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
static int ltdc_palette_image_valid(void)
{
    uint32_t image_size =
        (uint32_t)(v5f_ltdc_palette_800x480_end - v5f_ltdc_palette_800x480);
    uint32_t clut_size =
        (uint32_t)(v5f_ltdc_palette_800x480_clut_rgb888_end -
                   v5f_ltdc_palette_800x480_clut_rgb888);

    return (image_size == V5F_L8_FB_BYTES) &&
           (clut_size == V5F_LTDC_PALETTE_CLUT_BYTES) &&
           (V5F_LTDC_PALETTE_IMAGE_WIDTH == V5F_L8_FB_WIDTH) &&
           (V5F_LTDC_PALETTE_IMAGE_HEIGHT == V5F_L8_FB_HEIGHT) &&
           (V5F_LTDC_PALETTE_IMAGE_BYTES == V5F_L8_FB_BYTES);
}

static void fb_load_ltdc_palette_image(void)
{
    uint32_t i;

    for(i = 0u; i < V5F_L8_FB_BYTES; i++)
    {
        s_lcd_fb[i] = v5f_ltdc_palette_800x480[i];
    }
    memory_barrier();
}

static void run_ltdc_l8_palette_image_test(void)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_R2M_FILL
static int gpha_fill_rect_actual(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height,
                                 uint16_t color)
{
    return ch32h417_gpha_2d_fill_rgb565(rgb_fb(),
                                        V5F_RGB_FB_WIDTH,
                                        V5F_RGB_FB_HEIGHT,
                                        x,
                                        y,
                                        width,
                                        height,
                                        color);
}

static int gpha_fill_user_rect(uint16_t x,
                               uint16_t y,
                               uint16_t width,
                               uint16_t height,
                               uint16_t color)
{
    uint16_t actual_x;
    uint16_t actual_y;

    if((width == 0u) || (height == 0u) ||
       (((uint32_t)x + width) > V5F_RGB_FB_WIDTH) ||
       (((uint32_t)y + height) > V5F_RGB_FB_HEIGHT))
    {
        return -1;
    }

    actual_x = (uint16_t)(V5F_RGB_FB_WIDTH - x - width);
    actual_y = (uint16_t)(V5F_RGB_FB_HEIGHT - y - height);
    return gpha_fill_rect_actual(actual_x, actual_y, width, height, color);
}

static uint16_t advance_position(uint16_t pos, uint8_t *forward)
{
    const uint16_t step = 4u;
    const uint16_t max_pos = V5F_RGB_FB_WIDTH - 112u;

    if(*forward != 0u)
    {
        if((uint16_t)(pos + step) >= max_pos)
        {
            *forward = 0u;
            return max_pos;
        }
        return (uint16_t)(pos + step);
    }

    if(pos <= step)
    {
        *forward = 1u;
        return 0u;
    }
    return (uint16_t)(pos - step);
}

static void run_gpha_r2m_fill_test(void)
{
    const uint16_t bg = ch32h417_ltdc_rgb_pack_rgb565(8u, 10u, 18u);
    const uint16_t orange = ch32h417_ltdc_rgb_pack_rgb565(255u, 108u, 16u);
    const uint16_t cyan = ch32h417_ltdc_rgb_pack_rgb565(0u, 255u, 255u);
    uint16_t pos = 0u;
    uint16_t old_pos = 0xFFFFu;
    uint8_t forward = 1u;
    int result;

    ch32h417_gpha_2d_init();
    result = gpha_fill_rect_actual(0u, 0u, V5F_RGB_FB_WIDTH, V5F_RGB_FB_HEIGHT, bg);
    if(result == 0)
    {
        fb_draw_border_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        result = 0;
        if(old_pos != 0xFFFFu)
        {
            result = gpha_fill_user_rect(old_pos, 36u, 88u, 42u, bg);
            if(result == 0)
            {
                result = gpha_fill_user_rect((uint16_t)(V5F_RGB_FB_WIDTH - 112u - old_pos),
                                             92u,
                                             64u,
                                             26u,
                                             bg);
            }
        }
        if(result == 0)
        {
            result = gpha_fill_user_rect(pos, 36u, 88u, 42u, orange);
        }
        if(result == 0)
        {
            result = gpha_fill_user_rect((uint16_t)(V5F_RGB_FB_WIDTH - 112u - pos),
                                         92u,
                                         64u,
                                         26u,
                                         cyan);
        }

        if(result == 0)
        {
            g_v5f_hw_test_diag.gpha_ok_count++;
            old_pos = pos;
            pos = advance_position(pos, &forward);
        }
        else
        {
            g_v5f_hw_test_diag.gpha_fail_count++;
            g_v5f_hw_test_diag.last_error = result;
            fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u));
            old_pos = 0xFFFFu;
        }

        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(16);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
static int gpha_fill_l8_quad_actual(uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    uint16_t height,
                                    uint8_t index0,
                                    uint8_t index1,
                                    uint8_t index2,
                                    uint8_t index3)
{
    return ch32h417_gpha_2d_fill_l8_quad(l8_fb(),
                                         V5F_L8_FB_WIDTH,
                                         V5F_L8_FB_HEIGHT,
                                         x,
                                         y,
                                         width,
                                         height,
                                         index0,
                                         index1,
                                         index2,
                                         index3);
}

static int gpha_fill_l8_quad_user(uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint8_t index0,
                                  uint8_t index1,
                                  uint8_t index2,
                                  uint8_t index3)
{
    uint16_t actual_x;
    uint16_t actual_y;

    if((width == 0u) || (height == 0u) ||
       ((x & 0x3u) != 0u) || ((width & 0x3u) != 0u) ||
       (((uint32_t)x + width) > V5F_L8_FB_WIDTH) ||
       (((uint32_t)y + height) > V5F_L8_FB_HEIGHT))
    {
        return -1;
    }

    actual_x = (uint16_t)(V5F_L8_FB_WIDTH - x - width);
    actual_y = (uint16_t)(V5F_L8_FB_HEIGHT - y - height);
    return gpha_fill_l8_quad_actual(actual_x,
                                    actual_y,
                                    width,
                                    height,
                                    index3,
                                    index2,
                                    index1,
                                    index0);
}

static int gpha_fill_l8_solid_user(uint16_t x,
                                   uint16_t y,
                                   uint16_t width,
                                   uint16_t height,
                                   uint8_t index)
{
    return gpha_fill_l8_quad_user(x, y, width, height, index, index, index, index);
}

static int gpha_l8_ltdc_draw_pattern(void)
{
    uint16_t x;
    int result;

    result = gpha_fill_l8_solid_user(0u, 0u, V5F_L8_FB_WIDTH, V5F_L8_FB_HEIGHT, 10u);
    if(result != 0)
    {
        return result;
    }

    result = gpha_fill_l8_quad_user(0u, 0u, V5F_L8_FB_WIDTH, 32u, 1u, 2u, 3u, 4u);
    if(result != 0)
    {
        return result;
    }

    result = gpha_fill_l8_solid_user(0u, 32u, V5F_L8_FB_WIDTH, 64u, 1u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(0u, 96u, V5F_L8_FB_WIDTH, 64u, 2u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(0u, 160u, V5F_L8_FB_WIDTH, 64u, 3u);
    if(result != 0)
    {
        return result;
    }

    result = gpha_fill_l8_solid_user(0u, 224u, 200u, 96u, 5u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(200u, 224u, 200u, 96u, 6u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(400u, 224u, 200u, 96u, 7u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(600u, 224u, 200u, 96u, 4u);
    if(result != 0)
    {
        return result;
    }

    for(x = 0u; x < V5F_L8_FB_WIDTH; x = (uint16_t)(x + 32u))
    {
        uint8_t index = (uint8_t)(16u + ((uint32_t)x * 224u / V5F_L8_FB_WIDTH));
        result = gpha_fill_l8_solid_user(x, 320u, 32u, 160u, index);
        if(result != 0)
        {
            return result;
        }
    }

    result = gpha_fill_l8_solid_user(0u, 0u, 4u, V5F_L8_FB_HEIGHT, 4u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user((uint16_t)(V5F_L8_FB_WIDTH - 4u), 0u, 4u, V5F_L8_FB_HEIGHT, 4u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(0u, (uint16_t)(V5F_L8_FB_HEIGHT - 4u), V5F_L8_FB_WIDTH, 4u, 4u);
    if(result != 0)
    {
        return result;
    }

    return gpha_fill_l8_solid_user(0u, 0u, V5F_L8_FB_WIDTH, 4u, 4u);
}

static void run_gpha_l8_ltdc_fullscreen_test(void)
{
    int result;

    ch32h417_gpha_2d_init();
    result = gpha_l8_ltdc_draw_pattern();
    if(result == 0)
    {
        g_v5f_hw_test_diag.gpha_ok_count++;
    }
    else
    {
        g_v5f_hw_test_diag.gpha_fail_count++;
        g_v5f_hw_test_diag.last_error = result;
        ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, 1u);
        memory_barrier();
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
static uint32_t gpha_argb8888(uint8_t red, uint8_t green, uint8_t blue)
{
    return ch32h417_gpha_2d_argb8888(red, green, blue);
}

static void gpha_l8_fill_clut(void)
{
    uint32_t i;
    uint32_t *clut = gpha_l8_clut();

    for(i = 0u; i < V5F_GPHA_L8_CLUT_ENTRIES; i++)
    {
        clut[i] = gpha_argb8888(0u, 0u, 0u);
    }

    clut[1] = gpha_argb8888(255u, 0u, 0u);
    clut[2] = gpha_argb8888(0u, 255u, 0u);
    clut[3] = gpha_argb8888(0u, 0u, 255u);
    clut[4] = gpha_argb8888(255u, 255u, 255u);
    memory_barrier();
}

static void gpha_l8_plot_user(uint16_t x, uint16_t y, uint8_t index)
{
    uint8_t *src;

    if((x >= V5F_RGB_FB_WIDTH) || (y >= V5F_RGB_FB_HEIGHT))
    {
        return;
    }

    src = gpha_l8_src();
    src[((uint32_t)(V5F_RGB_FB_HEIGHT - 1u - y) * V5F_RGB_FB_WIDTH) +
        (V5F_RGB_FB_WIDTH - 1u - x)] = index;
}

static void gpha_l8_draw_user_bars(void)
{
    uint16_t x;
    uint16_t y;

    for(y = 0u; y < V5F_RGB_FB_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_RGB_FB_WIDTH; x++)
        {
            uint8_t index = (uint8_t)((uint32_t)x * 5u / V5F_RGB_FB_WIDTH);
            gpha_l8_plot_user(x, y, index);
        }
    }
    memory_barrier();
}

static int gpha_l8_clut_to_rgb565(void)
{
    return ch32h417_gpha_2d_l8_to_rgb565(gpha_l8_src(),
                                          rgb_fb(),
                                          gpha_l8_clut(),
                                          V5F_RGB_FB_WIDTH,
                                          V5F_RGB_FB_HEIGHT,
                                          V5F_GPHA_L8_CLUT_ENTRIES);
}

static void run_gpha_pfc_l8_rgb565_test(void)
{
    int result;

    ch32h417_gpha_2d_init();
    gpha_l8_fill_clut();
    gpha_l8_draw_user_bars();
    result = gpha_l8_clut_to_rgb565();
    if(result == 0)
    {
        g_v5f_hw_test_diag.gpha_ok_count++;
    }
    else
    {
        g_v5f_hw_test_diag.gpha_fail_count++;
        g_v5f_hw_test_diag.last_error = result;
        fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u));
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565
static uint16_t gpha_argb4444(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue)
{
    return ch32h417_gpha_2d_argb4444(alpha, red, green, blue);
}

static uint32_t gpha_user_index(uint16_t x, uint16_t y)
{
    return ((uint32_t)(V5F_RGB_FB_HEIGHT - 1u - y) * V5F_RGB_FB_WIDTH) +
           (uint32_t)(V5F_RGB_FB_WIDTH - 1u - x);
}

static void gpha_blend_draw_background(void)
{
    uint16_t x;
    uint16_t y;
    uint16_t *bg = gpha_blend_bg();

    for(y = 0u; y < V5F_RGB_FB_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_RGB_FB_WIDTH; x++)
        {
            uint8_t band = (uint8_t)((x * 4u) / V5F_RGB_FB_WIDTH);
            uint8_t checker = (uint8_t)(((x / 32u) ^ (y / 32u)) & 1u);
            uint16_t color;

            if(band == 0u)
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(20u, 70u, 180u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(10u, 28u, 90u);
            }
            else if(band == 1u)
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(20u, 150u, 80u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(0u, 80u, 40u);
            }
            else if(band == 2u)
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(170u, 130u, 30u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(80u, 52u, 12u);
            }
            else
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(150u, 50u, 130u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(70u, 20u, 90u);
            }
            bg[gpha_user_index(x, y)] = color;
        }
    }
    memory_barrier();
}

static void gpha_blend_draw_foreground(uint16_t pos)
{
    uint32_t i;
    uint16_t x;
    uint16_t y;
    uint16_t *fg = gpha_blend_fg_argb4444();
    const uint16_t rect_w = 92u;
    const uint16_t rect_h = 58u;
    const uint16_t rect_y = 48u;

    for(i = 0u; i < V5F_RGB_FB_PIXELS; i++)
    {
        fg[i] = 0u;
    }

    for(y = 0u; y < rect_h; y++)
    {
        for(x = 0u; x < rect_w; x++)
        {
            uint16_t draw_x = (uint16_t)(pos + x);
            uint16_t draw_y = (uint16_t)(rect_y + y);
            uint8_t edge = (uint8_t)((x < 3u) || (y < 3u) ||
                                     (x >= (rect_w - 3u)) ||
                                     (y >= (rect_h - 3u)));
            uint16_t color = edge ? gpha_argb4444(0xFu, 0xFu, 0xFu, 0xFu) :
                                    gpha_argb4444(0xAu, 0xFu, 0x6u, 0x0u);

            if((draw_x < V5F_RGB_FB_WIDTH) && (draw_y < V5F_RGB_FB_HEIGHT))
            {
                fg[gpha_user_index(draw_x, draw_y)] = color;
            }
        }
    }
    memory_barrier();
}

static int gpha_blend_to_rgb565(void)
{
    return ch32h417_gpha_2d_blend_argb4444_over_rgb565(gpha_blend_fg_argb4444(),
                                                        gpha_blend_bg(),
                                                        rgb_fb(),
                                                        V5F_RGB_FB_WIDTH,
                                                        V5F_RGB_FB_HEIGHT);
}

static uint16_t gpha_blend_advance_position(uint16_t pos, uint8_t *forward)
{
    const uint16_t step = 6u;
    const uint16_t rect_w = 92u;
    const uint16_t max_pos = V5F_RGB_FB_WIDTH - rect_w;

    if(*forward != 0u)
    {
        if((uint16_t)(pos + step) >= max_pos)
        {
            *forward = 0u;
            return max_pos;
        }
        return (uint16_t)(pos + step);
    }

    if(pos <= step)
    {
        *forward = 1u;
        return 0u;
    }
    return (uint16_t)(pos - step);
}

static void run_gpha_blend_rgb565_test(void)
{
    uint16_t pos = 0u;
    uint8_t forward = 1u;
    int result;

    ch32h417_gpha_2d_init();
    gpha_blend_draw_background();

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        gpha_blend_draw_foreground(pos);
        result = gpha_blend_to_rgb565();
        if(result == 0)
        {
            g_v5f_hw_test_diag.gpha_ok_count++;
            pos = gpha_blend_advance_position(pos, &forward);
        }
        else
        {
            g_v5f_hw_test_diag.gpha_fail_count++;
            g_v5f_hw_test_diag.last_error = result;
            fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u));
        }

        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(16);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
#define FLASH_ASSET_BLOCKS             8u
#define FLASH_ASSET_START_BLOCK        (GD5F1G_BLOCK_COUNT - FLASH_ASSET_BLOCKS)
#define FLASH_ASSET_MANIFEST_OFFSET    0u
#define FLASH_ASSET_GRAY_CLUT_OFFSET   GD5F1G_PAGE_SIZE
#define FLASH_ASSET_GRAY_IMAGE_OFFSET  (FLASH_ASSET_GRAY_CLUT_OFFSET + GD5F1G_PAGE_SIZE)
#define FLASH_ASSET_PALETTE_CLUT_OFFSET \
    GD5F1G_L8_ASSET_ALIGN_PAGE_CONST(FLASH_ASSET_GRAY_IMAGE_OFFSET + V5F_LTDC_FLASH_ASSET_IMAGE_BYTES)
#define FLASH_ASSET_PALETTE_IMAGE_OFFSET (FLASH_ASSET_PALETTE_CLUT_OFFSET + GD5F1G_PAGE_SIZE)
#define FLASH_ASSET_TOTAL_BYTES \
    GD5F1G_L8_ASSET_ALIGN_PAGE_CONST(FLASH_ASSET_PALETTE_IMAGE_OFFSET + V5F_LTDC_FLASH_ASSET_IMAGE_BYTES)

#if FLASH_ASSET_TOTAL_BYTES > (FLASH_ASSET_BLOCKS * GD5F1G_BLOCK_SIZE)
#error V5F flash L8 asset package exceeds reserved SPI-NAND blocks.
#endif

static uint32_t flash_assets_checksum_buffer(const uint8_t *data, uint32_t length)
{
    return gd5f1g_l8_asset_fnv1a_buffer(data, length);
}

static void flash_assets_fill_page(uint8_t value)
{
    uint32_t i;

    for(i = 0u; i < GD5F1G_PAGE_SIZE; i++)
    {
        s_flash_page[i] = value;
    }
}

static void flash_assets_show_stage(uint8_t index)
{
    ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, index);
    memory_barrier();
    g_v5f_hw_test_diag.frame_count++;
}

static void flash_assets_fill_gray_clut(uint8_t *data)
{
    gd5f1g_l8_asset_fill_gray_clut(data, V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES);
}

static int flash_assets_program_linear(const gd5f1g_spi_bus_t *bus,
                                       uint32_t offset,
                                       const uint8_t *data,
                                       uint32_t length)
{
    return gd5f1g_l8_asset_program_linear(bus,
                                          FLASH_ASSET_START_BLOCK,
                                          offset,
                                          data,
                                          length);
}

static int flash_assets_read_linear(const gd5f1g_spi_bus_t *bus,
                                    uint32_t offset,
                                    uint8_t *data,
                                    uint32_t length)
{
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_read_linear(bus,
                                             FLASH_ASSET_START_BLOCK,
                                             offset,
                                             data,
                                             length,
                                             &status);
    g_v5f_hw_test_diag.flash_status = status;
    return result;
}

static int flash_assets_verify_linear(const gd5f1g_spi_bus_t *bus,
                                      uint32_t offset,
                                      uint32_t length,
                                      uint32_t expected_checksum)
{
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_verify_linear(bus,
                                               FLASH_ASSET_START_BLOCK,
                                               offset,
                                               length,
                                               expected_checksum,
                                               s_flash_page,
                                               GD5F1G_PAGE_SIZE,
                                               &status);
    g_v5f_hw_test_diag.flash_status = status;
    return result;
}

static int flash_assets_write_manifest(const gd5f1g_spi_bus_t *bus)
{
    int result;

    gd5f1g_l8_asset_manifest_init(&s_flash_manifest,
                                  V5F_LTDC_FLASH_ASSET_WIDTH,
                                  V5F_LTDC_FLASH_ASSET_HEIGHT,
                                  FLASH_ASSET_TOTAL_BYTES);
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          0u,
                                          GD5F1G_L8_ASSET_TYPE_GRAY_CLUT,
                                          FLASH_ASSET_GRAY_CLUT_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                          V5F_LTDC_FLASH_GRAY_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          1u,
                                          GD5F1G_L8_ASSET_TYPE_GRAY_IMAGE,
                                          FLASH_ASSET_GRAY_IMAGE_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                          V5F_LTDC_FLASH_GRAY_IMAGE_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          2u,
                                          GD5F1G_L8_ASSET_TYPE_PALETTE_CLUT,
                                          FLASH_ASSET_PALETTE_CLUT_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                          V5F_LTDC_FLASH_PALETTE_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          3u,
                                          GD5F1G_L8_ASSET_TYPE_PALETTE_IMAGE,
                                          FLASH_ASSET_PALETTE_IMAGE_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                          V5F_LTDC_FLASH_PALETTE_IMAGE_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    return gd5f1g_l8_asset_write_manifest(bus,
                                          FLASH_ASSET_START_BLOCK,
                                          &s_flash_manifest,
                                          s_flash_page,
                                          GD5F1G_PAGE_SIZE);
}

static int flash_assets_read_manifest(const gd5f1g_spi_bus_t *bus)
{
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_read_manifest(bus,
                                               FLASH_ASSET_START_BLOCK,
                                               V5F_LTDC_FLASH_ASSET_WIDTH,
                                               V5F_LTDC_FLASH_ASSET_HEIGHT,
                                               &s_flash_manifest,
                                               s_flash_page,
                                               GD5F1G_PAGE_SIZE,
                                               &status);
    g_v5f_hw_test_diag.flash_status = status;
    return result;
}

static int flash_assets_manifest_find(uint32_t type,
                                      uint32_t *offset_out,
                                      uint32_t *length_out,
                                      uint32_t *checksum_out)
{
    gd5f1g_l8_asset_entry_t entry;
    int result;

    if((offset_out == 0) || (length_out == 0) || (checksum_out == 0))
    {
        return GD5F1G_ERR_PARAM;
    }

    result = gd5f1g_l8_asset_manifest_find(&s_flash_manifest, type, &entry);
    if(result == GD5F1G_OK)
    {
        *offset_out = entry.offset;
        *length_out = entry.length;
        *checksum_out = entry.checksum;
    }
    return result;
}

static int flash_assets_check_blocks(const gd5f1g_spi_bus_t *bus)
{
    uint8_t marker = 0u;
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_check_blocks(bus,
                                              FLASH_ASSET_START_BLOCK,
                                              FLASH_ASSET_BLOCKS,
                                              &marker,
                                              &status);
    g_v5f_hw_test_diag.flash_bad_marker = marker;
    g_v5f_hw_test_diag.flash_bad_marker_status = status;
    return result;
}

static int flash_assets_erase_blocks(const gd5f1g_spi_bus_t *bus)
{
    return gd5f1g_l8_asset_erase_blocks(bus,
                                        FLASH_ASSET_START_BLOCK,
                                        FLASH_ASSET_BLOCKS);
}

static int flash_assets_decode_gray(void)
{
    uint32_t size =
        (uint32_t)(v5f_ltdc_flash_gray_lzss_end - v5f_ltdc_flash_gray_lzss);
    int result;

    if(size != V5F_LTDC_FLASH_GRAY_LZSS_BYTES)
    {
        return -105;
    }

    result = gd5f1g_l8_asset_lzss_decode(v5f_ltdc_flash_gray_lzss,
                                          size,
                                          l8_fb(),
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    gd5f1g_l8_asset_unfilter_left(l8_fb(),
                                  V5F_LTDC_FLASH_ASSET_WIDTH,
                                  V5F_LTDC_FLASH_ASSET_HEIGHT);
    if(flash_assets_checksum_buffer(l8_fb(), V5F_LTDC_FLASH_ASSET_IMAGE_BYTES) !=
       V5F_LTDC_FLASH_GRAY_IMAGE_FNV)
    {
        return GD5F1G_ERR_VERIFY;
    }
    return GD5F1G_OK;
}

static int flash_assets_decode_palette(void)
{
    uint32_t size =
        (uint32_t)(v5f_ltdc_flash_palette_lzss_end - v5f_ltdc_flash_palette_lzss);
    int result;

    if(size != V5F_LTDC_FLASH_PALETTE_LZSS_BYTES)
    {
        return -106;
    }

    result = gd5f1g_l8_asset_lzss_decode(v5f_ltdc_flash_palette_lzss,
                                          size,
                                          l8_fb(),
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if(flash_assets_checksum_buffer(l8_fb(), V5F_LTDC_FLASH_ASSET_IMAGE_BYTES) !=
       V5F_LTDC_FLASH_PALETTE_IMAGE_FNV)
    {
        return GD5F1G_ERR_VERIFY;
    }
    return GD5F1G_OK;
}

static int flash_assets_write_all(const gd5f1g_spi_bus_t *bus)
{
    uint32_t palette_clut_size =
        (uint32_t)(v5f_ltdc_flash_palette_clut_rgb888_end -
                   v5f_ltdc_flash_palette_clut_rgb888);
    int result;

    if(palette_clut_size != V5F_LTDC_FLASH_ASSET_CLUT_BYTES)
    {
        return -107;
    }

    flash_assets_show_stage(24u);
    flash_assets_fill_page(0xFFu);
    flash_assets_fill_gray_clut(s_flash_page);
    if(flash_assets_checksum_buffer(s_flash_page, V5F_LTDC_FLASH_ASSET_CLUT_BYTES) !=
       V5F_LTDC_FLASH_GRAY_CLUT_FNV)
    {
        return GD5F1G_ERR_VERIFY;
    }
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_GRAY_CLUT_OFFSET,
                                         s_flash_page,
                                         V5F_LTDC_FLASH_ASSET_CLUT_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    flash_assets_show_stage(48u);
    result = flash_assets_decode_gray();
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_GRAY_IMAGE_OFFSET,
                                         l8_fb(),
                                         V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    flash_assets_show_stage(72u);
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_PALETTE_CLUT_OFFSET,
                                         v5f_ltdc_flash_palette_clut_rgb888,
                                         V5F_LTDC_FLASH_ASSET_CLUT_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    flash_assets_show_stage(96u);
    result = flash_assets_decode_palette();
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_PALETTE_IMAGE_OFFSET,
                                         l8_fb(),
                                         V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    return flash_assets_write_manifest(bus);
}

static int flash_assets_verify_all(const gd5f1g_spi_bus_t *bus)
{
    int result;

    result = flash_assets_verify_linear(bus,
                                        FLASH_ASSET_GRAY_CLUT_OFFSET,
                                        V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                        V5F_LTDC_FLASH_GRAY_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_verify_linear(bus,
                                        FLASH_ASSET_GRAY_IMAGE_OFFSET,
                                        V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                        V5F_LTDC_FLASH_GRAY_IMAGE_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_verify_linear(bus,
                                        FLASH_ASSET_PALETTE_CLUT_OFFSET,
                                        V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                        V5F_LTDC_FLASH_PALETTE_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    return flash_assets_verify_linear(bus,
                                      FLASH_ASSET_PALETTE_IMAGE_OFFSET,
                                      V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                      V5F_LTDC_FLASH_PALETTE_IMAGE_FNV);
}

static int flash_assets_display_palette_from_flash(const gd5f1g_spi_bus_t *bus)
{
    uint32_t clut_offset = 0u;
    uint32_t clut_length = 0u;
    uint32_t clut_checksum = 0u;
    uint32_t image_offset = 0u;
    uint32_t image_length = 0u;
    uint32_t image_checksum = 0u;
    int result;

    result = flash_assets_read_manifest(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_manifest_find(GD5F1G_L8_ASSET_TYPE_PALETTE_CLUT,
                                        &clut_offset,
                                        &clut_length,
                                        &clut_checksum);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_manifest_find(GD5F1G_L8_ASSET_TYPE_PALETTE_IMAGE,
                                        &image_offset,
                                        &image_length,
                                        &image_checksum);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if((clut_length != V5F_LTDC_FLASH_ASSET_CLUT_BYTES) ||
       (image_length != V5F_LTDC_FLASH_ASSET_IMAGE_BYTES))
    {
        return GD5F1G_ERR_VERIFY;
    }

    result = flash_assets_read_linear(bus, clut_offset, s_flash_page, clut_length);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if(flash_assets_checksum_buffer(s_flash_page, clut_length) != clut_checksum)
    {
        return GD5F1G_ERR_VERIFY;
    }

    result = flash_assets_read_linear(bus, image_offset, l8_fb(), image_length);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if(flash_assets_checksum_buffer(l8_fb(), image_length) != image_checksum)
    {
        return GD5F1G_ERR_VERIFY;
    }

    (void)ch32h417_ltdc_rgb_layer1_load_clut_rgb888(s_flash_page,
                                                    V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES);
    memory_barrier();
    return GD5F1G_OK;
}

static int flash_assets_prepare_bus(ch32h417_gd5f1g_spi1_context_t *context,
                                    gd5f1g_spi_bus_t *bus)
{
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;
    int result;

    ch32h417_gd5f1g_spi1_init(context, bus);
    result = gd5f1g_read_id(bus, &manufacturer_id, &device_id);
    if(result != GD5F1G_OK)
    {
        ch32h417_gd5f1g_spi1_set_mode(context, CH32H417_GD5F1G_SPI_MODE0);
        result = gd5f1g_read_id(bus, &manufacturer_id, &device_id);
    }
    g_v5f_hw_test_diag.flash_manufacturer_id = manufacturer_id;
    g_v5f_hw_test_diag.flash_device_id = device_id;
    if(result != GD5F1G_OK)
    {
        return result;
    }

    result = gd5f1g_reset(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    return flash_assets_check_blocks(bus);
}

static void run_flash_l8_assets_test(void)
{
    ch32h417_gd5f1g_spi1_context_t context;
    gd5f1g_spi_bus_t bus;
    int result;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    flash_assets_show_stage(16u);
    result = flash_assets_prepare_bus(&context, &bus);
    if(result == GD5F1G_OK)
    {
        flash_assets_show_stage(32u);
        result = flash_assets_erase_blocks(&bus);
    }
    if(result == GD5F1G_OK)
    {
        result = flash_assets_write_all(&bus);
    }
    if(result == GD5F1G_OK)
    {
        flash_assets_show_stage(128u);
        result = flash_assets_read_manifest(&bus);
    }
    if(result == GD5F1G_OK)
    {
        result = flash_assets_verify_all(&bus);
    }
    if(result == GD5F1G_OK)
    {
        flash_assets_show_stage(192u);
        result = flash_assets_display_palette_from_flash(&bus);
    }

    g_v5f_hw_test_diag.spi_timeout_count = context.timeout_count;
    if((result == GD5F1G_OK) && (context.timeout_count == 0u))
    {
        g_v5f_hw_test_diag.gpha_ok_count++;
    }
    else
    {
        g_v5f_hw_test_diag.gpha_fail_count++;
        g_v5f_hw_test_diag.last_error =
            (result != GD5F1G_OK) ? result : GD5F1G_ERR_TIMEOUT;
        flash_assets_show_stage(255u);
    }

    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH
static void draw_byte_bits(uint16_t x, uint16_t y, uint8_t value)
{
    uint8_t bit;

    for(bit = 0u; bit < 8u; bit++)
    {
        fb_fill_user_rect_rgb565((uint16_t)(x + bit * 12u),
                                 y,
                                 9u,
                                 16u,
                                 ((value & (uint8_t)(0x80u >> bit)) != 0u) ?
                                     ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u) :
                                     ch32h417_ltdc_rgb_pack_rgb565(32u, 32u, 40u));
    }
}

static void draw_flash_report(int pass)
{
    uint16_t ok = ch32h417_ltdc_rgb_pack_rgb565(0u, 180u, 80u);
    uint16_t fail = ch32h417_ltdc_rgb_pack_rgb565(255u, 32u, 16u);
    uint16_t warn = ch32h417_ltdc_rgb_pack_rgb565(255u, 180u, 0u);
    uint16_t base = pass ? ok : fail;

    fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(4u, 6u, 10u));
    fb_fill_user_rect_rgb565(0u, 0u, V5F_RGB_FB_WIDTH, 30u, base);
    fb_fill_user_rect_rgb565(0u, 42u, V5F_RGB_FB_WIDTH, 20u,
                             (g_v5f_hw_test_diag.spi_timeout_count == 0u) ? ok : warn);
    fb_fill_user_rect_rgb565(0u, 74u, V5F_RGB_FB_WIDTH, 20u,
                             (g_v5f_hw_test_diag.flash_bad_marker == 0xFFu) ? ok : warn);
    draw_byte_bits(28u, 112u, g_v5f_hw_test_diag.flash_manufacturer_id);
    draw_byte_bits(150u, 112u, g_v5f_hw_test_diag.flash_device_id);
    fb_draw_border_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));
}

static void run_flash_test(void)
{
    ch32h417_gd5f1g_spi1_context_t context;
    gd5f1g_spi_bus_t bus;
    gd5f1g_info_t info = {0};
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;
    uint8_t marker = 0u;
    uint8_t marker_status = 0u;
    int id_result;
    int reset_result;
    int info_result;
    int marker_result;
    int pass;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    ch32h417_gd5f1g_spi1_init(&context, &bus);

    id_result = gd5f1g_read_id(&bus, &manufacturer_id, &device_id);
    reset_result = gd5f1g_reset(&bus);
    info_result = gd5f1g_read_info(&bus, &info);
    marker_result = gd5f1g_read_bad_block_marker(&bus,
                                                 GD5F1G_BLOCK_COUNT - 1u,
                                                 &marker,
                                                 &marker_status);

    g_v5f_hw_test_diag.flash_manufacturer_id = manufacturer_id;
    g_v5f_hw_test_diag.flash_device_id = device_id;
    g_v5f_hw_test_diag.flash_protection = info.protection;
    g_v5f_hw_test_diag.flash_config = info.config;
    g_v5f_hw_test_diag.flash_status = info.status;
    g_v5f_hw_test_diag.flash_status2 = info.status2;
    g_v5f_hw_test_diag.flash_bad_marker = marker;
    g_v5f_hw_test_diag.flash_bad_marker_status = marker_status;
    g_v5f_hw_test_diag.spi_timeout_count = context.timeout_count;

    pass = (id_result == GD5F1G_OK) &&
           (reset_result == GD5F1G_OK) &&
           (info_result == GD5F1G_OK) &&
           (marker_result == GD5F1G_OK) &&
           (manufacturer_id == GD5F1G_MANUFACTURER_ID) &&
           (device_id == GD5F1G_DEVICE_ID_3V) &&
           (context.timeout_count == 0u);
    if(!pass)
    {
        g_v5f_hw_test_diag.last_error =
            (id_result != GD5F1G_OK) ? id_result :
            (reset_result != GD5F1G_OK) ? reset_result :
            (info_result != GD5F1G_OK) ? info_result :
            marker_result;
    }

    while(1)
    {
        draw_flash_report(pass);
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

static void fail_forever(int error)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
    g_v5f_hw_test_diag.last_error = error;
    while(1)
    {
        rt_thread_mdelay(1000);
    }
}

static void v5f_hw_thread_entry(void *parameter)
{
    int result = CH32H417_LTDC_RGB_OK;
    (void)parameter;

    g_v5f_hw_test_diag.mode = APP_V5F_HW_TEST;
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_BOOT;
    rt_kprintf("V5F hardware test: %s\n", APP_V5F_HW_TEST_NAME);

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
    if(!ltdc_gray_image_valid())
    {
        fail_forever(-10);
    }
    fb_load_ltdc_gray_image();
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
    if(!ltdc_palette_image_valid())
    {
        fail_forever(-11);
    }
    fb_load_ltdc_palette_image();
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
    ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, 0u);
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
    ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, 0u);
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
    fb_draw_rgb565_channel_diag();
    result = lcd_start_rgb565_window();
#elif (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_R2M_FILL) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_TICK_DIAG)
    result = lcd_start_rgb565_window();
#endif

    if(result != CH32H417_LTDC_RGB_OK)
    {
        fail_forever(result);
    }
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_LCD_READY;

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
    run_ltdc_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
    run_ltdc_l8_palette_image_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
    run_ltdc_rgb565_diag_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_R2M_FILL
    run_gpha_r2m_fill_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
    run_gpha_pfc_l8_rgb565_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565
    run_gpha_blend_rgb565_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
    run_gpha_l8_ltdc_fullscreen_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH
    run_flash_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
    run_flash_l8_assets_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_TICK_DIAG
    run_tick_diag_test();
#else
    while(1)
    {
        rt_thread_mdelay(1000);
    }
#endif
}

int v5f_hw_test_start(void)
{
    rt_err_t err;

    err = rt_thread_init(&s_test_thread,
                         "v5f_hw",
                         v5f_hw_thread_entry,
                         RT_NULL,
                         s_test_thread_stack,
                         sizeof(s_test_thread_stack),
                         18,
                         10);
    if(err != RT_EOK)
    {
        return (int)err;
    }

    return (int)rt_thread_startup(&s_test_thread);
}
