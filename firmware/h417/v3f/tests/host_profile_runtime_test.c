/*
 * Host-side equivalence test for the V3F profile runtime.
 *
 * Feeds the generated factory AKPK image to the firmware parser and
 * asserts the installed runtime reproduces the legacy hardcoded table
 * (default_profile.c) byte for byte, plus local-control and AKHR
 * derivation checks.
 *
 * Build & run (from hardware/firmware/h417):
 *   gcc -std=gnu99 -Wall -Wextra -I ../common -I v3f/applications \
 *       v3f/tests/host_profile_runtime_test.c \
 *       v3f/applications/profile_runtime.c \
 *       v3f/applications/factory_profile_image.c \
 *       v3f/applications/half_state.c \
 *       v3f/applications/default_profile.c \
 *       -o build/host_profile_runtime_test && build/host_profile_runtime_test
 */

#include <stdio.h>
#include <string.h>

#include "profile_runtime.h"
#include "profile_store.h"
#include "default_profile.h"
#include "factory_profile_image.h"

static int s_failures;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

/* profile_store stubs: boot path sees no user slots. */
const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id)
{
    (void)slot_id;
    return 0;
}

const uint8_t *v3f_profile_store_staging_ptr(void) { return 0; }
int v3f_profile_store_staging_begin(uint32_t total_len)
{
    (void)total_len;
    return V3F_PROFILE_STORE_ERR_FLASH;
}
int v3f_profile_store_staging_write(uint32_t offset, const uint8_t *data,
                                    uint32_t len)
{
    (void)offset; (void)data; (void)len;
    return V3F_PROFILE_STORE_ERR_FLASH;
}
int v3f_profile_store_staging_finish(void) { return V3F_PROFILE_STORE_ERR_FLASH; }
void v3f_profile_store_staging_abort(void) {}
int v3f_profile_store_commit_staging_to_slot(uint8_t slot_id, uint32_t len)
{
    (void)slot_id; (void)len;
    return V3F_PROFILE_STORE_ERR_FLASH;
}
uint8_t v3f_profile_store_get_active_slot(void) { return 0U; }
int v3f_profile_store_set_active_slot(uint8_t slot_id)
{
    (void)slot_id;
    return V3F_PROFILE_STORE_ERR_FLASH;
}

static void set_key(v3f_global_key_state_t *keys, uint8_t key_id)
{
    keys->down[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
}

static uint8_t nkro_usage_set(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                              uint8_t usage)
{
    uint8_t bit_index = (uint8_t)(usage - 0x04U);
    uint8_t byte_index = (uint8_t)(2U + (bit_index >> 3));

    return (uint8_t)((nkro16[byte_index] >> (bit_index & 7U)) & 1U);
}

static void test_key_table_equivalence(void)
{
    uint8_t key_id;

    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        v3f_global_key_state_t keys;
        uint8_t legacy[AIK_NKRO_REPORT_BYTES];
        uint8_t runtime[AIK_NKRO_REPORT_BYTES];

        memset(&keys, 0, sizeof(keys));
        set_key(&keys, key_id);

        v3f_default_profile_build_nkro16(&keys, legacy);
        v3f_profile_runtime_build_nkro16(&keys, runtime);

        if(memcmp(legacy, runtime, sizeof(legacy)) != 0)
        {
            s_failures++;
            printf("FAIL key %u: legacy!=runtime\n", key_id);
        }
    }

    /* Chord: left ctrl (76) + shift (72) + A (65) + space (40). */
    {
        v3f_global_key_state_t keys;
        uint8_t legacy[AIK_NKRO_REPORT_BYTES];
        uint8_t runtime[AIK_NKRO_REPORT_BYTES];

        memset(&keys, 0, sizeof(keys));
        set_key(&keys, 76U);
        set_key(&keys, 72U);
        set_key(&keys, 65U);
        set_key(&keys, 40U);

        v3f_default_profile_build_nkro16(&keys, legacy);
        v3f_profile_runtime_build_nkro16(&keys, runtime);
        CHECK(memcmp(legacy, runtime, sizeof(legacy)) == 0);
        CHECK(runtime[0] == 0x03U); /* left ctrl | left shift */
    }
}

static void test_local_controls(void)
{
    aik_spi_half_state_v1_t left;
    aik_spi_half_state_v1_t right;
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES];

    memset(&left, 0, sizeof(left));
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_UP);
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_CENTER);

    memset(nkro16, 0, sizeof(nkro16));
    v3f_profile_runtime_apply_local_keyboard(&left, nkro16);
    CHECK(nkro_usage_set(nkro16, 0x52U) == 1U); /* up arrow */
    CHECK(nkro_usage_set(nkro16, 0x28U) == 1U); /* enter */
    CHECK(nkro_usage_set(nkro16, 0x51U) == 0U); /* down arrow not set */

    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, AIK_RIGHT_LOCAL_BIT_EC11_CW);
    CHECK(v3f_profile_runtime_consumer_usage(&right) ==
          AIK_CONSUMER_USAGE_VOLUME_UP);

    memset(&right, 0, sizeof(right));
    aik_spi_half_set_bit(&right, AIK_RIGHT_LOCAL_BIT_EC11_MUTE);
    CHECK(v3f_profile_runtime_consumer_usage(&right) ==
          AIK_CONSUMER_USAGE_MUTE);

    memset(&left, 0, sizeof(left));
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_UP);
    CHECK(v3f_profile_runtime_mouse_wheel(&left) == 1);

    memset(&left, 0, sizeof(left));
    aik_spi_half_set_bit(&left, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_DOWN);
    CHECK(v3f_profile_runtime_mouse_wheel(&left) == -1);
}

