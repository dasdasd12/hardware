/*
 * Host fault-injection test for the H417 profile Flash store.
 *
 * Build from firmware/:
 *   gcc -std=gnu99 -Wall -Wextra -Werror \
 *       -DV3F_PROFILE_STORE_HOST_TEST \
 *       -I common -I h417/v3f/applications \
 *       tests/host_profile_store_test.c \
 *       h417/v3f/applications/profile_store.c \
 *       -o build/host_profile_store_test
 */

#include <stdio.h>
#include <string.h>

#include "aik_profile_format.h"
#include "profile_store.h"

#define TEST_PACKAGE_SIZE 512U
#define META_A_BEGIN (V3F_PROFILE_FLASH_BASE + V3F_PROFILE_META_OFFSET)
#define META_A_END   (META_A_BEGIN + V3F_PROFILE_META_SIZE)
#define META_B_BEGIN (V3F_PROFILE_FLASH_BASE + V3F_PROFILE_META_B_OFFSET)
#define META_B_END   (META_B_BEGIN + V3F_PROFILE_META_SIZE)

static uint8_t s_flash[V3F_PROFILE_STORE_END_OFFSET];
static int s_failures;
static int s_bounds_error;
static uint32_t s_meta_erase_count;
static uint32_t s_last_meta_write_addr;
static uint32_t s_fail_write_begin;
static uint32_t s_fail_write_end;
static int s_fail_write_after;
static int s_fail_write_matches;

#define CHECK(condition) \
    do { \
        if(!(condition)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        } \
    } while(0)

static int flash_range(uint32_t address, uint32_t length,
                       uint32_t *offset)
{
    uint32_t relative;

    if(address < V3F_PROFILE_FLASH_BASE)
    {
        return -1;
    }
    relative = address - V3F_PROFILE_FLASH_BASE;
    if((relative > sizeof(s_flash)) ||
       (length > (uint32_t)sizeof(s_flash) - relative))
    {
        return -1;
    }
    if(offset != 0)
    {
        *offset = relative;
    }
    return 0;
}

static int address_is_metadata(uint32_t address)
{
    return (((address >= META_A_BEGIN) && (address < META_A_END)) ||
            ((address >= META_B_BEGIN) && (address < META_B_END)));
}

const uint8_t *v3f_profile_store_test_flash_ptr(uint32_t address)
{
    uint32_t offset;

    if(flash_range(address, 1U, &offset) != 0)
    {
        s_bounds_error = 1;
        return 0;
    }
    return &s_flash[offset];
}

int v3f_profile_store_test_flash_erase(uint32_t address, uint32_t length)
{
    uint32_t offset;

    if(flash_range(address, length, &offset) != 0)
    {
        s_bounds_error = 1;
        return -1;
    }
    if(address_is_metadata(address) != 0)
    {
        s_meta_erase_count++;
    }
    memset(&s_flash[offset], 0xFF, length);
    return 0;
}

int v3f_profile_store_test_flash_write(uint32_t address,
                                       const uint32_t *data,
                                       uint32_t length)
{
    const uint8_t *source = (const uint8_t *)data;
    uint32_t offset;
    uint32_t index;

    if(flash_range(address, length, &offset) != 0)
    {
        s_bounds_error = 1;
        return -1;
    }
    if(address_is_metadata(address) != 0)
    {
        s_last_meta_write_addr = address;
    }
    if((address < s_fail_write_end) &&
       ((address + length) > s_fail_write_begin))
    {
        if(s_fail_write_matches++ >= s_fail_write_after)
        {
            return -1;
        }
    }

    for(index = 0U; index < length; index++)
    {
        /* Real NOR Flash programming can only clear bits. */
        s_flash[offset + index] &= source[index];
    }
    return 0;
}

static void fail_writes(uint32_t begin, uint32_t length, int after)
{
    s_fail_write_begin = begin;
    s_fail_write_end = begin + length;
    s_fail_write_after = after;
    s_fail_write_matches = 0;
}

