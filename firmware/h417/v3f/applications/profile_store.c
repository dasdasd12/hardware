#include "profile_store.h"

#include <string.h>

#include "ch32h417.h"
#include "aik_profile_format.h"

#define STORE_PAGE_SIZE   256UL
#define STORE_ERASE_SIZE  0x2000UL

#define META_MAGIC0 'A'
#define META_MAGIC1 'K'
#define META_MAGIC2 'A'
#define META_MAGIC3 'M'

typedef struct
{
    uint8_t magic[4];
    uint8_t version;
    uint8_t active_slot;
    uint16_t reserved;
    uint32_t crc32c;   /* over the first 8 bytes */
} store_meta_record_t;

typedef struct
{
    uint8_t active;
    uint32_t total_len;
    uint32_t written;
    uint32_t page_fill;
    uint32_t page_buf[STORE_PAGE_SIZE / 4U];
} store_staging_state_t;

static store_staging_state_t s_staging;

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

static uint32_t round_up(uint32_t value, uint32_t unit)
{
    return (value + unit - 1U) & ~(unit - 1U);
}

const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id)
{
    uint32_t addr = slot_flash_addr(slot_id);

    return (addr != 0U) ? (const uint8_t *)addr : 0;
}

const uint8_t *v3f_profile_store_staging_ptr(void)
{
    return (const uint8_t *)(V3F_PROFILE_FLASH_BASE +
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
    if(FLASH_ROM_ERASE(V3F_PROFILE_FLASH_BASE + V3F_PROFILE_STAGING_OFFSET,
                       erase_len) != FLASH_COMPLETE)
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
    uint32_t page_addr = V3F_PROFILE_FLASH_BASE + V3F_PROFILE_STAGING_OFFSET +
                         (s_staging.written - s_staging.page_fill);

    if(s_staging.page_fill == 0U)
    {
        return V3F_PROFILE_STORE_OK;
    }

    /* Pad the tail of a short page with 0xFF (flash erased state). */
    memset((uint8_t *)s_staging.page_buf + s_staging.page_fill, 0xFF,
           STORE_PAGE_SIZE - s_staging.page_fill);

    if(FLASH_ROM_WRITE(page_addr, s_staging.page_buf,
                       STORE_PAGE_SIZE) != FLASH_COMPLETE)
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
    uint32_t slot_addr = slot_flash_addr(slot_id);
    const uint8_t *staging = v3f_profile_store_staging_ptr();
    uint32_t padded = round_up(len, STORE_PAGE_SIZE);
    uint32_t offset;
    uint32_t page_buf[STORE_PAGE_SIZE / 4U];

    if((slot_addr == 0U) || (len == 0U) || (len > V3F_PROFILE_SLOT_SIZE))
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }

    if(FLASH_ROM_ERASE(slot_addr,
                       round_up(padded, STORE_ERASE_SIZE)) != FLASH_COMPLETE)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    for(offset = 0U; offset < padded; offset += STORE_PAGE_SIZE)
    {
        memcpy(page_buf, staging + offset, STORE_PAGE_SIZE);
        if(FLASH_ROM_WRITE(slot_addr + offset, page_buf,
                           STORE_PAGE_SIZE) != FLASH_COMPLETE)
        {
            return V3F_PROFILE_STORE_ERR_FLASH;
        }
    }

    if(memcmp((const uint8_t *)slot_addr, staging, len) != 0)
    {
        return V3F_PROFILE_STORE_ERR_VERIFY;
    }
    return V3F_PROFILE_STORE_OK;
}

static uint32_t meta_flash_addr(void)
{
    return V3F_PROFILE_FLASH_BASE + V3F_PROFILE_META_OFFSET;
}

uint8_t v3f_profile_store_get_active_slot(void)
{
    const store_meta_record_t *rec =
        (const store_meta_record_t *)meta_flash_addr();

    if((rec->magic[0] != META_MAGIC0) || (rec->magic[1] != META_MAGIC1) ||
       (rec->magic[2] != META_MAGIC2) || (rec->magic[3] != META_MAGIC3) ||
       (rec->version != 1U))
    {
        return AIK_PROFILE_SLOT_FACTORY;
    }
    if(rec->crc32c != aik_crc32c(0U, (const uint8_t *)rec, 8U))
    {
        return AIK_PROFILE_SLOT_FACTORY;
    }
    if(rec->active_slot >= AIK_PROFILE_SLOT_COUNT_TOTAL)
    {
        return AIK_PROFILE_SLOT_FACTORY;
    }
    return rec->active_slot;
}

int v3f_profile_store_set_active_slot(uint8_t slot_id)
{
    uint32_t page_buf[STORE_PAGE_SIZE / 4U];
    store_meta_record_t rec;

    if(slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL)
    {
        return V3F_PROFILE_STORE_ERR_PARAM;
    }

    if(v3f_profile_store_get_active_slot() == slot_id)
    {
        return V3F_PROFILE_STORE_OK;
    }

    memset(&rec, 0, sizeof(rec));
    rec.magic[0] = META_MAGIC0;
    rec.magic[1] = META_MAGIC1;
    rec.magic[2] = META_MAGIC2;
    rec.magic[3] = META_MAGIC3;
    rec.version = 1U;
    rec.active_slot = slot_id;
    rec.crc32c = aik_crc32c(0U, (const uint8_t *)&rec, 8U);

    if(FLASH_ROM_ERASE(meta_flash_addr(),
                       V3F_PROFILE_META_SIZE) != FLASH_COMPLETE)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    memset(page_buf, 0xFF, sizeof(page_buf));
    memcpy(page_buf, &rec, sizeof(rec));
    if(FLASH_ROM_WRITE(meta_flash_addr(), page_buf,
                       STORE_PAGE_SIZE) != FLASH_COMPLETE)
    {
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    return (v3f_profile_store_get_active_slot() == slot_id) ?
           V3F_PROFILE_STORE_OK : V3F_PROFILE_STORE_ERR_VERIFY;
}
