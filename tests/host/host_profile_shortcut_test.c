/*
 * Build and run from hardware:
 *   make -C tests/host run
 */

#include <stdio.h>

#include "aik_profile_shortcut.h"

static int s_failures;

#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

static void test_no_fn_does_not_consume(void)
{
    aik_profile_shortcut_state_t state;

    aik_profile_shortcut_reset(&state);
    CHECK(aik_profile_shortcut_update(&state, 0U, 0x01U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_update(&state, 0U, 0x0FU) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0xF0U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 0U);
}

static void test_each_slot_emits_once(void)
{
    aik_profile_shortcut_state_t state;
    uint8_t slot;

    aik_profile_shortcut_reset(&state);
    for(slot = 0U; slot < 4U; slot++)
    {
        uint8_t mask = (uint8_t)(1U << slot);

        CHECK(aik_profile_shortcut_update(&state, 1U, mask) == slot);
        CHECK(aik_profile_shortcut_consuming(&state) == 1U);
        CHECK(aik_profile_shortcut_update(&state, 1U, mask) ==
              AIK_PROFILE_SHORTCUT_NONE);
        CHECK(aik_profile_shortcut_update(&state, 1U, mask) ==
              AIK_PROFILE_SHORTCUT_NONE);
        CHECK(aik_profile_shortcut_update(&state, 0U, 0U) ==
              AIK_PROFILE_SHORTCUT_NONE);
        CHECK(aik_profile_shortcut_consuming(&state) == 0U);
    }
}

static void test_fn_release_first_keeps_consuming(void)
{
    aik_profile_shortcut_state_t state;

    aik_profile_shortcut_reset(&state);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x04U) == 2U);
    CHECK(aik_profile_shortcut_update(&state, 0U, 0x04U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);

    /* A different slot cannot trigger while the original chord drains. */
    CHECK(aik_profile_shortcut_update(&state, 0U, 0x08U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x08U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);

    CHECK(aik_profile_shortcut_update(&state, 0U, 0U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 0U);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x08U) == 3U);
}

static void test_slot_release_first_keeps_consuming(void)
{
    aik_profile_shortcut_state_t state;

    aik_profile_shortcut_reset(&state);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x02U) == 1U);
    CHECK(aik_profile_shortcut_consumed_slot_mask(&state, 0xF2U) == 0x02U);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x01U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_update(&state, 0U, 0U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 0U);
    CHECK(aik_profile_shortcut_consumed_slot_mask(&state, 0x0FU) == 0U);
}

static void test_multi_slot_chord_never_switches(void)
{
    aik_profile_shortcut_state_t state;

    aik_profile_shortcut_reset(&state);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x03U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);

    /* Resolving an ambiguous chord to one key must not switch mid-hold. */
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x01U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_update(&state, 0U, 0x01U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);
    CHECK(aik_profile_shortcut_update(&state, 0U, 0U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 0U);
}

static void test_slot_then_fn_can_start_chord(void)
{
    aik_profile_shortcut_state_t state;

    aik_profile_shortcut_reset(&state);
    CHECK(aik_profile_shortcut_update(&state, 0U, 0x01U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 0U);
    CHECK(aik_profile_shortcut_update(&state, 1U, 0x01U) == 0U);
    CHECK(AIK_PROFILE_SHORTCUT_NONE != 0U);
}

static void test_invalid_input_preserves_state(void)
{
    aik_profile_shortcut_state_t state;

    aik_profile_shortcut_reset(&state);

    /* Invalid input cannot start a chord. */
    CHECK(aik_profile_shortcut_update_valid(
              &state, 1U, 0x01U, 0U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 0U);

    CHECK(aik_profile_shortcut_update_valid(
              &state, 1U, 0x01U, 1U) == 0U);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);

    /*
     * A short invalid interval must not re-arm the held chord, even when
     * the unavailable sample looks fully released.
     */
    CHECK(aik_profile_shortcut_update_valid(
              &state, 0U, 0U, 0U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);
    CHECK(aik_profile_shortcut_update_valid(
              &state, 1U, 0x01U, 1U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 1U);

    CHECK(aik_profile_shortcut_update_valid(
              &state, 0U, 0U, 1U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(&state) == 0U);
}

static void test_null_state_is_safe(void)
{
    aik_profile_shortcut_reset(0);
    CHECK(aik_profile_shortcut_update(0, 1U, 0x01U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_update_valid(0, 1U, 0x01U, 1U) ==
          AIK_PROFILE_SHORTCUT_NONE);
    CHECK(aik_profile_shortcut_consuming(0) == 0U);
    CHECK(aik_profile_shortcut_consumed_slot_mask(0, 0x0FU) == 0U);
}

int main(void)
{
    test_no_fn_does_not_consume();
    test_each_slot_emits_once();
    test_fn_release_first_keeps_consuming();
    test_slot_release_first_keeps_consuming();
    test_multi_slot_chord_never_switches();
    test_slot_then_fn_can_start_chord();
    test_invalid_input_preserves_state();
    test_null_state_is_safe();

    if(s_failures != 0)
    {
        printf("host profile shortcut tests: %d failure(s)\n", s_failures);
        return 1;
    }

    printf("host profile shortcut tests: PASS\n");
    return 0;
}
