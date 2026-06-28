/*
 * Fixed bring-up keyboard profile map.
 *
 * Source of truth: Docs-For-AI-Keyboard report keymap tables.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "keyboard_profile.h"

#ifndef APP_CH585_SPI_PCB_SOURCE0_RIGHT
#define APP_CH585_SPI_PCB_SOURCE0_RIGHT 0
#endif

#define HID_A           0x04U
#define HID_B           0x05U
#define HID_C           0x06U
#define HID_D           0x07U
#define HID_E           0x08U
#define HID_F           0x09U
#define HID_G           0x0AU
#define HID_H           0x0BU
#define HID_I           0x0CU
#define HID_J           0x0DU
#define HID_K           0x0EU
#define HID_L           0x0FU
#define HID_M           0x10U
#define HID_N           0x11U
#define HID_O           0x12U
#define HID_P           0x13U
#define HID_Q           0x14U
#define HID_R           0x15U
#define HID_S           0x16U
#define HID_T           0x17U
#define HID_U           0x18U
#define HID_V           0x19U
#define HID_W           0x1AU
#define HID_X           0x1BU
#define HID_Y           0x1CU
#define HID_Z           0x1DU
#define HID_1           0x1EU
#define HID_2           0x1FU
#define HID_3           0x20U
#define HID_4           0x21U
#define HID_5           0x22U
#define HID_6           0x23U
#define HID_7           0x24U
#define HID_8           0x25U
#define HID_9           0x26U
#define HID_0           0x27U
#define HID_ENTER       0x28U
#define HID_ESC         0x29U
#define HID_BACKSPACE   0x2AU
#define HID_TAB         0x2BU
#define HID_SPACE       0x2CU
#define HID_MINUS       0x2DU
#define HID_EQUAL       0x2EU
#define HID_LBRACKET    0x2FU
#define HID_RBRACKET    0x30U
#define HID_BACKSLASH   0x31U
#define HID_SEMICOLON   0x33U
#define HID_QUOTE       0x34U
#define HID_GRAVE       0x35U
#define HID_COMMA       0x36U
#define HID_DOT         0x37U
#define HID_SLASH       0x38U
#define HID_CAPS_LOCK   0x39U
#define HID_F1          0x3AU
#define HID_F2          0x3BU
#define HID_F3          0x3CU
#define HID_F4          0x3DU
#define HID_F5          0x3EU
#define HID_F6          0x3FU
#define HID_F7          0x40U
#define HID_F8          0x41U
#define HID_F9          0x42U
#define HID_F10         0x43U
#define HID_F11         0x44U
#define HID_F12         0x45U

#define MOD_LCTRL       0x01U
#define MOD_LSHIFT      0x02U
#define MOD_LALT        0x04U
#define MOD_LGUI        0x08U
#define MOD_RCTRL       0x10U
#define MOD_RSHIFT      0x20U
#define MOD_RALT        0x40U
#define MOD_RGUI        0x80U

/*
 * Local id = mux_lane * 16 + zero_based_mux_D.
 * Left table covers report hall ids 42..77.
 */
static const keyboard_profile_entry_t left_entries[] = {
    {KEYBOARD_PROFILE_SOURCE_LEFT,  0U, 42U, HID_F5,        0U,         "F5"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  1U, 43U, HID_F4,        0U,         "F4"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  2U, 44U, HID_F3,        0U,         "F3"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  3U, 45U, HID_F2,        0U,         "F2"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  4U, 46U, HID_F1,        0U,         "F1"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  5U, 47U, HID_ESC,       0U,         "Esc"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  6U, 48U, HID_6,         0U,         "6"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  7U, 49U, HID_5,         0U,         "5"},
    {KEYBOARD_PROFILE_SOURCE_LEFT,  8U, 50U, HID_4,         0U,         "4"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 16U, 51U, HID_3,         0U,         "3"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 17U, 52U, HID_2,         0U,         "2"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 18U, 53U, HID_1,         0U,         "1"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 19U, 54U, HID_GRAVE,     0U,         "Grave"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 20U, 55U, HID_Y,         0U,         "Y"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 21U, 56U, HID_T,         0U,         "T"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 22U, 57U, HID_R,         0U,         "R"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 23U, 58U, HID_E,         0U,         "E"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 24U, 59U, HID_W,         0U,         "W"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 32U, 60U, HID_Q,         0U,         "Q"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 33U, 61U, HID_TAB,       0U,         "Tab"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 34U, 62U, HID_G,         0U,         "G"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 35U, 63U, HID_F,         0U,         "F"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 36U, 64U, HID_D,         0U,         "D"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 37U, 65U, HID_S,         0U,         "S"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 38U, 66U, HID_A,         0U,         "A"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 39U, 67U, HID_CAPS_LOCK, 0U,         "Caps"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 40U, 68U, HID_B,         0U,         "B"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 48U, 69U, HID_V,         0U,         "V"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 49U, 70U, HID_C,         0U,         "C"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 50U, 71U, HID_X,         0U,         "X"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 51U, 72U, HID_Z,         0U,         "Z"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 52U, 73U, 0U,            MOD_LSHIFT, "LShift"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 53U, 74U, HID_SPACE,     0U,         "Space"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 54U, 75U, 0U,            MOD_LALT,   "LAlt"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 55U, 76U, 0U,            MOD_LGUI,   "LWin"},
    {KEYBOARD_PROFILE_SOURCE_LEFT, 56U, 77U, 0U,            MOD_LCTRL,  "LCtrl"},
};

