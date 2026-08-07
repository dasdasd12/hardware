#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32h417.h"
#include "ch32h417_dbgmcu.h"
#include "ch32h417_flash.h"
#include "ch32h417_gpio.h"
#include "ch32h417_iwdg.h"
#include "ch32h417_pwr.h"
#include "ch32h417_rcc.h"
#include "ch32h417_usbfs_device.h"
#include "ch32h417_usbfs_hid_nkro.h"

/*
 * Destructive test range: the final 8 KiB erase page in the H417 960 KiB
 * CodeFlash. Keep this outside all firmware images and persistent data.
 */
#define FLASH_TEST_ADDR             0x080EE000UL
#define FLASH_TEST_ALIAS_ADDR       0x000EE000UL
#define FLASH_TEST_ERASE_SIZE       0x00002000UL
#define FLASH_TEST_END_ADDR         (FLASH_TEST_ADDR + \
                                     FLASH_TEST_ERASE_SIZE - 1UL)
#define FLASH_TEST_PAGE_SIZE        256UL
#define FLASH_TEST_WORD_SIZE        4UL
#define FLASH_TEST_WORDS            (FLASH_TEST_ERASE_SIZE / \
                                     FLASH_TEST_WORD_SIZE)
#define FLASH_TEST_PAGE_WORDS       (FLASH_TEST_PAGE_SIZE / \
                                     FLASH_TEST_WORD_SIZE)
#define FLASH_TEST_PAGES            (FLASH_TEST_ERASE_SIZE / \
                                     FLASH_TEST_PAGE_SIZE)
#define FLASH_TEST_GUARD_ADDR       (FLASH_TEST_ADDR - FLASH_TEST_PAGE_SIZE)
#define FLASH_TEST_READ_REPEATS     16U
#define FLASH_FINAL_MAGIC           0x32445446UL /* "FTD2" */
/*
 * WCH CH32 CodeFlash does not read back as 0xFFFFFFFF after erase.
 * Its erased 32-bit read value is 0xE339E339 (16-bit: 0xE339).
 */
#define FLASH_ERASED_WORD           0xE339E339UL

#define FLASH_CFGR0_DUAL_BANK       (1UL << 28)
#define FLASH_TEST_WPR_MASK          (1UL << 31)
#define FLASH_STATR_WRPRTERR_MASK   0x00000010UL
#define FLASH_STATR_EOP_MASK        0x00000020UL

#define FLASH_RETAIN_ADDR           0x20178300UL
#define FLASH_RETAIN_MAGIC          0x464C5432UL
#define FLASH_RETAIN_MAGIC_INV      0xB9B3ABCDUL

#define IWDG_KEY_ENABLE             0xCCCCU
#define IWDG_READY_MASK             0x00000002UL
#define IWDG_READY_POLLS            2000000UL
#define IWDG_RELOAD_VALUE           4000U

#define STATUS_NOT_RUN              0U
#define CDC_REPORT_LINE_COUNT       27U

#if ((FLASH_TEST_ADDR & (FLASH_TEST_ERASE_SIZE - 1UL)) != 0UL)
#error "FLASH_TEST_ADDR must be aligned to the 8 KiB erase size"
#endif

#if ((FLASH_TEST_ADDR < 0x080B2000UL) || \
     ((FLASH_TEST_ADDR + FLASH_TEST_ERASE_SIZE) > 0x080F0000UL))
#error "Flash test page must stay in the reserved post-profile range"
#endif

typedef enum
{
    FLASH_STAGE_EMPTY = 0U,
    FLASH_STAGE_BOOT = 1U,
    FLASH_STAGE_INITIAL_READ = 5U,
    FLASH_STAGE_FAST_ERASE_ARMED = 10U,
    FLASH_STAGE_FAST_ERASE_RETURNED = 11U,
    FLASH_STAGE_FAST_ERASE_VERIFY = 12U,
    FLASH_STAGE_STD_ERASE1_ARMED = 20U,
    FLASH_STAGE_STD_ERASE1_RETURNED = 21U,
    FLASH_STAGE_FAST_PROGRAM_ARMED = 30U,
    FLASH_STAGE_FAST_PROGRAM_RETURNED = 31U,
    FLASH_STAGE_FAST_PROGRAM_VERIFY = 40U,
    FLASH_STAGE_READ_STABILITY = 50U,
    FLASH_STAGE_STD_ERASE2_ARMED = 60U,
    FLASH_STAGE_STD_ERASE2_RETURNED = 61U,
    FLASH_STAGE_WORD_PROGRAM_ARMED = 70U,
    FLASH_STAGE_WORD_PROGRAM_RETURNED = 71U,
    FLASH_STAGE_DELAYED_VERIFY = 80U,
    FLASH_STAGE_DONE = 90U,
    FLASH_STAGE_RECOVERED = 91U
} flash_test_stage_t;

typedef enum
{
    FLASH_RESULT_PASS = 1U,
    FLASH_RESULT_DEGRADED = 2U,
    FLASH_RESULT_FAIL = 3U
} flash_result_code_t;

typedef struct
{
    uint32_t expected_crc;
    uint32_t read_crc;
    uint32_t erased_words;
    uint32_t zero_words;
    uint32_t other_words;
    uint32_t mismatch_words;
    uint32_t first_bad_offset;
    uint32_t first_expected;
    uint32_t first_actual;
} memory_check_t;

typedef struct
{
    uint32_t magic;
    uint32_t magic_inv;
    uint32_t stage;
    uint32_t detail;
    uint32_t reset_flags;
    uint32_t statr;
    uint32_t ctlr;
    uint32_t addr;
} flash_retain_record_t;

