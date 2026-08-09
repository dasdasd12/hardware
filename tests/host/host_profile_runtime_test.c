/*
 * Host-side equivalence test for the V3F profile runtime.
 *
 * Feeds the generated factory AKPK image to the firmware parser and
 * asserts the installed runtime reproduces the legacy hardcoded table
 * (default_profile.c) byte for byte, plus local-control and AKHR
 * derivation checks.
 *
 * Build and run from hardware:
 *   make -C tests/host run
 */

#include <stdio.h>
#include <string.h>

#include "profile_runtime.h"
#include "profile_store.h"
#include "default_profile.h"
#include "factory_profile_image.h"

static int s_failures;
static uint8_t s_user_slot[V3F_PROFILE_SLOT_SIZE];
static uint8_t s_active_slot;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

/* profile_store stubs: one backing buffer is enough to exercise erased
 * and corrupt user-slot boot semantics. */
const uint8_t *v3f_profile_store_slot_ptr(uint8_t slot_id)
{
    if((slot_id >= AIK_PROFILE_USER_SLOT_FIRST) &&
       (slot_id < AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return s_user_slot;
    }
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
uint8_t v3f_profile_store_get_active_slot(void) { return s_active_slot; }
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
    CHECK((hdr->flags & AIK_HP_FLAG_HAS_FN_DISPATCH77) == 0U);
    CHECK(hdr->fn_dispatch_offset == 0U);
    CHECK(hdr->fn_hold_key == 0xFFU);
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
    CHECK((hdr->flags & AIK_HP_FLAG_HAS_FN_DISPATCH77) == 0U);
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

static uint8_t profile2_key_kept(uint8_t key_id)
{
    if((key_id <= 13U) ||
       ((key_id >= 41U) && (key_id <= 53U)))
    {
        return 1U;
    }
    if(((key_id >= 16U) && (key_id <= 19U)) ||
       (key_id == 22U) || (key_id == 23U) ||
       ((key_id >= 56U) && (key_id <= 60U)) ||
       (key_id == 66U) || (key_id == 73U) || (key_id == 40U))
    {
        return 1U;
    }
    return 0U;
}

static void test_erased_user_slot_defaults(void)
{
    v3f_profile_runtime_t factory;
    v3f_profile_runtime_t profile1;
    v3f_profile_runtime_t profile2;
    v3f_profile_runtime_t profile3;
    v3f_profile_runtime_t stored;
    uint8_t active;
    uint8_t key_id;
    uint8_t mapped_count = 0U;

    memset(s_user_slot, 0xFF, sizeof(s_user_slot));
    CHECK(v3f_profile_runtime_prepare_slot(AIK_PROFILE_SLOT_FACTORY,
                                           &factory) ==
          V3F_PROFILE_OK);
    CHECK(factory.usb_report_rate_hz == 8000U);
    CHECK(factory.wireless_report_rate_hz == 1000U);

    CHECK(v3f_profile_runtime_prepare_slot(1U, &profile1) == V3F_PROFILE_OK);
    CHECK(profile1.active_slot == 1U);
    CHECK(profile1.profile_id16 == 0xDC75U);
    CHECK(profile1.profile_id16 ==
          aik_profile_string_hash16("builtin_profile_1"));
    CHECK(profile1.generation16 == 1U);
    CHECK(profile1.revision == 1U);
    CHECK(profile1.usb_report_rate_hz == 4000U);
    CHECK(profile1.wireless_report_rate_hz == 1000U);
    CHECK(memcmp(profile1.base_keys, factory.base_keys,
                 sizeof(factory.base_keys)) == 0);
    CHECK(memcmp(profile1.triggers, factory.triggers,
                 sizeof(factory.triggers)) == 0);
    CHECK(memcmp(profile1.locals, factory.locals,
                 sizeof(factory.locals)) == 0);

    CHECK(v3f_profile_runtime_prepare_slot(2U, &profile2) == V3F_PROFILE_OK);
    CHECK(profile2.active_slot == 2U);
    CHECK(profile2.profile_id16 == 0x2F81U);
    CHECK(profile2.profile_id16 ==
          aik_profile_string_hash16("builtin_profile_2"));
    CHECK(profile2.generation16 == 1U);
    CHECK(profile2.revision == 1U);
    CHECK(profile2.usb_report_rate_hz == 8000U);
    CHECK(profile2.wireless_report_rate_hz == 1000U);
    CHECK(profile2.has_fn_overlay == 0U);
    CHECK(profile2.fn_hold_key == 0xFFU);
    CHECK(profile2.local_count == factory.local_count);
    CHECK(memcmp(profile2.locals, factory.locals,
                 sizeof(factory.locals)) == 0);
    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        const aik_hp_key_output_t *output = &profile2.base_keys[key_id];

        CHECK(profile2.fn_keys[key_id].usage == 0U);
        CHECK(profile2.fn_keys[key_id].modifier_mask == 0U);
        if((output->usage != 0U) || (output->modifier_mask != 0U))
        {
            mapped_count++;
        }

        if(key_id == 73U)
        {
            CHECK(output->usage == 0U);
            CHECK(output->modifier_mask == 0x02U);
        }
        else if(key_id == 40U)
        {
            CHECK(output->usage == 0U);
            CHECK(output->modifier_mask == 0x20U);
        }
        else if(profile2_key_kept(key_id) != 0U)
        {
            CHECK(memcmp(output, &factory.base_keys[key_id],
                         sizeof(*output)) == 0);
        }
        else
        {
            CHECK(output->usage == 0U);
            CHECK(output->modifier_mask == 0U);
        }

        CHECK(profile2.triggers[key_id].mode ==
              AIK_RT_MODE_RAPID_TRIGGER);
        CHECK(profile2.triggers[key_id].press_pm == 200U);
        CHECK(profile2.triggers[key_id].rt_press_delta_pm == 200U);
        CHECK(profile2.triggers[key_id].rt_release_delta_pm == 200U);
    }
    CHECK(mapped_count == 41U);

    CHECK(v3f_profile_runtime_prepare_slot(3U, &profile3) == V3F_PROFILE_OK);
    CHECK(profile3.active_slot == 3U);
    CHECK(profile3.profile_id16 == 0xAC82U);
    CHECK(profile3.profile_id16 ==
          aik_profile_string_hash16("builtin_profile_3"));
    CHECK(profile3.generation16 == 1U);
    CHECK(profile3.revision == 1U);
    CHECK(profile3.usb_report_rate_hz == 8000U);
    CHECK(profile3.wireless_report_rate_hz == 1000U);
    CHECK(profile3.has_fn_overlay == 0U);
    CHECK(profile3.fn_hold_key == 0xFFU);
    CHECK(profile3.local_count == 0U);
    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        CHECK(profile3.base_keys[key_id].usage == 0U);
        CHECK(profile3.base_keys[key_id].modifier_mask == 0U);
        CHECK(profile3.fn_keys[key_id].usage == 0U);
        CHECK(profile3.fn_keys[key_id].modifier_mask == 0U);
        CHECK(memcmp(&profile3.triggers[key_id], &factory.triggers[key_id],
                     sizeof(profile3.triggers[key_id])) == 0);
    }

    s_active_slot = 2U;
    active = v3f_profile_runtime_init();
    CHECK(active == 2U);
    CHECK(v3f_profile_runtime_get()->active_slot == 2U);
    CHECK(v3f_profile_runtime_get()->profile_id16 == 0x2F81U);

    /* A real stored AKPK always wins over the built-in slot preset. */
    memset(s_user_slot, 0xFF, sizeof(s_user_slot));
    memcpy(s_user_slot, g_v3f_factory_profile_image,
           g_v3f_factory_profile_image_size);
    CHECK(v3f_profile_runtime_prepare_slot(2U, &stored) == V3F_PROFILE_OK);
    CHECK(stored.active_slot == 2U);
    CHECK(stored.profile_id16 == factory.profile_id16);
    CHECK(stored.usb_report_rate_hz == factory.usb_report_rate_hz);
    CHECK(memcmp(stored.base_keys, factory.base_keys,
                 sizeof(factory.base_keys)) == 0);
    CHECK(memcmp(stored.triggers, factory.triggers,
                 sizeof(factory.triggers)) == 0);
    CHECK(memcmp(stored.locals, factory.locals,
                 sizeof(factory.locals)) == 0);

    /* Non-erased invalid content is corruption, not an empty default. */
    memset(s_user_slot, 0xFF, sizeof(s_user_slot));
    s_user_slot[0] = 0U;
    CHECK(v3f_profile_runtime_prepare_slot(1U, &stored) != V3F_PROFILE_OK);

    s_active_slot = 1U;
    active = v3f_profile_runtime_init();
    CHECK(active == AIK_PROFILE_SLOT_FACTORY);
    CHECK(v3f_profile_runtime_get()->active_slot ==
          AIK_PROFILE_SLOT_FACTORY);

    s_active_slot = AIK_PROFILE_SLOT_FACTORY;
    memset(s_user_slot, 0xFF, sizeof(s_user_slot));
}

int main(void)
{
    uint8_t active;

    memset(s_user_slot, 0xFF, sizeof(s_user_slot));

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
    test_erased_user_slot_defaults();

    if(s_failures == 0)
    {
        printf("host_profile_runtime_test: all checks passed\n");
        return 0;
    }
    printf("host_profile_runtime_test: %d failures\n", s_failures);
    return 1;
}
