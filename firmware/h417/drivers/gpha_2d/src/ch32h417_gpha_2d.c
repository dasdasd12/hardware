#include "ch32h417_gpha_2d.h"

#include "ch32h417_gpha.h"
#include "ch32h417_rcc.h"

#define GPHA_2D_TIMEOUT_POLLS 1000000u

static uint8_t rgb565_red8(uint16_t color)
{
    uint8_t value = (uint8_t)((color >> 11) & 0x1Fu);
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t rgb565_green8(uint16_t color)
{
    uint8_t value = (uint8_t)((color >> 5) & 0x3Fu);
    return (uint8_t)((value << 2) | (value >> 4));
}

static uint8_t rgb565_blue8(uint16_t color)
{
    uint8_t value = (uint8_t)(color & 0x1Fu);
    return (uint8_t)((value << 3) | (value >> 2));
}

static int wait_transfer_complete(void)
{
    uint32_t timeout = GPHA_2D_TIMEOUT_POLLS;

    while(timeout != 0u)
    {
        if(GPHA_GetFlagStatus(GPHA_FLAG_CE) != RESET)
        {
            GPHA_ClearFlag(GPHA_FLAG_CE);
            return CH32H417_GPHA_2D_ERR_CONFIG;
        }
        if(GPHA_GetFlagStatus(GPHA_FLAG_TC) != RESET)
        {
            GPHA_ClearFlag(GPHA_FLAG_TC);
            ch32h417_gpha_2d_memory_barrier();
            return CH32H417_GPHA_2D_OK;
        }
        timeout--;
    }

    GPHA_AbortTransfer();
    return CH32H417_GPHA_2D_ERR_TIMEOUT;
}

static int wait_clut_complete(void)
{
    uint32_t timeout = GPHA_2D_TIMEOUT_POLLS;

    while(timeout != 0u)
    {
        if(GPHA_GetFlagStatus(GPHA_FLAG_CAE) != RESET)
        {
            GPHA_ClearFlag(GPHA_FLAG_CAE);
            return CH32H417_GPHA_2D_ERR_CLUT_CONFIG;
        }
        if(GPHA_GetFlagStatus(GPHA_FLAG_CE) != RESET)
        {
            GPHA_ClearFlag(GPHA_FLAG_CE);
            return CH32H417_GPHA_2D_ERR_CONFIG;
        }
        if(GPHA_GetFlagStatus(GPHA_FLAG_CTC) != RESET)
        {
            GPHA_ClearFlag(GPHA_FLAG_CTC);
            ch32h417_gpha_2d_memory_barrier();
            return CH32H417_GPHA_2D_OK;
        }
        timeout--;
    }

    return CH32H417_GPHA_2D_ERR_CLUT_TIMEOUT;
}

static uint8_t area_valid(uint16_t framebuffer_width,
                          uint16_t framebuffer_height,
                          uint16_t x,
                          uint16_t y,
                          uint16_t width,
                          uint16_t height)
{
    return (uint8_t)((framebuffer_width != 0u) &&
                     (framebuffer_height != 0u) &&
                     (width != 0u) &&
                     (height != 0u) &&
                     (((uint32_t)x + width) <= framebuffer_width) &&
                     (((uint32_t)y + height) <= framebuffer_height));
}

void ch32h417_gpha_2d_memory_barrier(void)
{
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

void ch32h417_gpha_2d_init(void)
{
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPHA, ENABLE);
    GPHA_DeInit();
}

uint32_t ch32h417_gpha_2d_argb8888(uint8_t red, uint8_t green, uint8_t blue)
{
    return 0xFF000000u |
           ((uint32_t)red << 16) |
           ((uint32_t)green << 8) |
           (uint32_t)blue;
}

uint16_t ch32h417_gpha_2d_argb4444(uint8_t alpha,
                                   uint8_t red,
                                   uint8_t green,
                                   uint8_t blue)
{
    return (uint16_t)(((uint16_t)(alpha & 0x0Fu) << 12) |
                      ((uint16_t)(red & 0x0Fu) << 8) |
                      ((uint16_t)(green & 0x0Fu) << 4) |
                      (uint16_t)(blue & 0x0Fu));
}

int ch32h417_gpha_2d_fill_rgb565(uint16_t *framebuffer,
                                 uint16_t framebuffer_width,
                                 uint16_t framebuffer_height,
                                 uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height,
                                 uint16_t color)
{
    GPHA_InitTypeDef init;

    if((framebuffer == 0) ||
       !area_valid(framebuffer_width, framebuffer_height, x, y, width, height))
    {
        return CH32H417_GPHA_2D_ERR_PARAM;
    }

    GPHA_ClearFlag(GPHA_FLAG_TC | GPHA_FLAG_CE | GPHA_FLAG_CTC | GPHA_FLAG_CAE | GPHA_FLAG_TW);
    GPHA_StructInit(&init);
    init.GPHA_Mode = GPHA_R2M;
    init.GPHA_CMode = GPHA_RGB565;
    init.GPHA_OutputBlue = rgb565_blue8(color);
    init.GPHA_OutputGreen = rgb565_green8(color);
    init.GPHA_OutputRed = rgb565_red8(color);
    init.GPHA_OutputAlpha = 0u;
    init.GPHA_OutputMemoryAdd =
        (uint32_t)&framebuffer[((uint32_t)y * framebuffer_width) + x];
    init.GPHA_OutputOffset = framebuffer_width - width;
    init.GPHA_NumberOfLine = height;
    init.GPHA_PixelPerLine = width;
    GPHA_Init(&init);
    GPHA_StartTransfer();

    return wait_transfer_complete();
}

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
                                  uint8_t index3)
{
    GPHA_InitTypeDef init;

    if((framebuffer == 0) ||
       ((x & 0x3u) != 0u) ||
       ((width & 0x3u) != 0u) ||
       !area_valid(framebuffer_width, framebuffer_height, x, y, width, height))
    {
        return CH32H417_GPHA_2D_ERR_PARAM;
    }

    GPHA_ClearFlag(GPHA_FLAG_TC | GPHA_FLAG_CE | GPHA_FLAG_CTC | GPHA_FLAG_CAE | GPHA_FLAG_TW);
    GPHA_StructInit(&init);
    init.GPHA_Mode = GPHA_R2M;
    init.GPHA_CMode = GPHA_ARGB8888;
    init.GPHA_OutputBlue = index0;
    init.GPHA_OutputGreen = index1;
    init.GPHA_OutputRed = index2;
    init.GPHA_OutputAlpha = index3;
    init.GPHA_OutputMemoryAdd =
        (uint32_t)&framebuffer[((uint32_t)y * framebuffer_width) + x];
    init.GPHA_OutputOffset = (framebuffer_width - width) / 4u;
    init.GPHA_NumberOfLine = height;
    init.GPHA_PixelPerLine = width / 4u;
    GPHA_Init(&init);
    GPHA_StartTransfer();

    return wait_transfer_complete();
}

