#include "v5f_display.h"

#include <string.h>

#include <rtthread.h>

#include "ch32h417_ltdc_rgb.h"
#include "v5f_competition_ui.h"
#include "v5f_ui_theme.h"

#define V5F_DISPLAY_LCD_FB_REGION_BYTES (384u * 1024u)
#define V5F_DISPLAY_POWER_SETTLE_MS      550u
#define V5F_DISPLAY_CLUT_SETTLE_MS       100u
#define V5F_DISPLAY_SCAN_TIMEOUT_MS      100u
#define V5F_DISPLAY_SCAN_MIN_CHANGES     3u
#define V5F_DISPLAY_RELOAD_TIMEOUT_MS    100u

#define V5F_DISPLAY_COLOR_BACKGROUND     0u

#if V5F_DISPLAY_FRAMEBUFFER_BYTES > V5F_DISPLAY_LCD_FB_REGION_BYTES
#error V5F L8 framebuffer exceeds the linker-reserved LCD_FB region.
#endif

static uint8_t s_framebuffer[V5F_DISPLAY_FRAMEBUFFER_BYTES]
    __attribute__((section(".lcd_fb"), aligned(64)));
static volatile uint8_t s_ui_active;

volatile v5f_display_diag_t g_v5f_display_diag;

uint8_t *v5f_display_framebuffer(void)
{
    return s_framebuffer;
}