typedef struct
{
    uint32_t chip_id;
    uint32_t reset_flags;
    uint32_t cfgr0;
    uint32_t flash_capacity_kib;
    uint32_t actlr_initial;
    uint32_t statr_initial;
    uint32_t ctlr_initial;
    uint32_t addr_initial;
    uint32_t obr;
    uint32_t wpr;
    uint16_t ob_wrpr0;
    uint16_t ob_wrpr1;
    uint16_t ob_wrpr2;
    uint16_t ob_wrpr3;

    uint32_t recovery_stage;
    uint32_t recovery_detail;
    uint32_t recovery_statr;
    uint32_t recovery_ctlr;
    uint8_t recovered;
    uint8_t iwdg_reset;
    uint8_t watchdog_ready;
    uint8_t pe12_low;
    uint8_t test_write_protected;

    memory_check_t initial;
    memory_check_t retention;
    memory_check_t fast_erase;
    memory_check_t std_erase1;
    memory_check_t fast_program;
    memory_check_t std_erase2;
    memory_check_t word_program;
    memory_check_t delayed_program;

    uint32_t initial_first;
    uint32_t initial_last;
    uint32_t retention_tail_bad;
    uint8_t retention_present;
    uint8_t retention_pass;

    uint32_t fast_erase_status;
    uint32_t fast_erase_statr;
    uint32_t std_erase1_status;
    uint32_t std_erase1_statr;
    uint32_t fast_program_status;
    uint32_t fast_program_statr;
    uint32_t fast_program_pages;
    uint32_t fast_program_api_fail_page;
    uint32_t std_erase2_status;
    uint32_t std_erase2_statr;
    uint32_t word_program_status;
    uint32_t word_program_statr;
    uint32_t word_program_words;
    uint32_t word_program_tail_bad;

    uint32_t alias_crc_080;
    uint32_t alias_crc_000;
    uint32_t alias_mismatch_words;
    uint32_t read_stability_crc;
    uint32_t read_stability_failures;
    uint32_t guard_crc_before;
    uint32_t guard_crc_after;

    uint8_t fast_erase_pass;
    uint8_t std_erase1_pass;
    uint8_t fast_program_pass;
    uint8_t alias_pass;
    uint8_t read_stability_pass;
    uint8_t std_erase2_pass;
    uint8_t word_program_pass;
    uint8_t delayed_program_pass;
    uint8_t guard_checked;
    uint8_t guard_pass;
    uint8_t wrperr_seen;
    uint8_t eop_seen;
    uint8_t timeout_seen;
    uint8_t result_code;
} flash_test_result_t;

typedef uint32_t (*pattern_word_fn_t)(uint32_t index);

extern volatile uint16_t USBFS_CDC_ControlLineState;

#define FLASH_RETAIN \
    ((volatile flash_retain_record_t *)(uintptr_t)FLASH_RETAIN_ADDR)

static uint32_t s_page_buffer[FLASH_TEST_PAGE_WORDS]
    __attribute__((aligned(4)));
static flash_test_result_t s_result;
static uint8_t s_watchdog_started;

static void memory_fence(void)
{
    __asm volatile("fence rw, rw" ::: "memory");
}

static void delay_cycles(uint32_t cycles)
{
    volatile uint32_t i;

    for(i = 0U; i < cycles; i++)
    {
        __asm volatile("nop");
    }
}

static void board_power_init(void)
{
    SystemInit();

    /*
     * This is the same VIO18 selection used by the current product V3F main
     * before it starts the known-good USB stack.
     */
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    PWR_VIO18ModeCfg(PWR_VIO18CFGMODE_SW);
    PWR_VIO18LevelCfg(PWR_VIO18Level_MODE3);
    delay_cycles(10000U);
}

static void pe12_drive_low(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOE, ENABLE);

    /* Set the output latch first so the external pull-up sees a clean low. */
    GPIO_ResetBits(GPIOE, GPIO_Pin_12);
    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_High;
    GPIO_Init(GPIOE, &gpio);
    GPIO_ResetBits(GPIOE, GPIO_Pin_12);
}

