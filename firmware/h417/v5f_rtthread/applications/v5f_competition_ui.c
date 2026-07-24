/* Official 2026 competition home frame for the 800x480 V5F display. */

#include "v5f_competition_ui.h"

#include <stdint.h>

#include "v5f_competition_assets.h"
#include "v5f_display.h"
#include "v5f_ui_theme.h"

#define COMPETITION_LOGO_Y    70u
#define COMPETITION_SLOGAN_Y  296u

static void ui_draw_mask(uint16_t x,
                         uint16_t y,
                         uint16_t width,
                         uint16_t height,
                         uint16_t stride,
                         const uint8_t *bits,
                         uint8_t color)
{
    uint16_t row;

    for(row = 0u; row < height; row++)
    {
        uint16_t column = 0u;

        while(column < width)
        {
            uint16_t run_start;

            while((column < width) &&
                  ((bits[(row * stride) + (column / 8u)] &
                    (uint8_t)(0x80u >> (column & 7u))) == 0u))
            {
                column++;
            }
            run_start = column;
            while((column < width) &&
                  ((bits[(row * stride) + (column / 8u)] &
                    (uint8_t)(0x80u >> (column & 7u))) != 0u))
            {
                column++;
            }
            if(column > run_start)
            {
                v5f_display_fill_rect((uint16_t)(x + run_start),
                                      (uint16_t)(y + row),
                                      (uint16_t)(column - run_start),
                                      1u,
                                      color);
            }
        }
    }
}

void v5f_competition_ui_draw(void)
{
    const uint16_t logo_x =
        (uint16_t)((V5F_DISPLAY_WIDTH - V5F_COMPETITION_LOGO_WIDTH) / 2u);
    const uint16_t slogan_x =
        (uint16_t)((V5F_DISPLAY_WIDTH - V5F_COMPETITION_SLOGAN_WIDTH) / 2u);

    v5f_display_fill(V5F_UI_COLOR_WHITE);
    ui_draw_mask(logo_x,
                 COMPETITION_LOGO_Y,
                 V5F_COMPETITION_LOGO_WIDTH,
                 V5F_COMPETITION_LOGO_HEIGHT,
                 V5F_COMPETITION_LOGO_STRIDE,
                 s_v5f_competition_logo_bits,
                 V5F_UI_COLOR_COMPETITION_BLUE);
    ui_draw_mask(slogan_x,
                 COMPETITION_SLOGAN_Y,
                 V5F_COMPETITION_SLOGAN_WIDTH,
                 V5F_COMPETITION_SLOGAN_HEIGHT,
                 V5F_COMPETITION_SLOGAN_STRIDE,
                 s_v5f_competition_slogan_bits,
                 V5F_UI_COLOR_COMPETITION_BLUE);
    v5f_display_present();
}