void v5f_display_present(void)
{
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

void v5f_display_fill(uint8_t color_index)
{
    memset(s_framebuffer, color_index, sizeof(s_framebuffer));
}

void v5f_display_fill_rect(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint8_t color_index)
{
    uint16_t row;

    if((width == 0u) || (height == 0u) ||
       (x >= V5F_DISPLAY_WIDTH) || (y >= V5F_DISPLAY_HEIGHT))
    {
        return;
    }
    if(((uint32_t)x + width) > V5F_DISPLAY_WIDTH)
    {
        width = (uint16_t)(V5F_DISPLAY_WIDTH - x);
    }
    if(((uint32_t)y + height) > V5F_DISPLAY_HEIGHT)
    {
        height = (uint16_t)(V5F_DISPLAY_HEIGHT - y);
    }

    /* The mounted panel is rotated 180 degrees relative to framebuffer order. */
    for(row = 0u; row < height; row++)
    {
        uint32_t physical_y =
            (uint32_t)V5F_DISPLAY_HEIGHT - 1u - ((uint32_t)y + row);
        uint32_t physical_x =
            (uint32_t)V5F_DISPLAY_WIDTH - ((uint32_t)x + width);
        uint8_t *destination =
            &s_framebuffer[(physical_y * V5F_DISPLAY_WIDTH) + physical_x];

        memset(destination, color_index, width);
    }
}

static void prepare_initial_frame(void)
{
    /* Backlight stays off until the product UI has replaced this blank frame. */
    v5f_display_fill(V5F_DISPLAY_COLOR_BACKGROUND);
    v5f_display_present();
}

static int display_registers_valid(void)
{
    ch32h417_ltdc_rgb_snapshot_t snapshot;

    ch32h417_ltdc_rgb_snapshot(&snapshot);
    g_v5f_display_diag.last_cpsr = snapshot.cpsr;
    g_v5f_display_diag.last_cdsr = snapshot.cdsr;
    g_v5f_display_diag.last_layer_cr = LTDC_Layer1->CR;
    g_v5f_display_diag.last_layer_pfcr = LTDC_Layer1->PFCR;

    if((snapshot.sscr != 0x00070003u) ||
       (snapshot.bpcr != 0x000f0017u) ||
       (snapshot.awcr != 0x032f01f7u) ||
       (snapshot.twcr != 0x03610207u) ||
       (snapshot.layer_whpcr != 0x032f0010u) ||
       (snapshot.layer_wvpcr != 0x01f70018u) ||
       (snapshot.layer_cfbar != (uint32_t)s_framebuffer) ||
       (snapshot.layer_cfblr != 0x0320033Fu) ||
       (snapshot.layer_cfblnr != V5F_DISPLAY_HEIGHT) ||
       ((LTDC_Layer1->PFCR & LTDC_PFCR_PF) != LTDC_Pixelformat_L8) ||
       ((snapshot.gcr & LTDC_GCR_LTDCEN) == 0u) ||
       ((LTDC_Layer1->CR & LTDC_CR_LEN) == 0u))
    {
        return V5F_DISPLAY_ERR_REGISTERS;
    }

    return V5F_DISPLAY_OK;
}

static int wait_for_scanout(void)
{
    uint32_t previous = LTDC->CPSR;
    uint32_t elapsed;
    uint32_t changes = 0u;

    for(elapsed = 0u; elapsed < V5F_DISPLAY_SCAN_TIMEOUT_MS; elapsed++)
    {
        uint32_t current;

        rt_thread_mdelay(1u);
        current = LTDC->CPSR;
        if(current != previous)
        {
            changes++;
            previous = current;
            if(changes >= V5F_DISPLAY_SCAN_MIN_CHANGES)
            {
                g_v5f_display_diag.scan_changes += changes;
                g_v5f_display_diag.last_cpsr = current;
                g_v5f_display_diag.last_cdsr = LTDC->CDSR;
                return V5F_DISPLAY_OK;
            }
        }
    }

    g_v5f_display_diag.scan_changes += changes;
    g_v5f_display_diag.last_cpsr = LTDC->CPSR;
    g_v5f_display_diag.last_cdsr = LTDC->CDSR;
    return V5F_DISPLAY_ERR_SCANOUT;
}

static int reload_at_vertical_blank(void)
{
    rt_tick_t start = rt_tick_get();
    rt_tick_t timeout = rt_tick_from_millisecond(
        (rt_int32_t)V5F_DISPLAY_RELOAD_TIMEOUT_MS);

    if(timeout == 0u)
    {
        timeout = 1u;
    }

    LTDC_ReloadConfig(LTDC_VBReload);
    while((LTDC->SRCR & LTDC_SRCR_VBR) != 0u)
    {
        if((rt_tick_t)(rt_tick_get() - start) >= timeout)
        {
            return V5F_DISPLAY_ERR_MODE_SWITCH;
        }
        rt_thread_mdelay(1u);
    }

    return V5F_DISPLAY_OK;
}

int v5f_display_init(void)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};
    int result;

    if(g_v5f_display_diag.initialized != 0u)
    {
        return g_v5f_display_diag.last_error;
    }

    memset((void *)&g_v5f_display_diag, 0, sizeof(g_v5f_display_diag));
    prepare_initial_frame();

    ch32h417_lcd_rgb_control_init();
    ch32h417_lcd_rgb_disp_enable(1u);
    rt_thread_mdelay(V5F_DISPLAY_POWER_SETTLE_MS);

    layer.width = V5F_DISPLAY_WIDTH;
    layer.height = V5F_DISPLAY_HEIGHT;
    layer.pixel_format = LTDC_Pixelformat_L8;
    layer.framebuffer = (uint32_t)s_framebuffer;
    layer.line_pitch = V5F_DISPLAY_WIDTH;

    result = ch32h417_ltdc_rgb_start_layer1(&ch32h417_ltdc_rgb_panel_800x480,
                                            &layer,
                                            &black);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        g_v5f_display_diag.last_error = V5F_DISPLAY_ERR_LTDC;
        return V5F_DISPLAY_ERR_LTDC;
    }

    result = display_registers_valid();
    if(result != V5F_DISPLAY_OK)
    {
        g_v5f_display_diag.last_error = result;
        return result;
    }
    result = wait_for_scanout();
    if(result != V5F_DISPLAY_OK)
    {
        g_v5f_display_diag.last_error = result;
        return result;
    }

    /* Hardware validation showed that L8 CLUT writes must follow layer start. */
    rt_thread_mdelay(V5F_DISPLAY_CLUT_SETTLE_MS);
    result = ch32h417_ltdc_rgb_layer1_load_clut_rgb888(
        v5f_ui_clut_rgb888(),
        CH32H417_LTDC_RGB_CLUT_ENTRIES);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        g_v5f_display_diag.last_error = V5F_DISPLAY_ERR_CLUT;
        return V5F_DISPLAY_ERR_CLUT;
    }
    g_v5f_display_diag.last_layer_cr = LTDC_Layer1->CR;
    g_v5f_display_diag.last_layer_pfcr = LTDC_Layer1->PFCR;
    if(((LTDC_Layer1->CR & (LTDC_CR_LEN | LTDC_CR_CLUTEN)) !=
        (LTDC_CR_LEN | LTDC_CR_CLUTEN)) ||
       ((LTDC_Layer1->PFCR & LTDC_PFCR_PF) != LTDC_Pixelformat_L8))
    {
        g_v5f_display_diag.last_error = V5F_DISPLAY_ERR_CLUT;
        return V5F_DISPLAY_ERR_CLUT;
    }

    v5f_competition_ui_draw();
    result = wait_for_scanout();
    if(result != V5F_DISPLAY_OK)
    {
        g_v5f_display_diag.last_error = result;
        return result;
    }

    ch32h417_lcd_rgb_backlight_enable(1u);
    s_ui_active = 1u;
    g_v5f_display_diag.initialized = 1u;
    g_v5f_display_diag.last_error = V5F_DISPLAY_OK;
    return V5F_DISPLAY_OK;
}