static uint32_t crc32_bytes(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    crc = ~crc;
    for(i = 0U; i < len; i++)
    {
        uint32_t bit;

        crc ^= data[i];
        for(bit = 0U; bit < 8U; bit++)
        {
            uint32_t mask = 0U - (crc & 1U);

            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

static uint32_t crc32_region(uint32_t address, uint32_t bytes)
{
    const volatile uint32_t *source =
        (const volatile uint32_t *)(uintptr_t)address;
    uint32_t words = bytes / sizeof(uint32_t);
    uint32_t crc = 0U;
    uint32_t i;

    memory_fence();
    for(i = 0U; i < words; i++)
    {
        uint32_t value = source[i];

        crc = crc32_bytes(crc, (const uint8_t *)&value, sizeof(value));
    }
    memory_fence();
    return crc;
}

static uint32_t fast_pattern_word(uint32_t index)
{
    uint32_t page = index / FLASH_TEST_PAGE_WORDS;
    uint32_t x;

    switch(page)
    {
        case 0U:
            return 0x00000000UL;

        case 1U:
            return 0xAAAAAAAAUL;

        case 2U:
            return 0x55555555UL;

        case 3U:
            return ~(1UL << (index & 31U));

        case 4U:
            return 1UL << (index & 31U);

        default:
            x = 0x4175A55AUL + (0x9E3779B9UL * (index + 1U));
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            return x ^ (index * 0x85EBCA6BUL);
    }
}

static uint32_t final_pattern_word(uint32_t index)
{
    uint32_t x;

    if(index == 0U)
    {
        return FLASH_FINAL_MAGIC;
    }

    x = 0xC001D00DUL ^ (index * 0x27D4EB2DUL);
    x ^= x >> 15;
    x *= 0x85EBCA6BUL;
    x ^= x >> 13;
    return x;
}

static void memory_check_reset(memory_check_t *check)
{
    memset(check, 0, sizeof(*check));
    check->first_bad_offset = 0xFFFFFFFFUL;
}

static void scan_content(uint32_t address, uint32_t bytes,
                         memory_check_t *check)
{
    const volatile uint32_t *source =
        (const volatile uint32_t *)(uintptr_t)address;
    uint32_t words = bytes / sizeof(uint32_t);
    uint32_t i;

    memory_check_reset(check);
    memory_fence();
    for(i = 0U; i < words; i++)
    {
        uint32_t value = source[i];

        check->read_crc =
            crc32_bytes(check->read_crc, (const uint8_t *)&value,
                        sizeof(value));
        if(value == FLASH_ERASED_WORD)
        {
            check->erased_words++;
        }
        else if(value == 0x00000000UL)
        {
            check->zero_words++;
        }
        else
        {
            check->other_words++;
        }
    }
    memory_fence();
}

static void check_erased(uint32_t address, uint32_t bytes,
                         memory_check_t *check)
{
    const volatile uint32_t *source =
        (const volatile uint32_t *)(uintptr_t)address;
    uint32_t words = bytes / sizeof(uint32_t);
    uint32_t i;

    scan_content(address, bytes, check);
    check->mismatch_words = words - check->erased_words;
    if(check->mismatch_words == 0U)
    {
        return;
    }

    for(i = 0U; i < words; i++)
    {
        uint32_t actual = source[i];

        if(actual != FLASH_ERASED_WORD)
        {
            check->first_bad_offset = i * sizeof(uint32_t);
            check->first_expected = FLASH_ERASED_WORD;
            check->first_actual = actual;
            break;
        }
    }
}

static void check_pattern(uint32_t address, uint32_t words,
                          pattern_word_fn_t pattern,
                          memory_check_t *check)
{
    const volatile uint32_t *source =
        (const volatile uint32_t *)(uintptr_t)address;
    uint32_t i;

    memory_check_reset(check);
    memory_fence();
    for(i = 0U; i < words; i++)
    {
        uint32_t expected = pattern(i);
        uint32_t actual = source[i];

        check->expected_crc =
            crc32_bytes(check->expected_crc, (const uint8_t *)&expected,
                        sizeof(expected));
        check->read_crc =
            crc32_bytes(check->read_crc, (const uint8_t *)&actual,
                        sizeof(actual));
        if(actual == FLASH_ERASED_WORD)
        {
            check->erased_words++;
        }
        else if(actual == 0x00000000UL)
        {
            check->zero_words++;
        }
        else
        {
            check->other_words++;
        }

        if(actual != expected)
        {
            if(check->first_bad_offset == 0xFFFFFFFFUL)
            {
                check->first_bad_offset = i * sizeof(uint32_t);
                check->first_expected = expected;
                check->first_actual = actual;
            }
            check->mismatch_words++;
        }
    }
    memory_fence();
}

static uint32_t count_alias_mismatches(void)
{
    const volatile uint32_t *alias_080 =
        (const volatile uint32_t *)(uintptr_t)FLASH_TEST_ADDR;
    const volatile uint32_t *alias_000 =
        (const volatile uint32_t *)(uintptr_t)FLASH_TEST_ALIAS_ADDR;
    uint32_t mismatches = 0U;
    uint32_t i;

    memory_fence();
    for(i = 0U; i < FLASH_TEST_WORDS; i++)
    {
        if(alias_080[i] != alias_000[i])
        {
            mismatches++;
        }
    }
    memory_fence();
    return mismatches;
}

static uint8_t retain_record_valid(void)
{
    return (uint8_t)(((FLASH_RETAIN->magic == FLASH_RETAIN_MAGIC) &&
                      (FLASH_RETAIN->magic_inv ==
                       FLASH_RETAIN_MAGIC_INV)) ? 1U : 0U);
}

static uint8_t stage_was_in_progress(uint32_t stage)
{
    return (uint8_t)((((stage >= FLASH_STAGE_INITIAL_READ) &&
                       (stage < FLASH_STAGE_DONE)) &&
                      (stage != FLASH_STAGE_RECOVERED)) ? 1U : 0U);
}

static void retain_stage_set(uint32_t stage, uint32_t detail)
{
    FLASH_RETAIN->magic = FLASH_RETAIN_MAGIC;
    FLASH_RETAIN->magic_inv = FLASH_RETAIN_MAGIC_INV;
    FLASH_RETAIN->stage = stage;
    FLASH_RETAIN->detail = detail;
    FLASH_RETAIN->reset_flags = RCC->RSTSCKR;
    FLASH_RETAIN->statr = FLASH->STATR;
    FLASH_RETAIN->ctlr = FLASH->CTLR;
    FLASH_RETAIN->addr = FLASH->ADDR;
    memory_fence();
}

static void watchdog_feed(void)
{
    if(s_watchdog_started != 0U)
    {
        IWDG_ReloadCounter();
    }
}

static void watchdog_start_bounded(void)
{
    uint32_t polls = IWDG_READY_POLLS;

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload(IWDG_RELOAD_VALUE);
    IWDG_ReloadCounter();

    /*
     * Do not call IWDG_Enable(): the vendor helper waits forever for LSI.
     * Start it directly and bound the ready poll so a clock fault is still
     * reportable over CDC.
     */
    IWDG->CTLR = IWDG_KEY_ENABLE;
    s_watchdog_started = 1U;
    while(((RCC->RSTSCKR & IWDG_READY_MASK) == 0U) && (polls != 0U))
    {
        polls--;
    }
    s_result.watchdog_ready =
        (uint8_t)(((RCC->RSTSCKR & IWDG_READY_MASK) != 0U) ? 1U : 0U);
    watchdog_feed();
}

static void capture_operation_flags(uint32_t statr, uint32_t status)
{
    if((statr & FLASH_STATR_WRPRTERR_MASK) != 0U)
    {
        s_result.wrperr_seen = 1U;
    }
    if((statr & FLASH_STATR_EOP_MASK) != 0U)
    {
        s_result.eop_seen = 1U;
    }
    if(status == (uint32_t)FLASH_TIMEOUT)
    {
        s_result.timeout_seen = 1U;
    }
}

static FLASH_Status standard_erase_page(void)
{
    FLASH_Status status;

    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);
    FLASH_Unlock();
    status = FLASH_ErasePage(FLASH_TEST_ADDR);
    FLASH_Lock();
    return status;
}

static void fill_fast_page(uint32_t page)
{
    uint32_t i;
    uint32_t base = page * FLASH_TEST_PAGE_WORDS;

    for(i = 0U; i < FLASH_TEST_PAGE_WORDS; i++)
    {
        s_page_buffer[i] = fast_pattern_word(base + i);
    }
}

static void test_previous_retention(void)
{
    const volatile uint32_t *target =
        (const volatile uint32_t *)(uintptr_t)FLASH_TEST_ADDR;
    memory_check_t tail;

    if(target[0] != FLASH_FINAL_MAGIC)
    {
        s_result.retention_present = 0U;
        return;
    }

    s_result.retention_present = 1U;
    check_pattern(FLASH_TEST_ADDR, FLASH_TEST_PAGE_WORDS,
                  final_pattern_word, &s_result.retention);
    check_erased(FLASH_TEST_ADDR + FLASH_TEST_PAGE_SIZE,
                 FLASH_TEST_ERASE_SIZE - FLASH_TEST_PAGE_SIZE, &tail);
    s_result.retention_tail_bad = tail.mismatch_words;
    s_result.retention_pass =
        (uint8_t)(((s_result.retention.mismatch_words == 0U) &&
                   (s_result.retention_tail_bad == 0U)) ? 1U : 0U);
}

static void run_read_stability_test(void)
{
    uint32_t i;

    retain_stage_set(FLASH_STAGE_READ_STABILITY, 0U);
    s_result.read_stability_crc =
        crc32_region(FLASH_TEST_ADDR, FLASH_TEST_ERASE_SIZE);
    s_result.read_stability_failures = 0U;

    for(i = 1U; i < FLASH_TEST_READ_REPEATS; i++)
    {
        uint32_t crc;

        watchdog_feed();
        Delay_Ms(2U);
        crc = crc32_region(FLASH_TEST_ADDR, FLASH_TEST_ERASE_SIZE);
        if(crc != s_result.read_stability_crc)
        {
            s_result.read_stability_failures++;
        }
    }
    s_result.read_stability_pass =
        (uint8_t)((s_result.read_stability_failures == 0U) ? 1U : 0U);
}

static void run_fast_erase_with_fallback(void)
{
    if(s_result.watchdog_ready != 0U)
    {
        FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);
        retain_stage_set(FLASH_STAGE_FAST_ERASE_ARMED, 0U);
        watchdog_feed();
        s_result.fast_erase_status =
            (uint32_t)FLASH_ROM_ERASE(FLASH_TEST_ADDR,
                                      FLASH_TEST_ERASE_SIZE);
        retain_stage_set(FLASH_STAGE_FAST_ERASE_RETURNED, 0U);
        s_result.fast_erase_statr = FLASH->STATR;
        capture_operation_flags(s_result.fast_erase_statr,
                                s_result.fast_erase_status);

        retain_stage_set(FLASH_STAGE_FAST_ERASE_VERIFY, 0U);
        check_erased(FLASH_TEST_ADDR, FLASH_TEST_ERASE_SIZE,
                     &s_result.fast_erase);
        s_result.fast_erase_pass =
            (uint8_t)((s_result.fast_erase.mismatch_words == 0U) ? 1U : 0U);
    }

    if(s_result.fast_erase_pass == 0U)
    {
        retain_stage_set(FLASH_STAGE_STD_ERASE1_ARMED, 0U);
        watchdog_feed();
        s_result.std_erase1_status = (uint32_t)standard_erase_page();
        retain_stage_set(FLASH_STAGE_STD_ERASE1_RETURNED, 0U);
        s_result.std_erase1_statr = FLASH->STATR;
        capture_operation_flags(s_result.std_erase1_statr,
                                s_result.std_erase1_status);
        check_erased(FLASH_TEST_ADDR, FLASH_TEST_ERASE_SIZE,
                     &s_result.std_erase1);
        s_result.std_erase1_pass =
            (uint8_t)((s_result.std_erase1.mismatch_words == 0U) ? 1U : 0U);
    }
}

static void run_fast_program_test(void)
{
    uint32_t page;

    if(s_result.watchdog_ready == 0U)
    {
        return;
    }

    s_result.fast_program_api_fail_page = 0xFFFFFFFFUL;
    s_result.fast_program_status = FLASH_COMPLETE;
    for(page = 0U; page < FLASH_TEST_PAGES; page++)
    {
        uint32_t status;

        fill_fast_page(page);
        FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);
        retain_stage_set(FLASH_STAGE_FAST_PROGRAM_ARMED, page);
        watchdog_feed();
        status = (uint32_t)FLASH_ROM_WRITE(
            FLASH_TEST_ADDR + (page * FLASH_TEST_PAGE_SIZE),
            s_page_buffer, FLASH_TEST_PAGE_SIZE);
        s_result.fast_program_pages++;
        s_result.fast_program_status = status;
        s_result.fast_program_statr = FLASH->STATR;
        capture_operation_flags(s_result.fast_program_statr, status);

        if((status != (uint32_t)FLASH_COMPLETE) ||
           ((s_result.fast_program_statr &
             FLASH_STATR_WRPRTERR_MASK) != 0U))
        {
            s_result.fast_program_api_fail_page = page;
            break;
        }
    }

    retain_stage_set(FLASH_STAGE_FAST_PROGRAM_RETURNED,
                     s_result.fast_program_pages);
    retain_stage_set(FLASH_STAGE_FAST_PROGRAM_VERIFY, 0U);
    check_pattern(FLASH_TEST_ADDR, FLASH_TEST_WORDS, fast_pattern_word,
                  &s_result.fast_program);
    s_result.fast_program_pass =
        (uint8_t)((s_result.fast_program.mismatch_words == 0U) ? 1U : 0U);

    s_result.alias_crc_080 =
        crc32_region(FLASH_TEST_ADDR, FLASH_TEST_ERASE_SIZE);
    s_result.alias_crc_000 =
        crc32_region(FLASH_TEST_ALIAS_ADDR, FLASH_TEST_ERASE_SIZE);
    s_result.alias_mismatch_words = count_alias_mismatches();
    s_result.alias_pass =
        (uint8_t)(((s_result.alias_crc_080 == s_result.alias_crc_000) &&
                   (s_result.alias_mismatch_words == 0U)) ? 1U : 0U);

    run_read_stability_test();
}

