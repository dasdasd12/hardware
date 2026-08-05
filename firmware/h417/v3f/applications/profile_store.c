#include "profile_store.h"

#include <stddef.h>
#include <string.h>

#include "aik_profile_format.h"

#ifdef V3F_PROFILE_STORE_HOST_TEST
const uint8_t *v3f_profile_store_test_flash_ptr(uint32_t address);
int v3f_profile_store_test_flash_erase(uint32_t address, uint32_t length);
int v3f_profile_store_test_flash_write(uint32_t address,
                                       const uint32_t *data,
                                       uint32_t length);
#else
#include "ch32h417.h"
#endif

#define STORE_PAGE_SIZE          256UL
#define STORE_ERASE_SIZE         0x2000UL
#define STORE_META_PAGE_COUNT    (V3F_PROFILE_META_SIZE / STORE_PAGE_SIZE)

#define META_MAGIC0 'A'
#define META_MAGIC1 'K'
#define META_MAGIC2 'A'
#define META_MAGIC3 'M'

#define META_V2_MAGIC0 'A'
#define META_V2_MAGIC1 'K'
#define META_V2_MAGIC2 'M'
#define META_V2_MAGIC3 '2'
#define META_V2_VERSION 2U

#define META_FLAG_TRANSACTION_PENDING 0x01U
#define META_FLAG_ROLLBACK_VALID      0x02U
#define META_FLAG_MASK                0x03U
#define META_SLOT_MASK ((uint8_t)((1U << AIK_PROFILE_USER_SLOT_COUNT) - 1U))

typedef struct
{
    uint8_t magic[4];
    uint8_t version;
    uint8_t active_slot;
    uint16_t reserved;
    uint32_t crc32c;
} store_meta_v1_record_t;

/* One metadata record occupies one flash programming page. A record is
 * appended only after the data it references has been verified. */
typedef struct
{
    uint8_t magic[4];
    uint8_t version;
    uint8_t active_slot;
    uint8_t valid_mask;
    uint8_t flags;
    uint32_t generation;
    uint32_t slot_length[AIK_PROFILE_USER_SLOT_COUNT];
    uint32_t slot_crc32c[AIK_PROFILE_USER_SLOT_COUNT];
    uint8_t transaction_slot;
    uint8_t rollback_slot;
    uint8_t reserved0[2];
    uint32_t rollback_length;
    uint32_t rollback_crc32c;
    uint32_t record_crc32c;
    uint8_t reserved1[STORE_PAGE_SIZE - 52U];
} store_meta_record_t;

typedef char store_meta_record_size_check[
    (sizeof(store_meta_record_t) == STORE_PAGE_SIZE) ? 1 : -1];

typedef struct
{
    uint8_t active;
    uint32_t total_len;
    uint32_t written;
    uint32_t page_fill;
    uint32_t page_buf[STORE_PAGE_SIZE / 4U];
} store_staging_state_t;

static store_staging_state_t s_staging;

static const uint8_t *store_flash_ptr(uint32_t address)
{
#ifdef V3F_PROFILE_STORE_HOST_TEST
    return v3f_profile_store_test_flash_ptr(address);
#else
    return (const uint8_t *)address;
#endif
}

static int store_flash_erase(uint32_t address, uint32_t length)
{
#ifdef V3F_PROFILE_STORE_HOST_TEST
    return (v3f_profile_store_test_flash_erase(address, length) == 0) ?
           V3F_PROFILE_STORE_OK : V3F_PROFILE_STORE_ERR_FLASH;
#else
    return (FLASH_ROM_ERASE(address, length) == FLASH_COMPLETE) ?
           V3F_PROFILE_STORE_OK : V3F_PROFILE_STORE_ERR_FLASH;
#endif
}

static int store_flash_write(uint32_t address, const uint32_t *data,
                             uint32_t length)
{
#ifdef V3F_PROFILE_STORE_HOST_TEST
    return (v3f_profile_store_test_flash_write(address, data, length) == 0) ?
           V3F_PROFILE_STORE_OK : V3F_PROFILE_STORE_ERR_FLASH;
#else
    return (FLASH_ROM_WRITE(address, (uint32_t *)data, length) ==
            FLASH_COMPLETE) ?
           V3F_PROFILE_STORE_OK : V3F_PROFILE_STORE_ERR_FLASH;
#endif
}

