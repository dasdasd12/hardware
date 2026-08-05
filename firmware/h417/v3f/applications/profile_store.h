#ifndef V3F_PROFILE_STORE_H
#define V3F_PROFILE_STORE_H

/*
 * H417-side profile slot storage in internal flash.
 *
 * The region lives outside both linker-declared images (V3F 0..64K,
 * V5F 0x10000..+500K) inside the 960KB code flash. Normal profile
 * updates, resets, and power cycles preserve it. A full-chip/Erase-All
 * firmware download still erases this region, so firmware upgrades must
 * back up and restore user profiles if preservation is required.
 *
 *   +0x00000  user slot 1   (32KB, AKPK package)
 *   +0x08000  user slot 2
 *   +0x10000  user slot 3
 *   +0x18000  staging area  (32KB, streamed upload target)
 *   +0x20000  metadata journal A (8KB)
 *   +0x22000  rollback copy (32KB, one transaction at a time)
 *   +0x2A000  metadata journal B (8KB)
 *
 * The original addresses through +0x21FFF are preserved. Existing user
 * slots and the v1 active-slot record therefore migrate in place. New
 * writes use the rollback copy plus an append-only, dual-sector metadata
 * journal so an interrupted erase/program operation never destroys the
 * last committed Profile.
 *
 * Program/erase go through FLASH_ROM_WRITE/FLASH_ROM_ERASE which take
 * 0x08000000-based addresses (dual-flash mode: 8KB erase granularity,
 * 256B program granularity). Reads use the same mapping directly.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V3F_PROFILE_FLASH_BASE     0x08090000UL
#define V3F_PROFILE_SLOT_SIZE      0x8000UL
#define V3F_PROFILE_STAGING_OFFSET (3UL * V3F_PROFILE_SLOT_SIZE)
#define V3F_PROFILE_META_OFFSET    (V3F_PROFILE_STAGING_OFFSET + \
                                    V3F_PROFILE_SLOT_SIZE)
#define V3F_PROFILE_META_SIZE      0x2000UL
#define V3F_PROFILE_ROLLBACK_OFFSET (V3F_PROFILE_META_OFFSET + \
                                     V3F_PROFILE_META_SIZE)
#define V3F_PROFILE_META_B_OFFSET   (V3F_PROFILE_ROLLBACK_OFFSET + \
                                     V3F_PROFILE_SLOT_SIZE)
#define V3F_PROFILE_STORE_END_OFFSET (V3F_PROFILE_META_B_OFFSET + \
                                      V3F_PROFILE_META_SIZE)

#define V3F_PROFILE_STORE_OK        0
#define V3F_PROFILE_STORE_ERR_PARAM -1
#define V3F_PROFILE_STORE_ERR_FLASH -2
#define V3F_PROFILE_STORE_ERR_STATE -3
#define V3F_PROFILE_STORE_ERR_VERIFY -4

/* Memory-mapped read pointer to the last committed package in user slot
 * 1..3. Returns 0 for invalid ids and empty/deleted slots. During recovery
 * it may point at the rollback copy instead of the physical slot. */
const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id);

/* True only when the slot contains a committed user package. An empty or
 * deleted user slot still resolves to the Factory Profile at runtime. */
uint8_t v3f_profile_store_slot_present(uint8_t slot_id);

const uint8_t *v3f_profile_store_staging_ptr(void);

/* Streamed upload into the staging area. Offsets must be sequential
 * from 0; data is buffered into 256B pages and programmed as they
 * fill. begin erases the staging area (blocking, tens of ms). */
int v3f_profile_store_staging_begin(uint32_t total_len);
int v3f_profile_store_staging_write(uint32_t offset, const uint8_t *data,
                                    uint32_t len);
int v3f_profile_store_staging_finish(void);
void v3f_profile_store_staging_abort(void);

/* Copy a validated staging image into a user slot (erase + program +
 * verify). Blocking foreground operation. */
int v3f_profile_store_commit_staging_to_slot(uint8_t slot_id, uint32_t len);

/* Logically delete one user slot. The operation is atomic and avoids an
 * immediate 32KB erase; a later upload erases the physical slot before use.
 * Deleting the active slot also persists Factory as the active slot. */
int v3f_profile_store_delete_slot(uint8_t slot_id);

/* Active-slot record (meta sector). get returns AIK_PROFILE_SLOT_FACTORY
 * when the record is missing or corrupt. */
uint8_t v3f_profile_store_get_active_slot(void);
int v3f_profile_store_set_active_slot(uint8_t slot_id);

#ifdef __cplusplus
}
#endif

#endif /* V3F_PROFILE_STORE_H */
