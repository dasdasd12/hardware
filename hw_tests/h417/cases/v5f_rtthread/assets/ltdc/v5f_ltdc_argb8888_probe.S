    .section .image_rodata, "a", @progbits
    .global v5f_ltdc_argb8888_probe_320x192
    .type v5f_ltdc_argb8888_probe_320x192, @object
    .balign 4
v5f_ltdc_argb8888_probe_320x192:
    .incbin "../../../hw_tests/h417/cases/v5f_rtthread/assets/ltdc/v5f_ltdc_argb8888_probe_320x192.raw"
    .global v5f_ltdc_argb8888_probe_320x192_end
    .balign 4
v5f_ltdc_argb8888_probe_320x192_end:
    .size v5f_ltdc_argb8888_probe_320x192, v5f_ltdc_argb8888_probe_320x192_end - v5f_ltdc_argb8888_probe_320x192
