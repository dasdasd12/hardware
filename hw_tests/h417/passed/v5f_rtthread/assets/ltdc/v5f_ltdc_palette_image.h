#ifndef V5F_LTDC_PALETTE_IMAGE_H
#define V5F_LTDC_PALETTE_IMAGE_H

#include <stdint.h>

#define V5F_LTDC_PALETTE_IMAGE_WIDTH  800u
#define V5F_LTDC_PALETTE_IMAGE_HEIGHT 480u
#define V5F_LTDC_PALETTE_IMAGE_BYTES  \
    (V5F_LTDC_PALETTE_IMAGE_WIDTH * V5F_LTDC_PALETTE_IMAGE_HEIGHT)
#define V5F_LTDC_PALETTE_CLUT_ENTRIES 256u
#define V5F_LTDC_PALETTE_CLUT_BYTES   (V5F_LTDC_PALETTE_CLUT_ENTRIES * 3u)

extern const uint8_t v5f_ltdc_palette_800x480[];
extern const uint8_t v5f_ltdc_palette_800x480_end[];
extern const uint8_t v5f_ltdc_palette_800x480_clut_rgb888[];
extern const uint8_t v5f_ltdc_palette_800x480_clut_rgb888_end[];

#endif
