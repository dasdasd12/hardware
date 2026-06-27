#ifndef CH32H417_GPHA_2D_H
#define CH32H417_GPHA_2D_H

#include <stdint.h>

/*
 * CH32H417 GPHA 2D helper.
 *
 * This driver is a thin blocking wrapper around the WCH GPHA peripheral. It
 * has no RT-Thread dependency and does not allocate memory. Callers own all
 * framebuffers and CLUT buffers and must keep them alive until the blocking
 * operation returns.
 *
 * L8 note: GPHA does not provide native L8 output. The L8 fill helper uses
 * ARGB8888 R2M mode as a byte writer: one GPHA pixel writes four adjacent L8
 * indices. Therefore x and width must be 4-byte aligned.
 */

#define CH32H417_GPHA_2D_OK               0
#define CH32H417_GPHA_2D_ERR_PARAM        (-1)
#define CH32H417_GPHA_2D_ERR_CONFIG       (-2)
#define CH32H417_GPHA_2D_ERR_TIMEOUT      (-3)
#define CH32H417_GPHA_2D_ERR_CLUT_CONFIG  (-4)
#define CH32H417_GPHA_2D_ERR_CLUT_TIMEOUT (-5)

void ch32h417_gpha_2d_init(void);
uint32_t ch32h417_gpha_2d_argb8888(uint8_t red, uint8_t green, uint8_t blue);
uint16_t ch32h417_gpha_2d_argb4444(uint8_t alpha,
                                   uint8_t red,
                                   uint8_t green,
                                   uint8_t blue);

int ch32h417_gpha_2d_fill_rgb565(uint16_t *framebuffer,
                                 uint16_t framebuffer_width,
                                 uint16_t framebuffer_height,
                                 uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height,
                                 uint16_t color);

int ch32h417_gpha_2d_fill_l8_quad(uint8_t *framebuffer,
                                  uint16_t framebuffer_width,
                                  uint16_t framebuffer_height,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint8_t index0,
                                  uint8_t index1,
                                  uint8_t index2,
                                  uint8_t index3);

int ch32h417_gpha_2d_l8_to_rgb565(const uint8_t *source_l8,
                                  uint16_t *dest_rgb565,
                                  const uint32_t *clut_argb8888,
                                  uint16_t width,
                                  uint16_t height,
                                  uint16_t clut_entries);

int ch32h417_gpha_2d_blend_argb4444_over_rgb565(const uint16_t *fg_argb4444,
                                                const uint16_t *bg_rgb565,
                                                uint16_t *dest_rgb565,
                                                uint16_t width,
                                                uint16_t height);

void ch32h417_gpha_2d_memory_barrier(void);

#endif