static void test_triggers(void)
{
    const v3f_profile_runtime_t *rt = v3f_profile_runtime_get();
    uint8_t i;

    for(i = 0U; i < AIK_KEY_COUNT_TOTAL; i++)
    {
        CHECK(rt->triggers[i].press_pm == 400U);
        CHECK(rt->triggers[i].release_pm == 350U);
        CHECK(rt->triggers[i].rt_press_delta_pm == 100U);
        CHECK(rt->triggers[i].rt_release_delta_pm == 100U);
        CHECK(rt->triggers[i].mode == AIK_RT_MODE_RAPID_TRIGGER);
        if(s_failures != 0)
        {
            break;
        }
    }
}

static void test_half_patches(void)
{
    uint8_t patch[AIK_HP_MAX_SIZE];
    uint16_t len;
    const aik_hp_header_t *hdr;
    const v3f_profile_runtime_t *rt = v3f_profile_runtime_get();

    len = v3f_profile_runtime_build_half_patch(AIK_HALF_ID_LEFT, patch,
                                               sizeof(patch));
    CHECK(len != 0U);
    CHECK(aik_hp_valid(patch, len) == 1U);
    hdr = (const aik_hp_header_t *)patch;
    CHECK(hdr->half_id == AIK_HALF_ID_LEFT);
    CHECK(hdr->key_count == AIK_KEY_COUNT_LEFT);
    CHECK((hdr->flags & AIK_HP_FLAG_HAS_DISPATCH77) != 0U);
    CHECK(hdr->profile_id16 == rt->profile_id16);
    CHECK(hdr->local_count == 10U); /* composer gets every local binding */
    CHECK(memcmp(patch + hdr->dispatch_offset, rt->base_keys,
                 AIK_KEY_COUNT_TOTAL * sizeof(aik_hp_key_output_t)) == 0);

    /* Left half triggers map to global keys 41..76. */
    {
        const aik_hp_trigger_entry_t *trig =
            (const aik_hp_trigger_entry_t *)(patch + hdr->trigger_offset);

        CHECK(trig[0].press_pm == rt->triggers[AIK_KEY_COUNT_RIGHT].press_pm);
    }

    len = v3f_profile_runtime_build_half_patch(AIK_HALF_ID_RIGHT, patch,
                                               sizeof(patch));
    CHECK(len != 0U);
    CHECK(aik_hp_valid(patch, len) == 1U);
    hdr = (const aik_hp_header_t *)patch;
    CHECK(hdr->half_id == AIK_HALF_ID_RIGHT);
    CHECK(hdr->key_count == AIK_KEY_COUNT_RIGHT);
    CHECK((hdr->flags & AIK_HP_FLAG_HAS_DISPATCH77) == 0U);
    CHECK(hdr->local_count == 0U); /* right half publishes raw bits only */
}

static void test_release_to_rearm(void)
{
    v3f_global_key_state_t keys;
    uint8_t nkro[AIK_NKRO_REPORT_BYTES];
    uint8_t zero[AIK_NKRO_REPORT_BYTES];

    memset(zero, 0, sizeof(zero));

    /* A fresh install raises the rearm event exactly once. */
    CHECK(v3f_profile_runtime_install_package(g_v3f_factory_profile_image,
                                              g_v3f_factory_profile_image_size,
                                              AIK_PROFILE_SLOT_FACTORY) ==
          V3F_PROFILE_OK);
    CHECK(v3f_profile_runtime_rearm_take() == 1U);
    CHECK(v3f_profile_runtime_rearm_take() == 0U);

    /* Key 5 held across the swap: suppressed until released. */
    memset(&keys, 0, sizeof(keys));
    set_key(&keys, 5U);
    v3f_profile_runtime_rearm_latch(&keys);

    v3f_profile_runtime_build_nkro16(&keys, nkro);
    CHECK(memcmp(nkro, zero, sizeof(nkro)) == 0);

    /* Release clears the gate; the next press reports normally. */
    memset(&keys, 0, sizeof(keys));
    v3f_profile_runtime_build_nkro16(&keys, nkro);
    memset(&keys, 0, sizeof(keys));
    set_key(&keys, 5U);
    v3f_profile_runtime_build_nkro16(&keys, nkro);
    CHECK(memcmp(nkro, zero, sizeof(nkro)) != 0);
}

static void test_corrupt_package_rejected(void)
{
    static uint8_t bad[64 * 1024];
    uint32_t size = g_v3f_factory_profile_image_size;

    memcpy(bad, g_v3f_factory_profile_image, size);
    bad[size / 2U] ^= 0x5AU;
    CHECK(v3f_profile_package_validate(bad, size, 0) != V3F_PROFILE_OK);
}

int main(void)
{
    uint8_t active;

    CHECK(v3f_profile_package_validate(g_v3f_factory_profile_image,
                                       g_v3f_factory_profile_image_size,
                                       0) == V3F_PROFILE_OK);

    active = v3f_profile_runtime_init();
    CHECK(active == AIK_PROFILE_SLOT_FACTORY);
    CHECK(v3f_profile_runtime_valid() == 1U);
    CHECK(v3f_profile_runtime_get()->has_fn_overlay == 0U);
    CHECK(v3f_profile_runtime_get()->local_count == 10U);

    test_key_table_equivalence();
    test_local_controls();
    test_triggers();
    test_half_patches();
    test_release_to_rearm();
    test_corrupt_package_rejected();

    if(s_failures == 0)
    {
        printf("host_profile_runtime_test: all checks passed\n");
        return 0;
    }
    printf("host_profile_runtime_test: %d failures\n", s_failures);
    return 1;
}
