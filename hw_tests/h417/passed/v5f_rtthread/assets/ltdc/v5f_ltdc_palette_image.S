    .section .image_rodata, "a", @progbits
    .global v5f_ltdc_palette_800x480
    .type v5f_ltdc_palette_800x480, @object
    .balign 4
v5f_ltdc_palette_800x480:
    .incbin "../../../hw_tests/h417/passed/v5f_rtthread/assets/ltdc/v5f_ltdc_palette_800x480.raw"
    .global v5f_ltdc_palette_800x480_end
    .balign 4
v5f_ltdc_palette_800x480_end:
    .size v5f_ltdc_palette_800x480, v5f_ltdc_palette_800x480_end - v5f_ltdc_palette_800x480

    .global v5f_ltdc_palette_800x480_clut_rgb888
    .type v5f_ltdc_palette_800x480_clut_rgb888, @object
    .balign 4
v5f_ltdc_palette_800x480_clut_rgb888:
    .incbin "../../../hw_tests/h417/passed/v5f_rtthread/assets/ltdc/v5f_ltdc_palette_800x480_clut.rgb"
    .global v5f_ltdc_palette_800x480_clut_rgb888_end
    .balign 4
v5f_ltdc_palette_800x480_clut_rgb888_end:
    .size v5f_ltdc_palette_800x480_clut_rgb888, v5f_ltdc_palette_800x480_clut_rgb888_end - v5f_ltdc_palette_800x480_clut_rgb888