int v5f_display_deactivate(void)
{
    int result;

    if((g_v5f_display_diag.initialized == 0u) ||
       (g_v5f_display_diag.last_error != V5F_DISPLAY_OK))
    {
        return V5F_DISPLAY_ERR_MODE_SWITCH;
    }
    if(s_ui_active == 0u)
    {
        return V5F_DISPLAY_OK;
    }

    /* Block product UI writers before Layer1 is handed to the player. */
    s_ui_active = 0u;
    ch32h417_ltdc_rgb_layer1_clut_enable(0u);
    ch32h417_ltdc_rgb_layer1_enable(0u);
    result = reload_at_vertical_blank();
    if(result != V5F_DISPLAY_OK)
    {
        g_v5f_display_diag.last_error = result;
    }
    return result;
}

int v5f_display_activate_ui(void)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    int result;

    if(g_v5f_display_diag.initialized == 0u)
    {
        return V5F_DISPLAY_ERR_MODE_SWITCH;
    }

    /* Build the home screen while Layer1 still belongs to neither renderer. */
    s_ui_active = 0u;
    v5f_competition_ui_draw();

    ch32h417_ltdc_rgb_layer1_enable(0u);
    ch32h417_ltdc_rgb_layer1_clut_enable(0u);

    layer.width = V5F_DISPLAY_WIDTH;
    layer.height = V5F_DISPLAY_HEIGHT;
    layer.pixel_format = LTDC_Pixelformat_L8;
    layer.framebuffer = (uint32_t)s_framebuffer;
    layer.line_pitch = V5F_DISPLAY_WIDTH;
    result = ch32h417_ltdc_rgb_layer1_config(
        &ch32h417_ltdc_rgb_panel_800x480,
        &layer);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        g_v5f_display_diag.last_error = V5F_DISPLAY_ERR_LTDC;
        return V5F_DISPLAY_ERR_LTDC;
    }

    /*
     * Preserve the hardware-qualified order used at first boot: establish a
     * live L8 scanout, let it settle, and only then rewrite the H417 CLUT.
     * The alternate renderer reinitializes LTDC and therefore cannot be
     * assumed to leave the product palette intact.
     */
    ch32h417_ltdc_rgb_layer1_enable(1u);
    ch32h417_ltdc_rgb_enable(1u);
    result = reload_at_vertical_blank();
    if(result != V5F_DISPLAY_OK)
    {
        g_v5f_display_diag.last_error = result;
        return result;
    }

    result = display_registers_valid();
    if(result == V5F_DISPLAY_OK)
    {
        result = wait_for_scanout();
    }
    if(result != V5F_DISPLAY_OK)
    {
        g_v5f_display_diag.last_error = result;
        return result;
    }

    rt_thread_mdelay(V5F_DISPLAY_CLUT_SETTLE_MS);
    result = ch32h417_ltdc_rgb_layer1_load_clut_rgb888(
        v5f_ui_clut_rgb888(),
        CH32H417_LTDC_RGB_CLUT_ENTRIES);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        g_v5f_display_diag.last_error = V5F_DISPLAY_ERR_CLUT;
        return V5F_DISPLAY_ERR_CLUT;
    }
    g_v5f_display_diag.last_layer_cr = LTDC_Layer1->CR;
    g_v5f_display_diag.last_layer_pfcr = LTDC_Layer1->PFCR;
    if(((LTDC_Layer1->CR & (LTDC_CR_LEN | LTDC_CR_CLUTEN)) !=
        (LTDC_CR_LEN | LTDC_CR_CLUTEN)) ||
       ((LTDC_Layer1->PFCR & LTDC_PFCR_PF) != LTDC_Pixelformat_L8))
    {
        g_v5f_display_diag.last_error = V5F_DISPLAY_ERR_CLUT;
        return V5F_DISPLAY_ERR_CLUT;
    }
    result = wait_for_scanout();
    if(result != V5F_DISPLAY_OK)
    {
        g_v5f_display_diag.last_error = result;
        return result;
    }

    s_ui_active = 1u;
    g_v5f_display_diag.last_error = V5F_DISPLAY_OK;
    return V5F_DISPLAY_OK;
}

uint8_t v5f_display_is_ready(void)
{
    return (uint8_t)((g_v5f_display_diag.initialized != 0u) &&
                     (g_v5f_display_diag.last_error == V5F_DISPLAY_OK) &&
                     (s_ui_active != 0u));
}