static void run_standard_erase_and_program(void)
{
    uint32_t word;
    memory_check_t tail;

    retain_stage_set(FLASH_STAGE_STD_ERASE2_ARMED, 0U);
    watchdog_feed();
    s_result.std_erase2_status = (uint32_t)standard_erase_page();
    retain_stage_set(FLASH_STAGE_STD_ERASE2_RETURNED, 0U);
    s_result.std_erase2_statr = FLASH->STATR;
    capture_operation_flags(s_result.std_erase2_statr,
                            s_result.std_erase2_status);
    check_erased(FLASH_TEST_ADDR, FLASH_TEST_ERASE_SIZE,
                 &s_result.std_erase2);
    s_result.std_erase2_pass =
        (uint8_t)((s_result.std_erase2.mismatch_words == 0U) ? 1U : 0U);
    if(s_result.std_erase2_pass == 0U)
    {
        return;
    }

    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);
    FLASH_Unlock();
    s_result.word_program_status = FLASH_COMPLETE;
    for(word = 0U; word < FLASH_TEST_PAGE_WORDS; word++)
    {
        FLASH_Status status;

        retain_stage_set(FLASH_STAGE_WORD_PROGRAM_ARMED, word);
        watchdog_feed();
        status = FLASH_ProgramWord(
            FLASH_TEST_ADDR + (word * sizeof(uint32_t)),
            final_pattern_word(word));
        s_result.word_program_status = (uint32_t)status;
        s_result.word_program_words++;
        if(status != FLASH_COMPLETE)
        {
            break;
        }
    }
    FLASH_Lock();
    retain_stage_set(FLASH_STAGE_WORD_PROGRAM_RETURNED,
                     s_result.word_program_words);
    s_result.word_program_statr = FLASH->STATR;
    capture_operation_flags(s_result.word_program_statr,
                            s_result.word_program_status);

    check_pattern(FLASH_TEST_ADDR, FLASH_TEST_PAGE_WORDS,
                  final_pattern_word, &s_result.word_program);
    check_erased(FLASH_TEST_ADDR + FLASH_TEST_PAGE_SIZE,
                 FLASH_TEST_ERASE_SIZE - FLASH_TEST_PAGE_SIZE, &tail);
    s_result.word_program_tail_bad = tail.mismatch_words;
    s_result.word_program_pass =
        (uint8_t)(((s_result.word_program.mismatch_words == 0U) &&
                   (s_result.word_program_tail_bad == 0U)) ? 1U : 0U);

    retain_stage_set(FLASH_STAGE_DELAYED_VERIFY, 0U);
    for(word = 0U; word < 25U; word++)
    {
        watchdog_feed();
        Delay_Ms(10U);
    }
    check_pattern(FLASH_TEST_ADDR, FLASH_TEST_PAGE_WORDS,
                  final_pattern_word, &s_result.delayed_program);
    s_result.delayed_program_pass =
        (uint8_t)((s_result.delayed_program.mismatch_words == 0U) ? 1U : 0U);
}

