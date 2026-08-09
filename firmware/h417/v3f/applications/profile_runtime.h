#ifndef V3F_PROFILE_RUNTIME_H
#define V3F_PROFILE_RUNTIME_H

/*
 * V3F profile runtime: validates AKPK packages, installs the embedded
 * AKRT runtime table into RAM structures the report path reads, and
 * derives the per-half AKHR patches pushed to the CH585s.
 *
 * Load order at boot: active logical slot (an erased user slot derives
 * its built-in preset from the embedded factory AKPK image) -> factory
 * slot -> (caller falls back to the legacy default_profile table when
 * nothing validates).
 */

#include <stdint.h>

#include "aik_profile_format.h"
#include "half_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fiveway direction events: firmware extension of the doc
 * signal_event_code enum (pending upstream assignment). */
#define V3F_RT_EVENT_FW_UP    0x0100U
#define V3F_RT_EVENT_FW_DOWN  0x0101U
#define V3F_RT_EVENT_FW_LEFT  0x0102U
#define V3F_RT_EVENT_FW_RIGHT 0x0103U

#define V3F_PROFILE_LOCAL_MAX 12U

#define V3F_PROFILE_OK          0
#define V3F_PROFILE_ERR_PACKAGE -1
#define V3F_PROFILE_ERR_TABLE   -2
#define V3F_PROFILE_ERR_PARAM   -3

typedef struct
{
    uint8_t valid;
    uint8_t active_slot;
    uint8_t has_fn_overlay;
    uint8_t fn_hold_key;        /* 0xFF when no overlay hold key */
    uint16_t profile_id16;
    uint16_t generation16;
    uint16_t usb_report_rate_hz;
    uint16_t wireless_report_rate_hz;
    uint32_t revision;
    aik_hp_key_output_t base_keys[AIK_KEY_COUNT_TOTAL];
    aik_hp_key_output_t fn_keys[AIK_KEY_COUNT_TOTAL];
    aik_hp_trigger_entry_t triggers[AIK_KEY_COUNT_TOTAL];
    aik_hp_local_entry_t locals[V3F_PROFILE_LOCAL_MAX];
    uint8_t local_count;
} v3f_profile_runtime_t;

/* Validate a memory-mapped AKPK package. buf_limit bounds how many
 * bytes may be read; on success *out_total_len receives the package
 * total_size. */
int v3f_profile_package_validate(const uint8_t *pkg, uint32_t buf_limit,
                                 uint32_t *out_total_len);

/* Parse a validated package into a caller-owned candidate without
 * changing the active runtime or release-to-rearm state. */
int v3f_profile_runtime_prepare_package(
    const uint8_t *pkg,
    uint32_t buf_limit,
    uint8_t slot_id,
    v3f_profile_runtime_t *out_candidate);

/* Prepare a runtime by logical slot. Factory always uses the embedded
 * image. An erased user slot derives its built-in Profile 1/2/3 preset
 * from that image without writing flash during boot; a stored package
 * always overrides the preset. Non-empty invalid slots remain errors
 * rather than being silently replaced. */
int v3f_profile_runtime_prepare_slot(
    uint8_t slot_id,
    v3f_profile_runtime_t *out_candidate);

/* Commit a previously prepared candidate. This is deliberately a
 * no-failure operation so persistent active-slot metadata can be
 * updated before the visible runtime table changes. */
void v3f_profile_runtime_commit_candidate(
    const v3f_profile_runtime_t *candidate);

/* Parse and immediately install a package. Kept for boot-time loading
 * and tests; interactive activation should use profile_activate so the
 * runtime and persistent active-slot metadata change transactionally. */
int v3f_profile_runtime_install_package(const uint8_t *pkg,
                                        uint32_t buf_limit,
                                        uint8_t slot_id);

/* Boot-time load: active slot, then factory image. Returns the slot
 * that ended up active, or 0xFF if nothing validated (caller keeps the
 * legacy hardcoded path). */
uint8_t v3f_profile_runtime_init(void);

const v3f_profile_runtime_t *v3f_profile_runtime_get(void);
uint8_t v3f_profile_runtime_valid(void);

/* Report path. Mirrors the legacy default_profile/local-control
 * helpers, but reads the installed runtime table. */
void v3f_profile_runtime_build_nkro16(const v3f_global_key_state_t *keys,
                                      uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);
void v3f_profile_runtime_apply_local_keyboard(
    const aik_spi_half_state_v1_t *left,
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);
uint16_t v3f_profile_runtime_consumer_usage(
    const aik_spi_half_state_v1_t *right);
int8_t v3f_profile_runtime_mouse_wheel(const aik_spi_half_state_v1_t *left);

/* Derive the AKHR patch for one half from the installed runtime.
 * Returns the patch length, or 0 on error. */
uint16_t v3f_profile_runtime_build_half_patch(uint8_t half_id,
                                              uint8_t *out,
                                              uint16_t out_max);

/* Release-to-rearm after a table swap: returns 1 exactly once after an
 * install; the caller then latches the currently-held keys, which stay
 * suppressed (old binding neutralised, new one not yet armed) until
 * they are physically released. */
uint8_t v3f_profile_runtime_rearm_take(void);
void v3f_profile_runtime_rearm_latch(const v3f_global_key_state_t *keys);

#ifdef __cplusplus
}
#endif

#endif /* V3F_PROFILE_RUNTIME_H */