/*
 * Right table covers report hall ids 1..41.
 */
static const keyboard_profile_entry_t right_entries[] = {
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  0U,  1U, HID_F12,       0U,         "F12"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  1U,  2U, HID_F11,       0U,         "F11"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  2U,  3U, HID_F10,       0U,         "F10"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  3U,  4U, HID_F9,        0U,         "F9"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  4U,  5U, HID_F8,        0U,         "F8"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  5U,  6U, HID_F7,        0U,         "F7"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  6U,  7U, HID_F6,        0U,         "F6"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  7U,  8U, HID_BACKSPACE, 0U,         "Backspace"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  8U,  9U, HID_EQUAL,     0U,         "Equal"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT,  9U, 10U, HID_MINUS,     0U,         "Minus"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 16U, 11U, HID_0,         0U,         "0"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 17U, 12U, HID_9,         0U,         "9"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 18U, 13U, HID_8,         0U,         "8"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 19U, 14U, HID_7,         0U,         "7"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 20U, 15U, HID_BACKSLASH, 0U,         "Backslash"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 21U, 16U, HID_RBRACKET,  0U,         "RBracket"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 22U, 17U, HID_LBRACKET,  0U,         "LBracket"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 23U, 18U, HID_P,         0U,         "P"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 24U, 19U, HID_O,         0U,         "O"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 25U, 20U, HID_I,         0U,         "I"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 32U, 21U, HID_U,         0U,         "U"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 33U, 22U, HID_Y,         0U,         "Y"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 34U, 23U, HID_ENTER,     0U,         "Enter"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 35U, 24U, HID_QUOTE,     0U,         "Quote"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 36U, 25U, HID_SEMICOLON, 0U,         "Semicolon"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 37U, 26U, HID_L,         0U,         "L"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 38U, 27U, HID_K,         0U,         "K"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 39U, 28U, HID_J,         0U,         "J"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 40U, 29U, HID_H,         0U,         "H"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 41U, 30U, 0U,            MOD_RSHIFT, "RShift"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 48U, 31U, HID_SLASH,     0U,         "Slash"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 49U, 32U, HID_DOT,       0U,         "Dot"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 50U, 33U, HID_COMMA,     0U,         "Comma"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 51U, 34U, HID_M,         0U,         "M"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 52U, 35U, HID_N,         0U,         "N"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 53U, 36U, HID_B,         0U,         "B"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 54U, 37U, 0U,            MOD_RCTRL,  "RCtrl"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 55U, 38U, 0U,            MOD_RGUI,   "RWin"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 56U, 39U, 0U,            0U,         "Fn"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 57U, 40U, 0U,            MOD_RALT,   "RAlt"},
    {KEYBOARD_PROFILE_SOURCE_RIGHT, 58U, 41U, HID_SPACE,     0U,         "Space"},
};

static const keyboard_profile_entry_t *lookup_in_table(const keyboard_profile_entry_t *table,
                                                       size_t count,
                                                       uint8_t local_id)
{
    size_t i;

    for (i = 0U; i < count; i++)
    {
        if (table[i].local_id == local_id)
        {
            return &table[i];
        }
    }

    return NULL;
}

const keyboard_profile_entry_t *keyboard_profile_lookup(uint8_t logical_source,
                                                        uint8_t local_id)
{
    if (logical_source == KEYBOARD_PROFILE_SOURCE_LEFT)
    {
        return lookup_in_table(left_entries,
                               sizeof(left_entries) / sizeof(left_entries[0]),
                               local_id);
    }

    if (logical_source == KEYBOARD_PROFILE_SOURCE_RIGHT)
    {
        return lookup_in_table(right_entries,
                               sizeof(right_entries) / sizeof(right_entries[0]),
                               local_id);
    }

    return NULL;
}