static void classify_result(void)
{
    uint8_t erase_available =
        (uint8_t)(((s_result.fast_erase_pass != 0U) ||
                   (s_result.std_erase1_pass != 0U)) ? 1U : 0U);
    uint8_t retention_ok =
        (uint8_t)(((s_result.retention_present == 0U) ||
                   (s_result.retention_pass != 0U)) ? 1U : 0U);

    if(s_result.recovered != 0U)
    {
        s_result.result_code = FLASH_RESULT_FAIL;
        return;
    }

    if((s_result.flash_capacity_kib == 960U) &&
       (s_result.pe12_low != 0U) &&
       (s_result.watchdog_ready != 0U) &&
       (s_result.fast_erase_pass != 0U) &&
       (s_result.fast_program_pass != 0U) &&
       (s_result.alias_pass != 0U) &&
       (s_result.read_stability_pass != 0U) &&
       (s_result.std_erase2_pass != 0U) &&
       (s_result.word_program_pass != 0U) &&
       (s_result.delayed_program_pass != 0U) &&
       (s_result.guard_pass != 0U) &&
       (retention_ok != 0U) &&
       (s_result.test_write_protected == 0U) &&
       (s_result.wrperr_seen == 0U) &&
       (s_result.timeout_seen == 0U))
    {
        s_result.result_code = FLASH_RESULT_PASS;
    }
    else if((erase_available != 0U) &&
            (s_result.std_erase2_pass != 0U) &&
            (s_result.word_program_pass != 0U) &&
            (s_result.delayed_program_pass != 0U) &&
            (s_result.guard_pass != 0U))
    {
        s_result.result_code = FLASH_RESULT_DEGRADED;
    }
    else
    {
        s_result.result_code = FLASH_RESULT_FAIL;
    }
}

static const char *diagnosis_name(void)
{
    if(s_result.recovered != 0U)
    {
        return (s_result.iwdg_reset != 0U) ?
                   "WATCHDOG_RECOVERY" : "TEST_INTERRUPTED_RESET";
    }
    if(s_result.flash_capacity_kib != 960U)
    {
        return "CAPACITY_NOT_960K";
    }
    if(s_result.pe12_low == 0U)
    {
        return "PE12_DRIVE_FAIL";
    }
    if((s_result.fast_erase_pass == 0U) &&
       (s_result.std_erase1_pass == 0U))
    {
        return ((s_result.wrperr_seen != 0U) ||
                (s_result.test_write_protected != 0U)) ?
                   "ERASE_WRITE_PROTECTED" : "ERASE_BLOCKED_OR_DEAD";
    }
    if((s_result.std_erase2_pass == 0U) ||
       (s_result.word_program_pass == 0U))
    {
        return ((s_result.wrperr_seen != 0U) ||
                (s_result.test_write_protected != 0U)) ?
                   "PROGRAM_WRITE_PROTECTED" : "STANDARD_PROGRAM_FAIL";
    }
    if(s_result.watchdog_ready == 0U)
    {
        return "WATCHDOG_CLOCK_FAIL";
    }
    if(s_result.read_stability_pass == 0U)
    {
        return "READ_UNSTABLE";
    }
    if(s_result.alias_pass == 0U)
    {
        return "FLASH_ALIAS_MISMATCH";
    }
    if(s_result.guard_pass == 0U)
    {
        return "ADJACENT_PAGE_CORRUPTED";
    }
    if((s_result.retention_present != 0U) &&
       (s_result.retention_pass == 0U))
    {
        return "POWER_CYCLE_RETENTION_FAIL";
    }
    if((s_result.fast_erase_pass == 0U) ||
       (s_result.fast_program_pass == 0U))
    {
        return "FLASH_ALIVE_FAST_PATH_FAIL";
    }
    return "FLASH_OK";
}

