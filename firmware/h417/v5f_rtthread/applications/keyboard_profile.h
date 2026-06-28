/*
 * Fixed bring-up keyboard profile map.
 *
 * This table maps CH585 half-scan local key ids to HID keyboard usages. It is
 * the firmware-side stand-in for the future PC-compiled Profile/RuntimeTable.
 */

#ifndef KEYBOARD_PROFILE_H__
#define KEYBOARD_PROFILE_H__

#include <stdint.h>

#define KEYBOARD_PROFILE_SOURCE_LEFT       0U
#define KEYBOARD_PROFILE_SOURCE_RIGHT      1U
#define KEYBOARD_PROFILE_BOOT_REPORT_LEN   8U
#define KEYBOARD_PROFILE_HALF_KEY_COUNT    64U

typedef struct
{
    uint8_t source;
    uint8_t local_id;
    uint16_t hall_id;
    uint8_t hid_usage;
    uint8_t modifier;
    const char *label;
} keyboard_profile_entry_t;

const keyboard_profile_entry_t *keyboard_profile_lookup(uint8_t logical_source,
                                                        uint8_t local_id);
const keyboard_profile_entry_t *keyboard_profile_lookup_scan_source(uint8_t scan_source,
                                                                    uint8_t local_id);
const keyboard_profile_entry_t *keyboard_profile_lookup_raw_index(uint16_t raw_index);
uint8_t keyboard_profile_logical_source_from_scan_source(uint8_t scan_source);
const char *keyboard_profile_source_name(uint8_t logical_source);
const char *keyboard_profile_scan_source_name(uint8_t scan_source);

uint8_t keyboard_profile_build_boot_report_from_raw(const uint16_t *raw_adc,
                                                    uint16_t key_count,
                                                    uint16_t down_threshold,
                                                    uint8_t report[KEYBOARD_PROFILE_BOOT_REPORT_LEN],
                                                    uint8_t *first_scan_source,
                                                    uint8_t *first_local_id);

#endif /* KEYBOARD_PROFILE_H__ */
