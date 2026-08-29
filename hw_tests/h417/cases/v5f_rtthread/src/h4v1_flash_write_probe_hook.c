/*
 * Destructive write-probe hook for the qualified H4V1 test.
 *
 * This is intentionally a different object from the read-only install hook.
 * Only h417_v5f_flash_h4v1_write_probe links it.  The post-link tool patches
 * the same qualified completion call and this trampoline preserves that
 * original action before running the fail-closed Stage 1..5 chain.
 */
#include <stdint.h>

#include "h4v1_flash_postpass_stage1.h"
#include "h4v1_flash_postpass_stage2.h"
#include "h4v1_flash_postpass_stage3.h"
#include "h4v1_flash_postpass_stage4.h"
#include "h4v1_flash_postpass_stage5.h"

#define H4V1_POSTPASS_TEXT \
    __attribute__((section(".h4v1_postpass.text"), noinline, used))
#define H4V1_POSTPASS_RODATA \
    __attribute__((section(".h4v1_postpass.rodata"), aligned(4), used))
#define H4V1_POSTPASS_IDENTITY_RODATA \
    __attribute__((section(".h4v1_postpass.rodata.identity"), aligned(4), used))
#define H4V1_POSTPASS_ORIGINAL_SENTINEL 0x48345031u

#ifndef H4V1_FLASH_WRITE_PROBE_FRAMES
#define H4V1_FLASH_WRITE_PROBE_FRAMES 90u
#endif

#if H4V1_FLASH_WRITE_PROBE_FRAMES == 165u
static const char h4v1_flash_write_probe_identity[] \
    H4V1_POSTPASS_IDENTITY_RODATA =
    "FLASH WRITE_PROBE HOOK ENTER stage5=armed frames=165 "
    "reference=chunk165_pass block=1015 row=0000fdc0";
#elif H4V1_FLASH_WRITE_PROBE_FRAMES == 90u
static const char h4v1_flash_write_probe_identity[] \
    H4V1_POSTPASS_IDENTITY_RODATA =
    "FLASH WRITE_PROBE HOOK ENTER stage5=armed frames=90 "
    "reference=chunk90_pass block=1015 row=0000fdc0";
#else
#error "Unsupported H4V1 Flash write-probe frame contract"
#endif

extern void h4v1_postpass_stage1_log(const char *text);

volatile const uint32_t h4v1_flash_postpass_original_call
    H4V1_POSTPASS_RODATA = H4V1_POSTPASS_ORIGINAL_SENTINEL;

void H4V1_POSTPASS_TEXT h4v1_flash_postpass_trampoline(void)
{
    void (*original_completion)(void) =
        (void (*)(void))(uintptr_t)h4v1_flash_postpass_original_call;

    original_completion();
    h4v1_postpass_stage1_log(h4v1_flash_write_probe_identity);
    if(h4v1_flash_postpass_stage1_run() == H4V1_FLASH_POSTPASS_STAGE1_OK)
    {
        if(h4v1_flash_postpass_stage2_run() ==
           H4V1_FLASH_POSTPASS_STAGE2_OK)
        {
            if(h4v1_flash_postpass_stage3_run() ==
               H4V1_FLASH_POSTPASS_STAGE3_OK)
            {
                if(h4v1_flash_postpass_stage4_run() ==
                   H4V1_FLASH_POSTPASS_STAGE4_OK)
                {
                    (void)h4v1_flash_postpass_stage5_run();
                }
            }
        }
    }
}