static const char *check_state(uint8_t attempted, uint8_t passed)
{
    if(attempted == 0U)
    {
        return "SKIP";
    }
    return (passed != 0U) ? "PASS" : "FAIL";
}

static const char *result_name(void)
{
    if(s_result.result_code == FLASH_RESULT_PASS)
    {
        return "PASS";
    }
    if(s_result.result_code == FLASH_RESULT_DEGRADED)
    {
        return "DEGRADED";
    }
    return "FAIL";
}

static void capture_static_diagnostics(void)
{
    s_result.chip_id = DBGMCU_GetCHIPID();
    s_result.reset_flags = RCC->RSTSCKR;
    s_result.iwdg_reset =
        (uint8_t)((RCC_GetFlagStatus(RCC_FLAG_IWDGRST) == SET) ? 1U : 0U);
    if(s_result.iwdg_reset != 0U)
    {
        s_result.watchdog_ready = 1U;
    }
    s_result.cfgr0 = *(volatile uint32_t *)(uintptr_t)FLASH_CFGR0_BASE;
    s_result.flash_capacity_kib =
        (FLASH_GetCapacity() == FLASHCapacity_960K) ? 960U : 480U;
    s_result.actlr_initial = FLASH->ACTLR;
    s_result.statr_initial = FLASH->STATR;
    s_result.ctlr_initial = FLASH->CTLR;
    s_result.addr_initial = FLASH->ADDR;
    s_result.obr = FLASH->OBR;
    s_result.wpr = FLASH->WPR;
    /*
     * WPR bit 31 covers dual-bank sectors 31..119, including the final
     * 8 KiB test page. WCH option-byte write protection is active-low.
     */
    s_result.test_write_protected =
        (uint8_t)(((s_result.wpr & FLASH_TEST_WPR_MASK) == 0U) ? 1U : 0U);
    s_result.ob_wrpr0 = OB->WRPR0;
    s_result.ob_wrpr1 = OB->WRPR1;
    s_result.ob_wrpr2 = OB->WRPR2;
    s_result.ob_wrpr3 = OB->WRPR3;
}

static void run_flash_test(void)
{
    memset(&s_result, 0, sizeof(s_result));
    s_result.fast_program_api_fail_page = 0xFFFFFFFFUL;
    s_result.pe12_low =
        (uint8_t)((GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_12) == Bit_RESET) ?
                  1U : 0U);

    capture_static_diagnostics();

    if((retain_record_valid() != 0U) &&
       (stage_was_in_progress(FLASH_RETAIN->stage) != 0U))
    {
        s_result.recovered = 1U;
        s_result.recovery_stage = FLASH_RETAIN->stage;
        s_result.recovery_detail = FLASH_RETAIN->detail;
        s_result.recovery_statr = FLASH_RETAIN->statr;
        s_result.recovery_ctlr = FLASH_RETAIN->ctlr;
        retain_stage_set(FLASH_STAGE_RECOVERED, s_result.recovery_stage);
        classify_result();
        return;
    }

    retain_stage_set(FLASH_STAGE_BOOT, 0U);
    RCC_ClearFlag();
    watchdog_start_bounded();
    retain_stage_set(FLASH_STAGE_INITIAL_READ, 0U);
    watchdog_feed();

    if(s_result.flash_capacity_kib != 960U)
    {
        classify_result();
        retain_stage_set(FLASH_STAGE_DONE, s_result.result_code);
        return;
    }

    scan_content(FLASH_TEST_ADDR, FLASH_TEST_ERASE_SIZE, &s_result.initial);
    s_result.initial_first =
        *(const volatile uint32_t *)(uintptr_t)FLASH_TEST_ADDR;
    s_result.initial_last =
        *(const volatile uint32_t *)(uintptr_t)
            (FLASH_TEST_END_ADDR - sizeof(uint32_t) + 1U);
    test_previous_retention();
    s_result.guard_crc_before =
        crc32_region(FLASH_TEST_GUARD_ADDR, FLASH_TEST_PAGE_SIZE);

    run_fast_erase_with_fallback();
    if((s_result.fast_erase_pass != 0U) ||
       (s_result.std_erase1_pass != 0U))
    {
        run_fast_program_test();
        run_standard_erase_and_program();
    }

    s_result.guard_crc_after =
        crc32_region(FLASH_TEST_GUARD_ADDR, FLASH_TEST_PAGE_SIZE);
    s_result.guard_checked = 1U;
    s_result.guard_pass =
        (uint8_t)((s_result.guard_crc_before ==
                   s_result.guard_crc_after) ? 1U : 0U);

    classify_result();
    retain_stage_set(FLASH_STAGE_DONE, s_result.result_code);
    watchdog_feed();
}