static void clear_write_failure(void)
{
    s_fail_write_begin = 0U;
    s_fail_write_end = 0U;
    s_fail_write_after = 0;
    s_fail_write_matches = 0;
}

static void make_package(uint8_t *package, uint8_t marker)
{
    aik_pkg_header_t *header = (aik_pkg_header_t *)package;
    uint32_t index;

    memset(package, 0, TEST_PACKAGE_SIZE);
    header->magic[0] = AIK_PKG_MAGIC0;
    header->magic[1] = AIK_PKG_MAGIC1;
    header->magic[2] = AIK_PKG_MAGIC2;
    header->magic[3] = AIK_PKG_MAGIC3;
    header->package_version = AIK_PKG_VERSION;
    header->header_size = AIK_PKG_HEADER_SIZE;
    header->profile_schema_version = AIK_PROFILE_SCHEMA_VERSION;
    header->revision = marker;
    header->total_size = TEST_PACKAGE_SIZE;
    for(index = AIK_PKG_HEADER_SIZE; index < TEST_PACKAGE_SIZE; index++)
    {
        package[index] = (uint8_t)(marker + index);
    }
}

static int stage_and_commit(uint8_t slot_id, const uint8_t *package)
{
    int status;

    status = v3f_profile_store_staging_begin(TEST_PACKAGE_SIZE);
    if(status != V3F_PROFILE_STORE_OK)
    {
        return status;
    }
    status = v3f_profile_store_staging_write(0U, package,
                                             TEST_PACKAGE_SIZE);
    if(status != V3F_PROFILE_STORE_OK)
    {
        return status;
    }
    status = v3f_profile_store_staging_finish();
    if(status != V3F_PROFILE_STORE_OK)
    {
        return status;
    }
    return v3f_profile_store_commit_staging_to_slot(
        slot_id, TEST_PACKAGE_SIZE);
}

static void expect_slot(uint8_t slot_id, const uint8_t *package)
{
    const uint8_t *stored = v3f_profile_store_slot_ptr(slot_id);

    CHECK(stored != 0);
    if(stored != 0)
    {
        CHECK(memcmp(stored, package, TEST_PACKAGE_SIZE) == 0);
    }
}

