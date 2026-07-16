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
 *   +0x20000  meta sector   (8KB, active-slot record)
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

#define V3F_PROFILE_STORE_OK        0
#define V3F_PROFILE_STORE_ERR_PARAM -1
#define V3F_PROFILE_STORE_ERR_FLASH -2
#define V3F_PROFILE_STORE_ERR_STATE -3
#define V3F_PROFILE_STORE_ERR_VERIFY -4

/* Memory-mapped read pointer to a user slot (1..3); 0 for other ids.
 * Runtime code treats a fully erased package header as the embedded
 * factory Profile until the PC commits a package to that slot. */
const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id);

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

/* Active-slot record (meta sector). get returns AIK_PROFILE_SLOT_FACTORY
 * when the record is missing or corrupt. */
uint8_t v3f_profile_store_get_active_slot(void);
int v3f_profile_store_set_active_slot(uint8_t slot_id);

#ifdef __cplusplus
}
#endif

#endif /* V3F_PROFILE_STORE_H */