static void format_report_line(uint8_t index, char line[96])
{
    uint8_t fast_erase_attempted =
        (uint8_t)((s_result.fast_erase_status != STATUS_NOT_RUN) ? 1U : 0U);
    uint8_t std_erase1_attempted =
        (uint8_t)((s_result.std_erase1_status != STATUS_NOT_RUN) ? 1U : 0U);
    uint8_t fast_program_attempted =
        (uint8_t)((s_result.fast_program_pages != 0U) ? 1U : 0U);
    uint8_t std_erase2_attempted =
        (uint8_t)((s_result.std_erase2_status != STATUS_NOT_RUN) ? 1U : 0U);
    uint8_t word_program_attempted =
        (uint8_t)((s_result.word_program_words != 0U) ? 1U : 0U);

    switch(index)
    {
        case 0U:
            (void)snprintf(line, 96U, "H417 INTERNAL FLASH DIAG v3\r\n");
            break;

        case 1U:
            (void)snprintf(line, 96U,
                           "RANGE %08lX-%08lX PE12=%s\r\n",
                           (unsigned long)FLASH_TEST_ADDR,
                           (unsigned long)FLASH_TEST_END_ADDR,
                           (s_result.pe12_low != 0U) ? "LOW" : "FAIL");
            break;

        case 2U:
            (void)snprintf(line, 96U,
                           "CHIP id=%08lX cap=%luK dual=%u wd=%u\r\n",
                           (unsigned long)s_result.chip_id,
                           (unsigned long)s_result.flash_capacity_kib,
                           (unsigned int)
                               ((s_result.cfgr0 & FLASH_CFGR0_DUAL_BANK) != 0U),
                           (unsigned int)s_result.watchdog_ready);
            break;

        case 3U:
            (void)snprintf(line, 96U,
                           "RESET flags=%08lX iwdg=%u rec=%u stage=%lu/%lu\r\n",
                           (unsigned long)s_result.reset_flags,
                           (unsigned int)s_result.iwdg_reset,
                           (unsigned int)s_result.recovered,
                           (unsigned long)s_result.recovery_stage,
                           (unsigned long)s_result.recovery_detail);
            break;

        case 4U:
            (void)snprintf(line, 96U,
                           "REG0 act=%08lX sta=%08lX ctl=%08lX\r\n",
                           (unsigned long)s_result.actlr_initial,
                           (unsigned long)s_result.statr_initial,
                           (unsigned long)s_result.ctlr_initial);
            break;

        case 5U:
            (void)snprintf(line, 96U,
                           "REG1 addr=%08lX obr=%08lX wpr=%08lX wp119=%u\r\n",
                           (unsigned long)s_result.addr_initial,
                           (unsigned long)s_result.obr,
                           (unsigned long)s_result.wpr,
                           (unsigned int)s_result.test_write_protected);
            break;

        case 6U:
            (void)snprintf(line, 96U,
                           "CFG cfgr0=%08lX obwr=%04X/%04X/%04X/%04X\r\n",
                           (unsigned long)s_result.cfgr0,
                           (unsigned int)s_result.ob_wrpr0,
                           (unsigned int)s_result.ob_wrpr1,
                           (unsigned int)s_result.ob_wrpr2,
                           (unsigned int)s_result.ob_wrpr3);
            break;

        case 7U:
            (void)snprintf(line, 96U,
                           "INITIAL erased=%lu zero=%lu other=%lu crc=%08lX\r\n",
                           (unsigned long)s_result.initial.erased_words,
                           (unsigned long)s_result.initial.zero_words,
                           (unsigned long)s_result.initial.other_words,
                           (unsigned long)s_result.initial.read_crc);
            break;

        case 8U:
            (void)snprintf(line, 96U,
                           "INITIAL first=%08lX last=%08lX retention=%s\r\n",
                           (unsigned long)s_result.initial_first,
                           (unsigned long)s_result.initial_last,
                           (s_result.retention_present == 0U) ? "NA" :
                               ((s_result.retention_pass != 0U) ?
                                    "PASS" : "FAIL"));
            break;

        case 9U:
            (void)snprintf(line, 96U,
                           "RETENTION bad=%lu tail=%lu crc=%08lX\r\n",
                           (unsigned long)s_result.retention.mismatch_words,
                           (unsigned long)s_result.retention_tail_bad,
                           (unsigned long)s_result.retention.read_crc);
            break;

        case 10U:
            (void)snprintf(line, 96U,
                           "ERASE_FAST %s api=%lu sta=%08lX erased=%lu\r\n",
                           check_state(fast_erase_attempted,
                                       s_result.fast_erase_pass),
                           (unsigned long)s_result.fast_erase_status,
                           (unsigned long)s_result.fast_erase_statr,
                           (unsigned long)s_result.fast_erase.erased_words);
            break;

        case 11U:
            (void)snprintf(line, 96U,
                           "ERASE_FAST bad=%lu off=%08lX val=%08lX\r\n",
                           (unsigned long)s_result.fast_erase.mismatch_words,
                           (unsigned long)s_result.fast_erase.first_bad_offset,
                           (unsigned long)s_result.fast_erase.first_actual);
            break;

        case 12U:
            (void)snprintf(line, 96U,
                           "ERASE_STD1 %s api=%lu sta=%08lX erased=%lu\r\n",
                           check_state(std_erase1_attempted,
                                       s_result.std_erase1_pass),
                           (unsigned long)s_result.std_erase1_status,
                           (unsigned long)s_result.std_erase1_statr,
                           (unsigned long)s_result.std_erase1.erased_words);
            break;

        case 13U:
            (void)snprintf(line, 96U,
                           "PROG_FAST %s pages=%lu api=%lu failpage=%ld\r\n",
                           check_state(fast_program_attempted,
                                       s_result.fast_program_pass),
                           (unsigned long)s_result.fast_program_pages,
                           (unsigned long)s_result.fast_program_status,
                           (long)((s_result.fast_program_api_fail_page ==
                                   0xFFFFFFFFUL) ?
                                      -1L :
                                      (long)s_result
                                          .fast_program_api_fail_page));
            break;

        case 14U:
            (void)snprintf(line, 96U,
                           "PROG_FAST bad=%lu off=%08lX exp=%08lX got=%08lX\r\n",
                           (unsigned long)s_result.fast_program.mismatch_words,
                           (unsigned long)s_result.fast_program.first_bad_offset,
                           (unsigned long)s_result.fast_program.first_expected,
                           (unsigned long)s_result.fast_program.first_actual);
            break;

        case 15U:
            (void)snprintf(line, 96U,
                           "PROG_CRC exp=%08lX read=%08lX sta=%08lX\r\n",
                           (unsigned long)s_result.fast_program.expected_crc,
                           (unsigned long)s_result.fast_program.read_crc,
                           (unsigned long)s_result.fast_program_statr);
            break;

        case 16U:
            (void)snprintf(line, 96U,
                           "ALIAS %s diff=%lu c80=%08lX c00=%08lX\r\n",
                           check_state(fast_program_attempted,
                                       s_result.alias_pass),
                           (unsigned long)s_result.alias_mismatch_words,
                           (unsigned long)s_result.alias_crc_080,
                           (unsigned long)s_result.alias_crc_000);
            break;

        case 17U:
            (void)snprintf(line, 96U,
                           "READ_REPEAT %s n=%u unstable=%lu crc=%08lX\r\n",
                           check_state(fast_program_attempted,
                                       s_result.read_stability_pass),
                           (unsigned int)FLASH_TEST_READ_REPEATS,
                           (unsigned long)s_result.read_stability_failures,
                           (unsigned long)s_result.read_stability_crc);
            break;

        case 18U:
            (void)snprintf(line, 96U,
                           "ERASE_STD2 %s api=%lu sta=%08lX erased=%lu\r\n",
                           check_state(std_erase2_attempted,
                                       s_result.std_erase2_pass),
                           (unsigned long)s_result.std_erase2_status,
                           (unsigned long)s_result.std_erase2_statr,
                           (unsigned long)s_result.std_erase2.erased_words);
            break;

        case 19U:
            (void)snprintf(line, 96U,
                           "PROG_WORD %s words=%lu api=%lu sta=%08lX\r\n",
                           check_state(word_program_attempted,
                                       s_result.word_program_pass),
                           (unsigned long)s_result.word_program_words,
                           (unsigned long)s_result.word_program_status,
                           (unsigned long)s_result.word_program_statr);
            break;

        case 20U:
            (void)snprintf(line, 96U,
                           "PROG_WORD bad=%lu tail=%lu crc=%08lX/%08lX\r\n",
                           (unsigned long)s_result.word_program.mismatch_words,
                           (unsigned long)s_result.word_program_tail_bad,
                           (unsigned long)s_result.word_program.expected_crc,
                           (unsigned long)s_result.word_program.read_crc);
            break;

        case 21U:
            (void)snprintf(line, 96U,
                           "DELAY_250 %s bad=%lu crc=%08lX\r\n",
                           check_state(word_program_attempted,
                                       s_result.delayed_program_pass),
                           (unsigned long)s_result
                               .delayed_program.mismatch_words,
                           (unsigned long)s_result.delayed_program.read_crc);
            break;

        case 22U:
            (void)snprintf(line, 96U,
                           "GUARD %s before=%08lX after=%08lX\r\n",
                           check_state(s_result.guard_checked,
                               s_result.guard_pass),
                           (unsigned long)s_result.guard_crc_before,
                           (unsigned long)s_result.guard_crc_after);
            break;

        case 23U:
            (void)snprintf(line, 96U,
                           "FLAGS wrperr=%u eop=%u timeout=%u\r\n",
                           (unsigned int)s_result.wrperr_seen,
                           (unsigned int)s_result.eop_seen,
                           (unsigned int)s_result.timeout_seen);
            break;

        case 24U:
            (void)snprintf(line, 96U,
                           "RECOVERY sta=%08lX ctl=%08lX\r\n",
                           (unsigned long)s_result.recovery_statr,
                           (unsigned long)s_result.recovery_ctlr);
            break;

        case 25U:
            (void)snprintf(line, 96U, "DIAG %s\r\n", diagnosis_name());
            break;

        default:
            (void)snprintf(line, 96U, "RESULT %s\r\n", result_name());
            break;
    }
}

