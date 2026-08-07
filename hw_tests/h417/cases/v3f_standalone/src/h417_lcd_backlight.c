#include "h417_common.h"
#include "ch32h417_ltdc_rgb.h"

#define BACKLIGHT_DISP_DELAY     55000000u

enum
{
    H417_ITEM_BACKLIGHT_GPIO = 51,
    H417_ITEM_BACKLIGHT_DISP = 52,
    H417_ITEM_BACKLIGHT_ON = 53
};

void h417_lcd_backlight_run(void)
{
    ch32h417_lcd_rgb_control_init();
    ch32h417_lcd_rgb_disp_enable(1u);
    h417_status_pass(H417_ITEM_BACKLIGHT_GPIO);

    h417_status_phase(50u, H417_ITEM_BACKLIGHT_DISP);
    h417_delay_cycles(BACKLIGHT_DISP_DELAY);

    ch32h417_lcd_rgb_backlight_enable(1u);
    h417_status_pass(H417_ITEM_BACKLIGHT_ON);

    while(1)
    {
        g_h417_status.cycle++;
        h417_status_phase(51u, H417_ITEM_BACKLIGHT_ON);
        h417_delay_cycles(1000000u);
    }
}