int main(void)
{
    uint8_t package_a[TEST_PACKAGE_SIZE];
    uint8_t package_b[TEST_PACKAGE_SIZE];
    uint8_t package_c[TEST_PACKAGE_SIZE];
    uint32_t metadata_erases_before;
    uint32_t index;
    int status;

    memset(s_flash, 0xFF, sizeof(s_flash));
    make_package(package_a, 0x11U);
    make_package(package_b, 0x44U);
    make_package(package_c, 0x88U);

    CHECK(v3f_profile_store_get_active_slot() ==
          AIK_PROFILE_SLOT_FACTORY);
    CHECK(v3f_profile_store_slot_ptr(1U) == 0);
    CHECK(v3f_profile_store_slot_present(1U) == 0U);

    CHECK(stage_and_commit(1U, package_a) == V3F_PROFILE_STORE_OK);
    expect_slot(1U, package_a);
    CHECK(v3f_profile_store_slot_present(1U) == 1U);
    CHECK(v3f_profile_store_set_active_slot(1U) == V3F_PROFILE_STORE_OK);
    CHECK(v3f_profile_store_get_active_slot() == 1U);

    /* Fail halfway through replacing slot 1. The pending transaction must
     * expose the previous package from rollback after a simulated reboot. */
    CHECK(v3f_profile_store_staging_begin(TEST_PACKAGE_SIZE) ==
          V3F_PROFILE_STORE_OK);
    CHECK(v3f_profile_store_staging_write(0U, package_b,
                                           TEST_PACKAGE_SIZE) ==
          V3F_PROFILE_STORE_OK);
    CHECK(v3f_profile_store_staging_finish() == V3F_PROFILE_STORE_OK);
    fail_writes(V3F_PROFILE_FLASH_BASE, V3F_PROFILE_SLOT_SIZE, 1);
    status = v3f_profile_store_commit_staging_to_slot(
        1U, TEST_PACKAGE_SIZE);
    CHECK(status == V3F_PROFILE_STORE_ERR_FLASH);
    CHECK(s_fail_write_matches == 2);
    clear_write_failure();
    v3f_profile_store_staging_abort();
    expect_slot(1U, package_a);
    CHECK(v3f_profile_store_get_active_slot() == 1U);

    /* A subsequent foreground mutation repairs the target and can replace
     * it normally. */
    CHECK(stage_and_commit(1U, package_b) == V3F_PROFILE_STORE_OK);
    expect_slot(1U, package_b);

    /* Lose power while appending the final commit record. The previous
     * package remains the committed view even though target programming
     * itself completed. */
    CHECK(v3f_profile_store_staging_begin(TEST_PACKAGE_SIZE) ==
          V3F_PROFILE_STORE_OK);
    CHECK(v3f_profile_store_staging_write(0U, package_c,
                                           TEST_PACKAGE_SIZE) ==
          V3F_PROFILE_STORE_OK);
    CHECK(v3f_profile_store_staging_finish() == V3F_PROFILE_STORE_OK);
    /* The journal is still in sector A here: allow the pending record,
     * then fail the final commit record. */
    fail_writes(META_A_BEGIN, V3F_PROFILE_META_SIZE, 1);
    status = v3f_profile_store_commit_staging_to_slot(
        1U, TEST_PACKAGE_SIZE);
    CHECK(status == V3F_PROFILE_STORE_ERR_FLASH);
    CHECK(s_fail_write_matches == 2);
    CHECK(memcmp(&s_flash[0], package_c, TEST_PACKAGE_SIZE) == 0);
    clear_write_failure();
    v3f_profile_store_staging_abort();
    expect_slot(1U, package_b);

    CHECK(stage_and_commit(2U, package_c) == V3F_PROFILE_STORE_OK);
    expect_slot(2U, package_c);
    CHECK(v3f_profile_store_set_active_slot(2U) == V3F_PROFILE_STORE_OK);
    CHECK(v3f_profile_store_get_active_slot() == 2U);

    /* A post-commit payload bit flip is rejected by the stored CRC. With
     * no previous package for this slot, runtime will use Factory. */
    s_flash[V3F_PROFILE_SLOT_SIZE + 100U] ^= 0x01U;
    CHECK(v3f_profile_store_slot_ptr(2U) == 0);
    CHECK(v3f_profile_store_slot_present(2U) == 0U);

    /* Corrupt the newest metadata record. Boot selection must fall back to
     * the previous valid journal entry rather than an arbitrary slot. */
    CHECK(s_last_meta_write_addr != 0U);
    s_flash[(s_last_meta_write_addr - V3F_PROFILE_FLASH_BASE) + 44U] ^= 0x01U;
    CHECK(v3f_profile_store_get_active_slot() == 1U);

    /* Deleting the active package is atomic and selects Factory. */
    CHECK(v3f_profile_store_delete_slot(1U) == V3F_PROFILE_STORE_OK);
    CHECK(v3f_profile_store_slot_present(1U) == 0U);
    CHECK(v3f_profile_store_slot_ptr(1U) == 0);
    CHECK(v3f_profile_store_get_active_slot() ==
          AIK_PROFILE_SLOT_FACTORY);

    /* Appending active-slot records amortizes erase wear. One erase per
     * switch would produce 100 erases here; the journal should need only
     * a handful when rolling between its two sectors. */
    metadata_erases_before = s_meta_erase_count;
    for(index = 0U; index < 100U; index++)
    {
        uint8_t slot = (uint8_t)((index & 1U) ? 2U : 3U);

        CHECK(v3f_profile_store_set_active_slot(slot) ==
              V3F_PROFILE_STORE_OK);
    }
    CHECK((s_meta_erase_count - metadata_erases_before) < 6U);
    CHECK(s_bounds_error == 0);

    if(s_failures == 0)
    {
        printf("host_profile_store_test: all checks passed\n");
        return 0;
    }
    printf("host_profile_store_test: %d failures\n", s_failures);
    return 1;
}
