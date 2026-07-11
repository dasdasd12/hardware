#ifndef V3F_PROFILE_SYNC_H
#define V3F_PROFILE_SYNC_H

/*
 * H417 -> CH585 profile synchronisation.
 *
 * Derives the AKHR patch for each half from the installed runtime and
 * streams it over the SPI profile command group, one transaction per
 * main-loop tick (interleaved with normal state polling so the report
 * path keeps running). Reconciliation: the periodic profile status
 * poll compares each half's reported slot/id/generation against the
 * active runtime and re-marks the half dirty on mismatch.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void v3f_profile_sync_init(void);
void v3f_profile_sync_mark_all_dirty(void);

/* Run the sync engine for this tick. Returns a bitmask of halves that
 * used the SPI bus (bit 0 = left, bit 1 = right); the caller skips the
 * normal state poll for those halves this tick. */
uint8_t v3f_profile_sync_poll(uint16_t host_seq);

/* Periodic status reconciliation (call at a low rate). */
void v3f_profile_sync_status_poll(uint16_t host_seq);

uint8_t v3f_profile_sync_half_synced(uint8_t half_id);

#ifdef __cplusplus
}
#endif

#endif /* V3F_PROFILE_SYNC_H */
