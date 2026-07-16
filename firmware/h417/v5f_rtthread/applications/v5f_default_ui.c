/*
 * Default 800x480 L8 product image.
 *
 * This is the static welcome frame validated by the V5F LTDC UI hardware
 * test: near-black background, "Welcome back!" title, and the centered
 * Claude Code-style orange mascot. Drawing uses the product display service,
 * which applies the panel's physical 180-degree mounting rotation.
 */

#include "v5f_default_ui.h"

#include <stddef.h>

#include "ch32h417_ltdc_rgb.h"
#include "v5f_display.h"

#define UI_COLOR_BG      0u
#define UI_COLOR_WHITE   1u
#define UI_COLOR_BLUE    2u
#define UI_COLOR_ORANGE  3u
#define UI_COLOR_GRAY    4u
#define UI_COLOR_BORDER  5u

#define UI_GLYPH_SIZE    8u
#define UI_MASCOT_COLS   16u
#define UI_MASCOT_ROWS   10u

typedef struct
{
    char character;
    uint8_t rows[UI_GLYPH_SIZE];
} ui_glyph_t;

/* Same six-color palette as the validated hardware-test UI. */
static const uint8_t s_ui_clut_rgb888[
    CH32H417_LTDC_RGB_CLUT_ENTRIES * 3u] = {
    0x0Cu, 0x0Cu, 0x0Cu, /* 0 background, near black */
    0xF2u, 0xF2u, 0xF2u, /* 1 primary text, white */
    0x46u, 0x96u, 0xFFu, /* 2 hovered option, blue */
    0xD7u, 0x77u, 0x57u, /* 3 mascot and bullet, Claude orange */
    0x9Au, 0x9Au, 0x9Au, /* 4 secondary text, gray */
    0x5Au, 0x5Au, 0x5Au, /* 5 dialog border, dark gray */
};

/* 16x10 single-color mascot, bit 0 = leftmost cell. */
static const uint16_t s_ui_mascot_rows[UI_MASCOT_ROWS] = {
    0x3FFCu,
    0x3FFCu,
    0x37ECu,
    0x37ECu,
    0xFFFFu,
    0xFFFFu,
    0x3FFCu,
    0x3FFCu,
    0x1428u,
    0x1428u,
};

/*
 * font8x8_basic glyphs used by "Welcome back!".
 * Original font by Daniel Hepper / Marcel Sondaar, public domain.
 */
static const ui_glyph_t s_ui_glyphs[] = {
    {' ', {0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u}},
    {'!', {0x18u, 0x3Cu, 0x3Cu, 0x18u, 0x18u, 0x00u, 0x18u, 0x00u}},
    {'W', {0x63u, 0x63u, 0x63u, 0x6Bu, 0x7Fu, 0x77u, 0x63u, 0x00u}},
    {'a', {0x00u, 0x00u, 0x1Eu, 0x30u, 0x3Eu, 0x33u, 0x6Eu, 0x00u}},
    {'b', {0x07u, 0x06u, 0x06u, 0x3Eu, 0x66u, 0x66u, 0x3Bu, 0x00u}},
    {'c', {0x00u, 0x00u, 0x1Eu, 0x33u, 0x03u, 0x33u, 0x1Eu, 0x00u}},
    {'e', {0x00u, 0x00u, 0x1Eu, 0x33u, 0x3Fu, 0x03u, 0x1Eu, 0x00u}},
    {'k', {0x07u, 0x06u, 0x66u, 0x36u, 0x1Eu, 0x36u, 0x67u, 0x00u}},
    {'l', {0x0Eu, 0x0Cu, 0x0Cu, 0x0Cu, 0x0Cu, 0x0Cu, 0x1Eu, 0x00u}},
    {'m', {0x00u, 0x00u, 0x33u, 0x7Fu, 0x7Fu, 0x6Bu, 0x63u, 0x00u}},
    {'o', {0x00u, 0x00u, 0x1Eu, 0x33u, 0x33u, 0x33u, 0x1Eu, 0x00u}},
};

static const uint8_t *ui_find_glyph(char character)
{
    size_t index;

    for(index = 0u; index < (sizeof(s_ui_glyphs) / sizeof(s_ui_glyphs[0]));
        index++)
    {
        if(s_ui_glyphs[index].character == character)
        {
            return s_ui_glyphs[index].rows;
        }
    }

    return s_ui_glyphs[0].rows;
}

static void ui_draw_char(uint16_t x,
                         uint16_t y,
                         char character,
                         uint16_t scale,
                         uint8_t color)
{
    const uint8_t *glyph = ui_find_glyph(character);
    uint32_t row;
    uint32_t column;

    for(row = 0u; row < UI_GLYPH_SIZE; row++)
    {
        uint8_t bits = glyph[row];

        for(column = 0u; column < UI_GLYPH_SIZE; column++)
        {
            if((bits & (uint8_t)(1u << column)) != 0u)
            {
                v5f_display_fill_rect(
                    (uint16_t)(x + (column * scale)),
                    (uint16_t)(y + (row * scale)),
                    scale,
                    scale,
                    color);
            }
        }
    }
}

static uint16_t ui_text_width(const char *text, uint16_t scale)
{
    uint16_t characters = 0u;

    while(text[characters] != '\0')
    {
        characters++;
    }

    return (uint16_t)(characters * UI_GLYPH_SIZE * scale);
}

static void ui_draw_text_centered(uint16_t y,
                                  const char *text,
                                  uint16_t scale,
                                  uint8_t color)
{
    uint16_t x =
        (uint16_t)((V5F_DISPLAY_WIDTH - ui_text_width(text, scale)) / 2u);

    while(*text != '\0')
    {
        ui_draw_char(x, y, *text, scale, color);
        x = (uint16_t)(x + (UI_GLYPH_SIZE * scale));
        text++;
    }
}

static void ui_draw_mascot(uint16_t x, uint16_t y, uint16_t cell)
{
    uint32_t row;
    uint32_t column;

    for(row = 0u; row < UI_MASCOT_ROWS; row++)
    {
        uint16_t bits = s_ui_mascot_rows[row];

        for(column = 0u; column < UI_MASCOT_COLS; column++)
        {
            if((bits & (uint16_t)(1u << column)) != 0u)
            {
                v5f_display_fill_rect(
                    (uint16_t)(x + (column * cell)),
                    (uint16_t)(y + (row * cell)),
                    cell,
                    cell,
                    UI_COLOR_ORANGE);
            }
        }
    }
}

const uint8_t *v5f_default_ui_clut_rgb888(void)
{
    return s_ui_clut_rgb888;
}

void v5f_default_ui_draw(void)
{
    const uint16_t mascot_cell = 20u;
    const uint16_t mascot_width = UI_MASCOT_COLS * mascot_cell;

    v5f_display_fill(UI_COLOR_BG);
    ui_draw_text_centered(58u, "Welcome back!", 6u, UI_COLOR_WHITE);
    ui_draw_mascot((uint16_t)((V5F_DISPLAY_WIDTH - mascot_width) / 2u),
                   195u,
                   mascot_cell);
    v5f_display_present();
}
