#ifndef V5F_DEFAULT_UI_H
#define V5F_DEFAULT_UI_H

#include <stdint.h>

/*
 * Static product boot image derived from the validated Claude Code-style
 * LTDC hardware-test welcome frame.
 */
const uint8_t *v5f_default_ui_clut_rgb888(void);
void v5f_default_ui_draw(void);

#endif /* V5F_DEFAULT_UI_H */