int ch32h417_gpha_2d_l8_to_rgb565(const uint8_t *source_l8,
                                  uint16_t *dest_rgb565,
                                  const uint32_t *clut_argb8888,
                                  uint16_t width,
                                  uint16_t height,
                                  uint16_t clut_entries)
{
    GPHA_InitTypeDef init;
    GPHA_FG_InitTypeDef fg;
    int result;

    if((source_l8 == 0) ||
       (dest_rgb565 == 0) ||
       (clut_argb8888 == 0) ||
       (width == 0u) ||
       (height == 0u) ||
       (clut_entries == 0u) ||
       (clut_entries > 256u))
    {
        return CH32H417_GPHA_2D_ERR_PARAM;
    }

    GPHA_ClearFlag(GPHA_FLAG_TC | GPHA_FLAG_CE | GPHA_FLAG_CTC | GPHA_FLAG_CAE | GPHA_FLAG_TW);
    GPHA_StructInit(&init);
    init.GPHA_Mode = GPHA_M2M_PFC;
    init.GPHA_CMode = GPHA_RGB565;
    init.GPHA_OutputMemoryAdd = (uint32_t)dest_rgb565;
    init.GPHA_OutputOffset = 0u;
    init.GPHA_NumberOfLine = height;
    init.GPHA_PixelPerLine = width;
    GPHA_Init(&init);

    GPHA_FG_StructInit(&fg);
    fg.GPHA_FGMA = (uint32_t)source_l8;
    fg.GPHA_FGO = 0u;
    fg.GPHA_FGCM = CM_L8;
    fg.GPHA_FG_CLUT_CM = CLUT_CM_ARGB8888;
    fg.GPHA_FG_CLUT_SIZE = clut_entries - 1u;
    fg.GPHA_FGPFC_ALPHA_VALUE = 0xFFu;
    fg.GPHA_FGPFC_ALPHA_MODE = NO_MODIF_ALPHA_VALUE;
    fg.GPHA_FGC_BLUE = 0xFFu;
    fg.GPHA_FGC_GREEN = 0xFFu;
    fg.GPHA_FGC_RED = 0xFFu;
    fg.GPHA_FGCMAR = (uint32_t)clut_argb8888;
    GPHA_FGConfig(&fg);

    GPHA->FGCWRS = GPHA_FGCWRS_FG_CLUT_EN;
    GPHA_FGStart(ENABLE);
    result = wait_clut_complete();
    if(result != CH32H417_GPHA_2D_OK)
    {
        return result;
    }

    GPHA_ClearFlag(GPHA_FLAG_TC | GPHA_FLAG_CE | GPHA_FLAG_CTC | GPHA_FLAG_CAE | GPHA_FLAG_TW);
    GPHA_StartTransfer();
    return wait_transfer_complete();
}