static uint32_t slot_flash_addr(uint8_t slot_id)
{
    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return 0U;
    }
    return V3F_PROFILE_FLASH_BASE +
           ((uint32_t)(slot_id - AIK_PROFILE_USER_SLOT_FIRST) *
            V3F_PROFILE_SLOT_SIZE);
}

static uint32_t rollback_flash_addr(void)
{
    return V3F_PROFILE_FLASH_BASE + V3F_PROFILE_ROLLBACK_OFFSET;
}

static uint32_t meta_sector_addr(uint8_t sector)
{
    return V3F_PROFILE_FLASH_BASE +
           ((sector == 0U) ? V3F_PROFILE_META_OFFSET :
                             V3F_PROFILE_META_B_OFFSET);
}

static uint32_t round_up(uint32_t value, uint32_t unit)
{
    return (value + unit - 1U) & ~(unit - 1U);
}

static uint8_t generation_newer(uint32_t candidate, uint32_t reference)
{
    return (uint8_t)((int32_t)(candidate - reference) > 0);
}

static uint32_t meta_record_crc(const store_meta_record_t *record)
{
    return aik_crc32c(0U, (const uint8_t *)record,
                      (uint32_t)offsetof(store_meta_record_t,
                                         record_crc32c));
}

static uint8_t blob_valid(const uint8_t *data, uint32_t length,
                          uint32_t crc32c)
{
    if((data == 0) || (length < AIK_PKG_HEADER_SIZE) ||
       (length > V3F_PROFILE_SLOT_SIZE))
    {
        return 0U;
    }
    return (uint8_t)(aik_crc32c(0U, data, length) == crc32c);
}

static uint8_t package_descriptor(const uint8_t *data,
                                  uint32_t *length,
                                  uint32_t *crc32c)
{
    const aik_pkg_header_t *header = (const aik_pkg_header_t *)data;

    if((data == 0) || (length == 0) || (crc32c == 0))
    {
        return 0U;
    }
    if((header->magic[0] != AIK_PKG_MAGIC0) ||
       (header->magic[1] != AIK_PKG_MAGIC1) ||
       (header->magic[2] != AIK_PKG_MAGIC2) ||
       (header->magic[3] != AIK_PKG_MAGIC3) ||
       (header->package_version != AIK_PKG_VERSION) ||
       (header->header_size != AIK_PKG_HEADER_SIZE) ||
       (header->total_size < AIK_PKG_HEADER_SIZE) ||
       (header->total_size > V3F_PROFILE_SLOT_SIZE))
    {
        return 0U;
    }

    *length = header->total_size;
    *crc32c = aik_crc32c(0U, data, *length);
    return 1U;
}

