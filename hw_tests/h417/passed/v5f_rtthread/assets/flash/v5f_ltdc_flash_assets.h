#ifndef V5F_LTDC_FLASH_ASSETS_H
#define V5F_LTDC_FLASH_ASSETS_H

#include <stdint.h>

#define V5F_LTDC_FLASH_ASSET_WIDTH  800u
#define V5F_LTDC_FLASH_ASSET_HEIGHT 480u
#define V5F_LTDC_FLASH_ASSET_IMAGE_BYTES \
    (V5F_LTDC_FLASH_ASSET_WIDTH * V5F_LTDC_FLASH_ASSET_HEIGHT)
#define V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES 256u
#define V5F_LTDC_FLASH_ASSET_CLUT_BYTES   (V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES * 3u)

#define V5F_LTDC_FLASH_GRAY_LZSS_BYTES    262380u
#define V5F_LTDC_FLASH_PALETTE_LZSS_BYTES 186852u
#define V5F_LTDC_FLASH_GRAY_IMAGE_FNV     0xD14D5125u
#define V5F_LTDC_FLASH_GRAY_CLUT_FNV      0x7F16E8C5u
#define V5F_LTDC_FLASH_PALETTE_IMAGE_FNV  0x1769EBC9u
#define V5F_LTDC_FLASH_PALETTE_CLUT_FNV   0x4DD96A71u

extern const uint8_t v5f_ltdc_flash_gray_lzss[];
extern const uint8_t v5f_ltdc_flash_gray_lzss_end[];
extern const uint8_t v5f_ltdc_flash_palette_lzss[];
extern const uint8_t v5f_ltdc_flash_palette_lzss_end[];
extern const uint8_t v5f_ltdc_flash_palette_clut_rgb888[];
extern const uint8_t v5f_ltdc_flash_palette_clut_rgb888_end[];

#endif