int ch32h417_gpha_2d_blend_argb4444_over_rgb565(const uint16_t *fg_argb4444,
                                                const uint16_t *bg_rgb565,
                                                uint16_t *dest_rgb565,
                                                uint16_t width,
                                                uint16_t height)
{
    GPHA_InitTypeDef init;
    GPHA_FG_InitTypeDef fg;
    GPHA_BG_InitTypeDef bg;

    if((fg_argb4444 == 0) ||
       (bg_rgb565 == 0) ||
       (dest_rgb565 == 0) ||
       (width == 0u) ||
       (height == 0u))
    {
        return CH32H417_GPHA_2D_ERR_PARAM;
    }

    GPHA_ClearFlag(GPHA_FLAG_TC | GPHA_FLAG_CE | GPHA_FLAG_CTC | GPHA_FLAG_CAE | GPHA_FLAG_TW);
    GPHA_StructInit(&init);
    init.GPHA_Mode = GPHA_M2M_BLEND;
    init.GPHA_CMode = GPHA_RGB565;
    init.GPHA_OutputMemoryAdd = (uint32_t)dest_rgb565;
    init.GPHA_OutputOffset = 0u;
    init.GPHA_NumberOfLine = height;
    init.GPHA_PixelPerLine = width;
    GPHA_Init(&init);

    GPHA_FG_StructInit(&fg);
    fg.GPHA_FGMA = (uint32_t)fg_argb4444;
    fg.GPHA_FGO = 0u;
    fg.GPHA_FGCM = CM_ARGB4444;
    fg.GPHA_FGPFC_ALPHA_MODE = NO_MODIF_ALPHA_VALUE;
    fg.GPHA_FGPFC_ALPHA_VALUE = 0xFFu;
    GPHA_FGConfig(&fg);

    GPHA_BG_StructInit(&bg);
    bg.GPHA_BGMA = (uint32_t)bg_rgb565;
    bg.GPHA_BGO = 0u;
    bg.GPHA_BGCM = CM_RGB565;
    bg.GPHA_BGPFC_ALPHA_MODE = REPLACE_ALPHA_VALUE;
    bg.GPHA_BGPFC_ALPHA_VALUE = 0xFFu;
    GPHA_BGConfig(&bg);

    GPHA_StartTransfer();
    return wait_transfer_complete();
}
