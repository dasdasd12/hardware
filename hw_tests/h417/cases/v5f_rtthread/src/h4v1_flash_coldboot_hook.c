/*
 * Read-only H4V1 cold-boot adapters for the qualified 165-frame image.
 *
 * Five fail-closed post-link substitutions enter this isolated tail:
 *   - the qualified line reader receives the fixed H4V1 contract;
 *   - the qualified raw reader receives bytes from the committed NAND map;
 *   - the post-RESULT completion preserves watchdog-complete, then confirms
 *     PLAY PASS;
 *   - the qualified H4C LOOP logger is preserved, then replays cold-boot
 *     status so a monitor attached after power-up can still qualify the boot.
 *   - the upload-only credit logger is suppressed because cold boot has no
 *     USB producer to consume those acknowledgements.
 *
 * No Flash write, erase, unlock or feature-set operation is exposed here.
 */
#include <stdint.h>

#include "h4v1_flash_coldboot_core.h"

#define H4CB_HOOK_TEXT \
    __attribute__((section(".h4v1_postpass.text"), noinline, used))
#define H4CB_HOOK_RODATA \
    __attribute__((section(".h4v1_postpass.rodata"), aligned(4), used))
#define H4CB_HOOK_DATA \
    __attribute__((section(".h4v1_postpass.data"), aligned(4), used))
#define H4CB_HOOK_IDENTITY \
    __attribute__((section(".h4v1_postpass.rodata.identity"), aligned(4), used))

#define H4CB_ORIGINAL_CALL_SENTINEL 0x48345031u
#define H4CB_ORIGINAL_LOOP_SENTINEL 0x4834434Cu
#define H4CB_BOOT_COOKIE_FRESH      0x43424631u
#define H4CB_BOOT_COOKIE_ACTIVE     0x43424131u

#ifndef H4V1_FLASH_COLD_BOOT_FRAMES
#define H4V1_FLASH_COLD_BOOT_FRAMES 0u
#endif

#if H4V1_FLASH_COLD_BOOT_FRAMES != 165u
#error "The read-only H4V1 cold-boot target is qualified only for 165 frames"
#endif

static const char h4v1_flash_coldboot_identity[] H4CB_HOOK_IDENTITY =
    "FLASH COLD BOOT HOOK ENTER readonly=1 frames=165 "
    "reference=chunk165_pass manifest=768 payload=769..1014 scratch=1015";

/* Both words are replaced only after the post-link tool proves the exact
 * qualified old targets. */
volatile const uint32_t h4v1_flash_postpass_original_call H4CB_HOOK_RODATA =
    H4CB_ORIGINAL_CALL_SENTINEL;
volatile const uint32_t h4v1_flash_coldboot_original_loop_log H4CB_HOOK_RODATA =
    H4CB_ORIGINAL_LOOP_SENTINEL;

/* Unlike the large NOLOAD context this initialized word is copied from Flash
 * on every reset.  It makes a warm reset distinguishable from stale NOLOAD
 * state without moving or growing the qualified .bss/heap. */
static volatile uint32_t h4v1_flash_coldboot_boot_cookie H4CB_HOOK_DATA =
    H4CB_BOOT_COOKIE_FRESH;

void H4CB_HOOK_TEXT h4v1_flash_coldboot_log_line(const char *line)
{
    int (*qualified_logger)(const char *) =
        (int (*)(const char *))(uintptr_t)
            h4v1_flash_coldboot_original_loop_log;

    (void)qualified_logger(line);
}

int H4CB_HOOK_TEXT
h4v1_flash_coldboot_line_entry(char *dst, uint32_t cap)
{
    if(h4v1_flash_coldboot_boot_cookie != H4CB_BOOT_COOKIE_ACTIVE)
    {
        h4v1_flash_coldboot_reset();
        h4v1_flash_coldboot_boot_cookie = H4CB_BOOT_COOKIE_ACTIVE;
    }
    return h4v1_flash_coldboot_line_provider(dst, cap);
}

void H4CB_HOOK_TEXT
h4v1_flash_coldboot_loop_log_entry(const char *line)
{
    int (*qualified_logger)(const char *) =
        (int (*)(const char *))(uintptr_t)
            h4v1_flash_coldboot_original_loop_log;

    (void)qualified_logger(line);
    h4v1_flash_coldboot_replay_status();
}

void H4CB_HOOK_TEXT h4v1_flash_postpass_trampoline(void)
{
    void (*original_completion)(void) =
        (void (*)(void))(uintptr_t)h4v1_flash_postpass_original_call;

    original_completion();
    h4v1_flash_coldboot_play_pass();
}
