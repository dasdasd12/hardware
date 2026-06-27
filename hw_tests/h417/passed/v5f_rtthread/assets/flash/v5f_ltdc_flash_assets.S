    .section .image_rodata, "a", @progbits
    .global v5f_ltdc_flash_gray_lzss
    .type v5f_ltdc_flash_gray_lzss, @object
    .balign 4
v5f_ltdc_flash_gray_lzss:
    .incbin "../../../hw_tests/h417/passed/v5f_rtthread/assets/flash/v5f_ltdc_gray_800x480_left.lzss"
    .global v5f_ltdc_flash_gray_lzss_end
    .balign 4
v5f_ltdc_flash_gray_lzss_end:
    .size v5f_ltdc_flash_gray_lzss, v5f_ltdc_flash_gray_lzss_end - v5f_ltdc_flash_gray_lzss

    .global v5f_ltdc_flash_palette_lzss
    .type v5f_ltdc_flash_palette_lzss, @object
    .balign 4
v5f_ltdc_flash_palette_lzss:
    .incbin "../../../hw_tests/h417/passed/v5f_rtthread/assets/flash/v5f_ltdc_palette_800x480.lzss"
    .global v5f_ltdc_flash_palette_lzss_end
    .balign 4
v5f_ltdc_flash_palette_lzss_end:
    .size v5f_ltdc_flash_palette_lzss, v5f_ltdc_flash_palette_lzss_end - v5f_ltdc_flash_palette_lzss

    .global v5f_ltdc_flash_palette_clut_rgb888
    .type v5f_ltdc_flash_palette_clut_rgb888, @object
    .balign 4
v5f_ltdc_flash_palette_clut_rgb888:
    .incbin "../../../hw_tests/h417/passed/v5f_rtthread/assets/ltdc/v5f_ltdc_palette_800x480_clut.rgb"
    .global v5f_ltdc_flash_palette_clut_rgb888_end
    .balign 4
v5f_ltdc_flash_palette_clut_rgb888_end:
    .size v5f_ltdc_flash_palette_clut_rgb888, v5f_ltdc_flash_palette_clut_rgb888_end - v5f_ltdc_flash_palette_clut_rgb888