static uint8_t meta_record_valid(const store_meta_record_t *record)
{
    uint8_t slot;

    if((record == 0) ||
       (record->magic[0] != META_V2_MAGIC0) ||
       (record->magic[1] != META_V2_MAGIC1) ||
       (record->magic[2] != META_V2_MAGIC2) ||
       (record->magic[3] != META_V2_MAGIC3) ||
       (record->version != META_V2_VERSION) ||
       (record->active_slot >= AIK_PROFILE_SLOT_COUNT_TOTAL) ||
       ((record->valid_mask & (uint8_t)~META_SLOT_MASK) != 0U) ||
       ((record->flags & (uint8_t)~META_FLAG_MASK) != 0U) ||
       (record->record_crc32c != meta_record_crc(record)))
    {
        return 0U;
    }

    if(((record->flags & META_FLAG_TRANSACTION_PENDING) != 0U) &&
       ((record->transaction_slot < AIK_PROFILE_USER_SLOT_FIRST) ||
        (record->transaction_slot >= AIK_PROFILE_SLOT_COUNT_TOTAL)))
    {
        return 0U;
    }
    if(((record->flags & META_FLAG_ROLLBACK_VALID) != 0U) &&
       ((record->rollback_slot < AIK_PROFILE_USER_SLOT_FIRST) ||
        (record->rollback_slot >= AIK_PROFILE_SLOT_COUNT_TOTAL) ||
        (record->rollback_length < AIK_PKG_HEADER_SIZE) ||
        (record->rollback_length > V3F_PROFILE_SLOT_SIZE)))
    {
        return 0U;
    }

    for(slot = 0U; slot < AIK_PROFILE_USER_SLOT_COUNT; slot++)
    {
        if(((record->valid_mask & (uint8_t)(1U << slot)) != 0U) &&
           ((record->slot_length[slot] < AIK_PKG_HEADER_SIZE) ||
            (record->slot_length[slot] > V3F_PROFILE_SLOT_SIZE)))
        {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t page_is_erased(const uint8_t *page)
{
    uint32_t index;

    if(page == 0)
    {
        return 0U;
    }
    for(index = 0U; index < STORE_PAGE_SIZE; index++)
    {
        if(page[index] != 0xFFU)
        {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t legacy_active_slot(void)
{
    const store_meta_v1_record_t *record =
        (const store_meta_v1_record_t *)store_flash_ptr(meta_sector_addr(0U));

    if((record == 0) ||
       (record->magic[0] != META_MAGIC0) ||
       (record->magic[1] != META_MAGIC1) ||
       (record->magic[2] != META_MAGIC2) ||
       (record->magic[3] != META_MAGIC3) ||
       (record->version != 1U) ||
       (record->active_slot >= AIK_PROFILE_SLOT_COUNT_TOTAL) ||
       (record->crc32c != aik_crc32c(0U, (const uint8_t *)record, 8U)))
    {
        return AIK_PROFILE_SLOT_FACTORY;
    }
    return record->active_slot;
}

static void meta_build_legacy(store_meta_record_t *record)
{
    uint8_t index;

    memset(record, 0, sizeof(*record));
    record->magic[0] = META_V2_MAGIC0;
    record->magic[1] = META_V2_MAGIC1;
    record->magic[2] = META_V2_MAGIC2;
    record->magic[3] = META_V2_MAGIC3;
    record->version = META_V2_VERSION;
    record->active_slot = legacy_active_slot();
    record->transaction_slot = 0xFFU;
    record->rollback_slot = 0xFFU;

    for(index = 0U; index < AIK_PROFILE_USER_SLOT_COUNT; index++)
    {
        uint8_t slot_id = (uint8_t)(AIK_PROFILE_USER_SLOT_FIRST + index);
        const uint8_t *data = store_flash_ptr(slot_flash_addr(slot_id));

        if(package_descriptor(data, &record->slot_length[index],
                              &record->slot_crc32c[index]) != 0U)
        {
            record->valid_mask |= (uint8_t)(1U << index);
        }
    }
    record->record_crc32c = meta_record_crc(record);
}

static void meta_load_current(store_meta_record_t *record,
                              uint32_t *record_address)
{
    uint8_t found = 0U;
    uint8_t sector;

    for(sector = 0U; sector < 2U; sector++)
    {
        uint32_t base = meta_sector_addr(sector);
        uint32_t page;

        for(page = 0U; page < STORE_META_PAGE_COUNT; page++)
        {
            uint32_t address = base + page * STORE_PAGE_SIZE;
            const store_meta_record_t *candidate =
                (const store_meta_record_t *)store_flash_ptr(address);

            if(meta_record_valid(candidate) == 0U)
            {
                continue;
            }
            if((found == 0U) ||
               (generation_newer(candidate->generation,
                                 record->generation) != 0U))
            {
                *record = *candidate;
                if(record_address != 0)
                {
                    *record_address = address;
                }
                found = 1U;
            }
        }
    }

    if(found == 0U)
    {
        meta_build_legacy(record);
        if(record_address != 0)
        {
            *record_address = 0U;
        }
    }
}

static uint32_t find_erased_meta_page(uint8_t sector)
{
    uint32_t base = meta_sector_addr(sector);
    uint32_t page;

    for(page = 0U; page < STORE_META_PAGE_COUNT; page++)
    {
        uint32_t address = base + page * STORE_PAGE_SIZE;

        if(page_is_erased(store_flash_ptr(address)) != 0U)
        {
            return address;
        }
    }
    return 0U;
}

static int meta_append(store_meta_record_t *next)
{
    store_meta_record_t current;
    uint32_t current_address;
    uint32_t destination;
    uint8_t preferred_sector;

    if(next == 0)
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }

    meta_load_current(&current, &current_address);
    next->magic[0] = META_V2_MAGIC0;
    next->magic[1] = META_V2_MAGIC1;
    next->magic[2] = META_V2_MAGIC2;
    next->magic[3] = META_V2_MAGIC3;
    next->version = META_V2_VERSION;
    next->generation = current.generation + 1U;
    next->record_crc32c = meta_record_crc(next);

    preferred_sector = (uint8_t)(
        (current_address >= meta_sector_addr(1U)) ? 1U : 0U);
    destination = find_erased_meta_page(preferred_sector);
    if(destination == 0U)
    {
        uint8_t other = (uint8_t)(preferred_sector ^ 1U);

        if(store_flash_erase(meta_sector_addr(other),
                             V3F_PROFILE_META_SIZE) !=
           V3F_PROFILE_STORE_OK)
        {
            return V3F_PROFILE_STORE_ERR_FLASH;
        }
        destination = meta_sector_addr(other);
    }

    if(store_flash_write(destination, (const uint32_t *)next,
                         STORE_PAGE_SIZE) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    if((meta_record_valid(
            (const store_meta_record_t *)store_flash_ptr(destination)) == 0U) ||
       (((const store_meta_record_t *)store_flash_ptr(destination))->generation !=
        next->generation))
    {
        return V3F_PROFILE_STORE_ERR_VERIFY;
    }
    return V3F_PROFILE_STORE_OK;
}

static const uint8_t *resolve_slot(const store_meta_record_t *record,
                                   uint8_t slot_id,
                                   uint32_t *length,
                                   uint32_t *crc32c)
{
    uint8_t index;
    const uint8_t *target;
    const uint8_t *rollback = store_flash_ptr(rollback_flash_addr());

    if((record == 0) ||
       (slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return 0;
    }
    index = (uint8_t)(slot_id - AIK_PROFILE_USER_SLOT_FIRST);

    if(((record->flags & META_FLAG_TRANSACTION_PENDING) != 0U) &&
       (record->transaction_slot == slot_id))
    {
        if(((record->flags & META_FLAG_ROLLBACK_VALID) != 0U) &&
           (record->rollback_slot == slot_id) &&
           (blob_valid(rollback, record->rollback_length,
                       record->rollback_crc32c) != 0U))
        {
            if(length != 0) *length = record->rollback_length;
            if(crc32c != 0) *crc32c = record->rollback_crc32c;
            return rollback;
        }
        return 0;
    }

    target = store_flash_ptr(slot_flash_addr(slot_id));
    if(((record->valid_mask & (uint8_t)(1U << index)) != 0U) &&
       (blob_valid(target, record->slot_length[index],
                   record->slot_crc32c[index]) != 0U))
    {
        if(length != 0) *length = record->slot_length[index];
        if(crc32c != 0) *crc32c = record->slot_crc32c[index];
        return target;
    }

    if(((record->flags & META_FLAG_ROLLBACK_VALID) != 0U) &&
       (record->rollback_slot == slot_id) &&
       (blob_valid(rollback, record->rollback_length,
                   record->rollback_crc32c) != 0U))
    {
        if(length != 0) *length = record->rollback_length;
        if(crc32c != 0) *crc32c = record->rollback_crc32c;
        return rollback;
    }
    return 0;
}

static int copy_blob_to_flash(uint32_t destination,
                              const uint8_t *source,
                              uint32_t length)
{
    uint32_t offset;
    uint32_t padded;
    uint32_t page_buf[STORE_PAGE_SIZE / 4U];

    if((source == 0) || (length < AIK_PKG_HEADER_SIZE) ||
       (length > V3F_PROFILE_SLOT_SIZE))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }

    if(store_flash_erase(destination, V3F_PROFILE_SLOT_SIZE) !=
       V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    padded = round_up(length, STORE_PAGE_SIZE);
    for(offset = 0U; offset < padded; offset += STORE_PAGE_SIZE)
    {
        uint32_t take = ((length - offset) < STORE_PAGE_SIZE) ?
                        (length - offset) : STORE_PAGE_SIZE;

        memset(page_buf, 0xFF, sizeof(page_buf));
        memcpy(page_buf, source + offset, take);
        if(store_flash_write(destination + offset, page_buf,
                             STORE_PAGE_SIZE) != V3F_PROFILE_STORE_OK)
        {
            return V3F_PROFILE_STORE_ERR_FLASH;
        }
    }

    if(memcmp(store_flash_ptr(destination), source, length) != 0)
    {
        return V3F_PROFILE_STORE_ERR_VERIFY;
    }
    return V3F_PROFILE_STORE_OK;
}

static int recover_pending_transaction(store_meta_record_t *record)
{
    store_meta_record_t recovered;
    uint8_t slot_id;
    uint8_t index;
    const uint8_t *rollback = store_flash_ptr(rollback_flash_addr());

    if((record->flags & META_FLAG_TRANSACTION_PENDING) == 0U)
    {
        return V3F_PROFILE_STORE_OK;
    }

    recovered = *record;
    slot_id = record->transaction_slot;
    index = (uint8_t)(slot_id - AIK_PROFILE_USER_SLOT_FIRST);
    if(((record->flags & META_FLAG_ROLLBACK_VALID) != 0U) &&
       (record->rollback_slot == slot_id) &&
       (blob_valid(rollback, record->rollback_length,
                   record->rollback_crc32c) != 0U))
    {
        if(copy_blob_to_flash(slot_flash_addr(slot_id), rollback,
                              record->rollback_length) !=
           V3F_PROFILE_STORE_OK)
        {
            return V3F_PROFILE_STORE_ERR_FLASH;
        }
        recovered.valid_mask |= (uint8_t)(1U << index);
        recovered.slot_length[index] = record->rollback_length;
        recovered.slot_crc32c[index] = record->rollback_crc32c;
    }
    else
    {
        recovered.valid_mask &= (uint8_t)~(uint8_t)(1U << index);
        recovered.slot_length[index] = 0U;
        recovered.slot_crc32c[index] = 0U;
    }
    recovered.flags &= (uint8_t)~META_FLAG_TRANSACTION_PENDING;
    recovered.transaction_slot = 0xFFU;

    if(meta_append(&recovered) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    meta_load_current(record, 0);
    return V3F_PROFILE_STORE_OK;
}

static int repair_from_rollback_if_needed(store_meta_record_t *record)
{
    store_meta_record_t repaired;
    uint8_t slot_id;
    uint8_t index;
    const uint8_t *target;
    const uint8_t *rollback = store_flash_ptr(rollback_flash_addr());

    if((record->flags & META_FLAG_ROLLBACK_VALID) == 0U)
    {
        return V3F_PROFILE_STORE_OK;
    }
    slot_id = record->rollback_slot;
    index = (uint8_t)(slot_id - AIK_PROFILE_USER_SLOT_FIRST);
    if((record->valid_mask & (uint8_t)(1U << index)) == 0U)
    {
        return V3F_PROFILE_STORE_OK;
    }

    target = store_flash_ptr(slot_flash_addr(slot_id));
    if(blob_valid(target, record->slot_length[index],
                  record->slot_crc32c[index]) != 0U)
    {
        return V3F_PROFILE_STORE_OK;
    }
    if(blob_valid(rollback, record->rollback_length,
                  record->rollback_crc32c) == 0U)
    {
        return V3F_PROFILE_STORE_OK;
    }
    if(copy_blob_to_flash(slot_flash_addr(slot_id), rollback,
                          record->rollback_length) !=
       V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    repaired = *record;
    repaired.slot_length[index] = record->rollback_length;
    repaired.slot_crc32c[index] = record->rollback_crc32c;
    if(meta_append(&repaired) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    meta_load_current(record, 0);
    return V3F_PROFILE_STORE_OK;
}

static int prepare_mutation(store_meta_record_t *record)
{
    meta_load_current(record, 0);
    if(recover_pending_transaction(record) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    if(repair_from_rollback_if_needed(record) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    return V3F_PROFILE_STORE_OK;
}

const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id)
{
    store_meta_record_t record;

    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return 0;
    }
    meta_load_current(&record, 0);
    return resolve_slot(&record, slot_id, 0, 0);
}

uint8_t v3f_profile_store_slot_present(uint8_t slot_id)
{
    return (uint8_t)(v3f_profile_store_slot_ptr(slot_id) != 0);
}

const uint8_t *v3f_profile_store_staging_ptr(void)
{
    return store_flash_ptr(V3F_PROFILE_FLASH_BASE +
                           V3F_PROFILE_STAGING_OFFSET);
}

int v3f_profile_store_staging_begin(uint32_t total_len)
{
    uint32_t erase_len;

    if((total_len == 0U) || (total_len > V3F_PROFILE_SLOT_SIZE))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }

    erase_len = round_up(total_len, STORE_ERASE_SIZE);
    if(store_flash_erase(V3F_PROFILE_FLASH_BASE +
                         V3F_PROFILE_STAGING_OFFSET,
                         erase_len) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    memset(&s_staging, 0, sizeof(s_staging));
    s_staging.active = 1U;
    s_staging.total_len = total_len;
    return V3F_PROFILE_STORE_OK;
}

static int staging_flush_page(void)
{
    uint32_t page_addr = V3F_PROFILE_FLASH_BASE +
                         V3F_PROFILE_STAGING_OFFSET +
                         (s_staging.written - s_staging.page_fill);

    if(s_staging.page_fill == 0U)
    {
        return V3F_PROFILE_STORE_OK;
    }

    memset((uint8_t *)s_staging.page_buf + s_staging.page_fill, 0xFF,
           STORE_PAGE_SIZE - s_staging.page_fill);
    if(store_flash_write(page_addr, s_staging.page_buf,
                         STORE_PAGE_SIZE) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    s_staging.page_fill = 0U;
    return V3F_PROFILE_STORE_OK;
}

int v3f_profile_store_staging_write(uint32_t offset, const uint8_t *data,
                                    uint32_t len)
{
    if((s_staging.active == 0U) || (data == 0))
    {
        return V3F_PROFILE_STORE_ERR_STATE;
    }
    if((offset != s_staging.written) ||
       ((offset + len) > s_staging.total_len))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }

    while(len != 0U)
    {
        uint32_t space = STORE_PAGE_SIZE - s_staging.page_fill;
        uint32_t take = (len < space) ? len : space;

        memcpy((uint8_t *)s_staging.page_buf + s_staging.page_fill, data,
               take);
        s_staging.page_fill += take;
        s_staging.written += take;
        data += take;
        len -= take;

        if(s_staging.page_fill == STORE_PAGE_SIZE)
        {
            int status = staging_flush_page();

            if(status != V3F_PROFILE_STORE_OK)
            {
                s_staging.active = 0U;
                return status;
            }
        }
    }
    return V3F_PROFILE_STORE_OK;
}

int v3f_profile_store_staging_finish(void)
{
    int status;

    if(s_staging.active == 0U)
    {
        return V3F_PROFILE_STORE_ERR_STATE;
    }
    if(s_staging.written != s_staging.total_len)
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }

    status = staging_flush_page();
    s_staging.active = 0U;
    return status;
}

void v3f_profile_store_staging_abort(void)
{
    memset(&s_staging, 0, sizeof(s_staging));
}

int v3f_profile_store_commit_staging_to_slot(uint8_t slot_id, uint32_t len)
{
    store_meta_record_t current;
    store_meta_record_t pending;
    store_meta_record_t committed;
    const uint8_t *staging = v3f_profile_store_staging_ptr();
    const uint8_t *old_data;
    uint32_t old_length = 0U;
    uint32_t old_crc32c = 0U;
    uint32_t new_crc32c;
    uint8_t index;

    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL) ||
       (len < AIK_PKG_HEADER_SIZE) || (len > V3F_PROFILE_SLOT_SIZE))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    if(prepare_mutation(&current) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    old_data = resolve_slot(&current, slot_id, &old_length, &old_crc32c);
    pending = current;
    pending.flags &= (uint8_t)~META_FLAG_ROLLBACK_VALID;
    pending.rollback_slot = 0xFFU;
    pending.rollback_length = 0U;
    pending.rollback_crc32c = 0U;
    if(old_data != 0)
    {
        const uint8_t *rollback = store_flash_ptr(rollback_flash_addr());

        if(old_data != rollback)
        {
            if(copy_blob_to_flash(rollback_flash_addr(), old_data,
                                  old_length) != V3F_PROFILE_STORE_OK)
            {
                return V3F_PROFILE_STORE_ERR_FLASH;
            }
        }
        pending.flags |= META_FLAG_ROLLBACK_VALID;
        pending.rollback_slot = slot_id;
        pending.rollback_length = old_length;
        pending.rollback_crc32c = old_crc32c;
    }

    pending.flags |= META_FLAG_TRANSACTION_PENDING;
    pending.transaction_slot = slot_id;
    if(meta_append(&pending) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    if(copy_blob_to_flash(slot_flash_addr(slot_id), staging, len) !=
       V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    new_crc32c = aik_crc32c(0U,
                            store_flash_ptr(slot_flash_addr(slot_id)), len);
    if(new_crc32c != aik_crc32c(0U, staging, len))
    {
        return V3F_PROFILE_STORE_ERR_VERIFY;
    }

    meta_load_current(&committed, 0);
    index = (uint8_t)(slot_id - AIK_PROFILE_USER_SLOT_FIRST);
    committed.valid_mask |= (uint8_t)(1U << index);
    committed.slot_length[index] = len;
    committed.slot_crc32c[index] = new_crc32c;
    committed.flags &= (uint8_t)~META_FLAG_TRANSACTION_PENDING;
    committed.transaction_slot = 0xFFU;
    if(meta_append(&committed) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    return V3F_PROFILE_STORE_OK;
}

int v3f_profile_store_delete_slot(uint8_t slot_id)
{
    store_meta_record_t record;
    uint8_t index;

    if((slot_id < AIK_PROFILE_USER_SLOT_FIRST) ||
       (slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    if(prepare_mutation(&record) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    index = (uint8_t)(slot_id - AIK_PROFILE_USER_SLOT_FIRST);
    record.valid_mask &= (uint8_t)~(uint8_t)(1U << index);
    record.slot_length[index] = 0U;
    record.slot_crc32c[index] = 0U;
    if(record.active_slot == slot_id)
    {
        record.active_slot = AIK_PROFILE_SLOT_FACTORY;
    }
    if(((record.flags & META_FLAG_ROLLBACK_VALID) != 0U) &&
       (record.rollback_slot == slot_id))
    {
        record.flags &= (uint8_t)~META_FLAG_ROLLBACK_VALID;
        record.rollback_slot = 0xFFU;
        record.rollback_length = 0U;
        record.rollback_crc32c = 0U;
    }
    return meta_append(&record);
}

uint8_t v3f_profile_store_get_active_slot(void)
{
    store_meta_record_t record;

    meta_load_current(&record, 0);
    return record.active_slot;
}

int v3f_profile_store_set_active_slot(uint8_t slot_id)
{
    store_meta_record_t record;

    if(slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL)
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }
    if(prepare_mutation(&record) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    if(record.active_slot == slot_id)
    {
        return V3F_PROFILE_STORE_OK;
    }

    record.active_slot = slot_id;
    if(meta_append(&record) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }
    return (v3f_profile_store_get_active_slot() == slot_id) ?
           V3F_PROFILE_STORE_OK : V3F_PROFILE_STORE_ERR_VERIFY;
}
