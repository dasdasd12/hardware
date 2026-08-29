/*
 * Install-only post-link hook for the qualified H4V1 test.
 *
 * Nothing references this object from the normal source graph.  The build
 * tool redirects the single watchdog-complete JAL immediately following the
 * qualified "RESULT PASS" line to this trampoline.  It also replaces the
 * sentinel below with that JAL's original target, preserving the original
 * completion action before the read-only Flash probe runs.
 */
#include <stdint.h>

#include "h4v1_flash_postpass_stage1.h"
#include "h4v1_flash_postpass_stage2.h"
#include "h4v1_flash_postpass_stage3.h"
#include "h4v1_flash_postpass_stage4.h"

#define H4V1_POSTPASS_TEXT \
    __attribute__((section(".h4v1_postpass.text"), noinline, used))
#define H4V1_POSTPASS_RODATA \
    __attribute__((section(".h4v1_postpass.rodata"), aligned(4), used))

#define H4V1_POSTPASS_ORIGINAL_SENTINEL 0x48345031u

/* Volatile prevents the compiler from folding the sentinel into the call. */
volatile const uint32_t h4v1_flash_postpass_original_call
    H4V1_POSTPASS_RODATA = H4V1_POSTPASS_ORIGINAL_SENTINEL;

void H4V1_POSTPASS_TEXT h4v1_flash_postpass_trampoline(void)
{
    void (*original_completion)(void) =
        (void (*)(void))(uintptr_t)h4v1_flash_postpass_original_call;

    original_completion();
    if(h4v1_flash_postpass_stage1_run() == H4V1_FLASH_POSTPASS_STAGE1_OK)
    {
        if(h4v1_flash_postpass_stage2_run() ==
           H4V1_FLASH_POSTPASS_STAGE2_OK)
        {
            if(h4v1_flash_postpass_stage3_run() ==
               H4V1_FLASH_POSTPASS_STAGE3_OK)
            {
                (void)h4v1_flash_postpass_stage4_run();
            }
        }
    }
}