uint8_t keyboard_profile_logical_source_from_scan_source(uint8_t scan_source)
{
#if APP_CH585_SPI_PCB_SOURCE0_RIGHT
    return (scan_source == 0U) ? KEYBOARD_PROFILE_SOURCE_RIGHT :
                                 KEYBOARD_PROFILE_SOURCE_LEFT;
#else
    return (scan_source == 0U) ? KEYBOARD_PROFILE_SOURCE_LEFT :
                                 KEYBOARD_PROFILE_SOURCE_RIGHT;
#endif
}

const keyboard_profile_entry_t *keyboard_profile_lookup_scan_source(uint8_t scan_source,
                                                                    uint8_t local_id)
{
    return keyboard_profile_lookup(keyboard_profile_logical_source_from_scan_source(scan_source),
                                   local_id);
}

const keyboard_profile_entry_t *keyboard_profile_lookup_raw_index(uint16_t raw_index)
{
    uint8_t scan_source = (uint8_t)(raw_index / KEYBOARD_PROFILE_HALF_KEY_COUNT);
    uint8_t local_id = (uint8_t)(raw_index % KEYBOARD_PROFILE_HALF_KEY_COUNT);

    return keyboard_profile_lookup_scan_source(scan_source, local_id);
}

const char *keyboard_profile_source_name(uint8_t logical_source)
{
    return (logical_source == KEYBOARD_PROFILE_SOURCE_RIGHT) ? "R" : "L";
}

const char *keyboard_profile_scan_source_name(uint8_t scan_source)
{
    return keyboard_profile_source_name(
        keyboard_profile_logical_source_from_scan_source(scan_source));
}

static uint8_t report_has_usage(const uint8_t report[KEYBOARD_PROFILE_BOOT_REPORT_LEN],
                                uint8_t usage)
{
    uint8_t i;

    for (i = 2U; i < KEYBOARD_PROFILE_BOOT_REPORT_LEN; i++)
    {
        if (report[i] == usage)
        {
            return 1U;
        }
    }

    return 0U;
}

uint8_t keyboard_profile_build_boot_report_from_raw(const uint16_t *raw_adc,
                                                    uint16_t key_count,
                                                    uint16_t down_threshold,
                                                    uint8_t report[KEYBOARD_PROFILE_BOOT_REPORT_LEN],
                                                    uint8_t *first_scan_source,
                                                    uint8_t *first_local_id)
{
    uint16_t raw_index;
    uint8_t slot = 2U;
    uint8_t any_down = 0U;

    if (report != NULL)
    {
        memset(report, 0, KEYBOARD_PROFILE_BOOT_REPORT_LEN);
    }
    if (first_scan_source != NULL)
    {
        *first_scan_source = 0xFFU;
    }
    if (first_local_id != NULL)
    {
        *first_local_id = 0xFFU;
    }

    if ((raw_adc == NULL) || (report == NULL))
    {
        return 0U;
    }

    for (raw_index = 0U; raw_index < key_count; raw_index++)
    {
        const keyboard_profile_entry_t *entry;
        uint8_t scan_source;
        uint8_t local_id;

        if (raw_adc[raw_index] < down_threshold)
        {
            continue;
        }

        entry = keyboard_profile_lookup_raw_index(raw_index);
        if (entry == NULL)
        {
            continue;
        }

        scan_source = (uint8_t)(raw_index / KEYBOARD_PROFILE_HALF_KEY_COUNT);
        local_id = (uint8_t)(raw_index % KEYBOARD_PROFILE_HALF_KEY_COUNT);

        any_down = 1U;
        if ((first_scan_source != NULL) && (*first_scan_source == 0xFFU))
        {
            *first_scan_source = scan_source;
        }
        if ((first_local_id != NULL) && (*first_local_id == 0xFFU))
        {
            *first_local_id = local_id;
        }

        report[0] |= entry->modifier;

        if ((entry->hid_usage == 0U) ||
            (report_has_usage(report, entry->hid_usage) != 0U))
        {
            continue;
        }

        if (slot < KEYBOARD_PROFILE_BOOT_REPORT_LEN)
        {
            report[slot++] = entry->hid_usage;
        }
    }

    return any_down;
}
