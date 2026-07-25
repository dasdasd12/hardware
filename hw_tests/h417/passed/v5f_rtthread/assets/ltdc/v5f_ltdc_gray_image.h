#ifndef V5F_LTDC_GRAY_IMAGE_H
#define V5F_LTDC_GRAY_IMAGE_H

#include <stdint.h>

#define V5F_LTDC_GRAY_IMAGE_WIDTH  800u
#define V5F_LTDC_GRAY_IMAGE_HEIGHT 480u
#define V5F_LTDC_GRAY_IMAGE_BYTES  (V5F_LTDC_GRAY_IMAGE_WIDTH * V5F_LTDC_GRAY_IMAGE_HEIGHT)

extern const uint8_t v5f_ltdc_gray_800x480[];
extern const uint8_t v5f_ltdc_gray_800x480_end[];

#endif
