/*
 * Transactional H4V1 Flash installer hook for the qualified 165-frame test.
 *
 * The normal USB -> SDRAM -> verify -> first playback path remains the exact
 * hardware-qualified image.  The post-link tool redirects one call after its
 * RESULT PASS to this isolated tail.  Always preserve the original completion
 * call first; only then identify the installer image and enter the read-only
 * Stage1 guard followed by the destructive Stage6 transaction.
 */
#include <stdint.h>

#include "h4v1_flash_postpass_stage1.h"
#include "h4v1_flash_installer_stage6.h"

#define H4V1_POSTPASS_TEXT \
    __attribute__((section(".h4v1_postpass.text"), noinline, used))
#define H4V1_POSTPASS_RODATA \
    __attribute__((section(".h4v1_postpass.rodata"), aligned(4), used))
#define H4V1_POSTPASS_IDENTITY_RODATA \
    __attribute__((section(".h4v1_postpass.rodata.identity"), aligned(4), used))
#define H4V1_POSTPASS_ORIGINAL_SENTINEL 0x48345031u

#ifndef H4V1_FLASH_INSTALL_FRAMES
#define H4V1_FLASH_INSTALL_FRAMES 0u
#endif

#if H4V1_FLASH_INSTALL_FRAMES != 165u
#error "The transactional H4V1 Flash installer is qualified only for 165 frames"
#endif

static const char h4v1_flash_install_identity[] \
    H4V1_POSTPASS_IDENTITY_RODATA =
    "FLASH INSTALL HOOK ENTER stage6=armed frames=165 "
    "reference=chunk165_pass blocks=768..1014 manifest=768 scratch=1015";

extern void h4v1_postpass_stage1_log(const char *text);

volatile const uint32_t h4v1_flash_postpass_original_call
    H4V1_POSTPASS_RODATA = H4V1_POSTPASS_ORIGINAL_SENTINEL;

void H4V1_POSTPASS_TEXT h4v1_flash_postpass_trampoline(void)
{
    void (*original_completion)(void) =
        (void (*)(void))(uintptr_t)h4v1_flash_postpass_original_call;

    original_completion();
    h4v1_postpass_stage1_log(h4v1_flash_install_identity);
    if(h4v1_flash_postpass_stage1_run() == H4V1_FLASH_POSTPASS_STAGE1_OK)
    {
        (void)h4v1_flash_installer_stage6_run();
    }
}