static void cdc_report_poll(void)
{
    static uint8_t previous_dtr;
    static uint8_t report_line = CDC_REPORT_LINE_COUNT;
    static char line[96];
    static uint16_t line_length;
    static uint16_t line_offset;
    uint8_t dtr =
        (uint8_t)(((USBFS_CDC_ControlLineState & 0x0001U) != 0U) ? 1U : 0U);

    if(dtr == 0U)
    {
        previous_dtr = 0U;
        report_line = CDC_REPORT_LINE_COUNT;
        line_length = 0U;
        line_offset = 0U;
        return;
    }

    if(previous_dtr == 0U)
    {
        previous_dtr = 1U;
        report_line = 0U;
        line_length = 0U;
        line_offset = 0U;
    }

    if(report_line < CDC_REPORT_LINE_COUNT)
    {
        uint16_t remaining;
        uint16_t chunk;

        if(line_length == 0U)
        {
            format_report_line(report_line, line);
            line_length = (uint16_t)strlen(line);
            line_offset = 0U;
        }

        remaining = line_length - line_offset;
        chunk = (remaining > 64U) ? 64U : remaining;
        if(ch32h417_usbfs_hid_nkro_debug_write(&line[line_offset]) != 0U)
        {
            line_offset += chunk;
            if(line_offset >= line_length)
            {
                report_line++;
                line_length = 0U;
                line_offset = 0U;
            }
        }
    }
}

int main(void)
{
    board_power_init();
    pe12_drive_low();

    /*
     * After an IWDG recovery reset the watchdog may already be counting.
     * Feed it before USB initialization so the recovery report can enumerate.
     */
    if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) == SET)
    {
        s_watchdog_started = 1U;
        IWDG_ReloadCounter();
    }

    /* Reuse the same product USBFS HID + CDC implementation as main. */
    ch32h417_usbfs_hid_nkro_init();

    run_flash_test();

    while(1)
    {
        /* PE12 must remain low even if later test code is added. */
        GPIO_ResetBits(GPIOE, GPIO_Pin_12);
        watchdog_feed();
        cdc_report_poll();
        Delay_Ms(1U);
    }
}
