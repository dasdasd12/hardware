#include "v5f_ui_theme.h"

#include "ch32h417_ltdc_rgb.h"

/* Shared L8 palette for the home, Claude, and approval frames. */
static const uint8_t s_ui_clut_rgb888[
    CH32H417_LTDC_RGB_CLUT_ENTRIES * 3u] = {
    0x0Cu, 0x0Cu, 0x0Cu, /* 0 background, near black */
    0xFFu, 0xFFu, 0xFFu, /* 1 primary text / competition background */
    0x46u, 0x96u, 0xFFu, /* 2 selected option / RUNNING */
    0xD7u, 0x77u, 0x57u, /* 3 Claude mascot and bullet */
    0x9Au, 0x9Au, 0x9Au, /* 4 secondary text / DONE */
    0x5Au, 0x5Au, 0x5Au, /* 5 dialog border */
    0x1Fu, 0x4Eu, 0x79u, /* 6 official 2026 competition artwork */
};

const uint8_t *v5f_ui_clut_rgb888(void)
{
    return s_ui_clut_rgb888;
}
