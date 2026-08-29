#ifndef V5F_DISPLAY_H
#define V5F_DISPLAY_H

#include <stdint.h>

/*
 * Product-side display service for the board's 800x480 RGB panel.
 *
 * The LTDC layer scans an L8 framebuffer from the linker-owned .lcd_fb
 * region.  Drawing coordinates use the user's view; the service applies the
 * panel's physical 180-degree mounting rotation when it writes memory.
 */

#define V5F_DISPLAY_WIDTH             800u
#define V5F_DISPLAY_HEIGHT            480u
#define V5F_DISPLAY_FRAMEBUFFER_BYTES \
    (V5F_DISPLAY_WIDTH * V5F_DISPLAY_HEIGHT)

#define V5F_DISPLAY_OK                0
#define V5F_DISPLAY_ERR_LTDC          (-1)
#define V5F_DISPLAY_ERR_CLUT          (-2)
#define V5F_DISPLAY_ERR_REGISTERS     (-3)
#define V5F_DISPLAY_ERR_SCANOUT       (-4)
#define V5F_DISPLAY_ERR_MODE_SWITCH   (-5)

typedef struct
{
    volatile uint32_t initialized;
    volatile int32_t last_error;
    volatile uint32_t scan_changes;
    volatile uint32_t last_cpsr;
    volatile uint32_t last_cdsr;
    volatile uint32_t last_layer_cr;
    volatile uint32_t last_layer_pfcr;
} v5f_display_diag_t;

extern volatile v5f_display_diag_t g_v5f_display_diag;

int v5f_display_init(void);
uint8_t v5f_display_is_ready(void);
uint8_t *v5f_display_framebuffer(void);

/*
 * Hand Layer1 exclusively to an alternate renderer and restore the product
 * L8 UI afterwards.  These operations leave the panel, DISP and backlight
 * running; only the LTDC Layer1 source/format ownership changes.
 */
int v5f_display_deactivate(void);
int v5f_display_activate_ui(void);

void v5f_display_fill(uint8_t color_index);
void v5f_display_fill_rect(uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height,
                           uint8_t color_index);
void v5f_display_present(void);

#endif /* V5F_DISPLAY_H */
