#ifndef V5F_UI_THEME_H
#define V5F_UI_THEME_H

#include <stdint.h>

#define V5F_UI_COLOR_BG                0u
#define V5F_UI_COLOR_WHITE             1u
#define V5F_UI_COLOR_BLUE              2u
#define V5F_UI_COLOR_ORANGE            3u
#define V5F_UI_COLOR_GRAY              4u
#define V5F_UI_COLOR_BORDER            5u
#define V5F_UI_COLOR_COMPETITION_BLUE  6u

const uint8_t *v5f_ui_clut_rgb888(void);

#endif /* V5F_UI_THEME_H */
