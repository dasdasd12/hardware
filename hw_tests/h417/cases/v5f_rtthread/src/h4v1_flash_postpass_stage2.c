/*
 * H4V1 Flash Stage 2: read-only PAGE READ + READ FROM CACHE validation.
 *
 * The test finds the first factory-good block in the reserved future H4V1
 * region, reads four pages twice and compares an aggregate FNV-1a value.
 * No Flash state is changed: RESET, SET FEATURE, WRITE ENABLE, PROGRAM and
 * ERASE are deliberately absent.
 * The 2 KiB scratch window is shared SRAM that is idle after the qualified
 * first 90-frame pass and before the original infinite playback loop starts.
 */
#include "h4v1_flash_postpass_stage2.h"

#include <stdint.h>

#include <rtthread.h>

#include "ch32h417.h"
#include "ch32h417_gpio.h"

#define H4V1_POSTPASS_TEXT \
    __attribute__((section(".h4v1_postpass.text"), noinline))
#define H4V1_POSTPASS_ENTRY \
    __attribute__((section(".h4v1_postpass.text"), noinline, used))
#define H4V1_POSTPASS_RODATA \
    __attribute__((section(".h4v1_postpass.rodata"), aligned(4), used))

#define H4V1_STAGE2_CMD_GET_FEATURE       0x0Fu
#define H4V1_STAGE2_CMD_PAGE_READ         0x13u
#define H4V1_STAGE2_CMD_READ_CACHE        0x03u
#define H4V1_STAGE2_FEATURE_CONFIG        0xB0u
#define H4V1_STAGE2_FEATURE_STATUS        0xC0u
#define H4V1_STAGE2_FEATURE_STATUS2       0xF0u
#define H4V1_STAGE2_CONFIG_ECC_ENABLE     0x10u
#define H4V1_STAGE2_STATUS_OIP            0x01u
#define H4V1_STAGE2_STATUS_ECC_MASK       0x30u
#define H4V1_STAGE2_STATUS_ECC_CLEAN      0x00u
#define H4V1_STAGE2_STATUS_ECC_CORRECTED  0x10u
#define H4V1_STAGE2_STATUS_ECC_FAILED     0x20u
#define H4V1_STAGE2_STATUS_ECC_8_BITS     0x30u
#define H4V1_STAGE2_STATUS2_ECCSE_MASK    0x30u
#define H4V1_STAGE2_SPI_RXNE              0x0001u
#define H4V1_STAGE2_SPI_TXE               0x0002u
#define H4V1_STAGE2_SPI_BYTE_TIMEOUT      1000000u
#define H4V1_STAGE2_READY_TIMEOUT_CYCLES  40000000u
#define H4V1_STAGE2_CORE_HZ               400000000u
#define H4V1_STAGE2_PAGE_BYTES            2048u
#define H4V1_STAGE2_PAGES                 4u
#define H4V1_STAGE2_FIRST_BLOCK           768u
#define H4V1_STAGE2_LAST_BLOCK            1015u
#define H4V1_STAGE2_PAGES_PER_BLOCK       64u
#define H4V1_STAGE2_BAD_MARK_COLUMN       2048u
#define H4V1_STAGE2_SCRATCH_ADDR          0x20174000u
#define H4V1_STAGE2_FNV_OFFSET            2166136261u
#define H4V1_STAGE2_FNV_PRIME             16777619u

typedef struct
{
    uint32_t spi_timeout_count;
    uint32_t ready_timeout_count;
    uint32_t ready_polls;
    uint32_t cache_cycles;
    uint32_t total_cycles;
    uint32_t hash;
    uint32_t ff_count;
    uint32_t ecc_clean_pages;
    uint32_t ecc_corrected_pages;
    uint32_t ecc_uncorrectable_pages;
    uint8_t status_last;
    uint8_t status2_last;
    uint8_t ecc_worst_bits;
} h4v1_stage2_pass_t;

extern void h4v1_postpass_stage1_log(const char *text);

const char h4v1_postpass_stage2_start_text[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE2 START access=page_read+cache_read read_only=1 "
    "blocks=768..1015 select=first_factory_good pages=4 bytes=8192 repeats=2 "
    "scratch=20174000 scope=sanity_not_final_bandwidth "
    "no_reset=1 no_set_feature=1 no_dma=1 no_sdram=1";
const char h4v1_postpass_stage2_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE2 PASS block=%u row=%u scanned=%u marker=%02x b0=%02x "
    "hash=%08x/%08x ff=%u/%u status=%02x/%02x f0=%02x/%02x "
    "ecc=c%u/r%u/u%u worst_upper=%u scan_ecc=c%u/r%u/u%u "
    "polls=%u/%u cycles=%u/%u cache_cycles=%u/%u "
    "e2e_KiBps=%u cache_KiBps=%u scope=sanity spi_timeout=%u";
const char h4v1_postpass_stage2_fail_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE2 FAIL result=%d block=%u scanned=%u marker=%02x b0=%02x "
    "hash=%08x/%08x status=%02x/%02x f0=%02x/%02x "
    "ecc=c%u/r%u/u%u worst_upper=%u scan_ecc=c%u/r%u/u%u "
    "spi_timeout=%u ready_timeout=%u";

uint32_t H4V1_POSTPASS_TEXT h4v1_stage2_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

void H4V1_POSTPASS_TEXT h4v1_stage2_select(void)
{
    GPIOF->BCR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage2_deselect(void)
{
    GPIOF->BSHR = GPIO_Pin_6;
}

uint8_t H4V1_POSTPASS_TEXT
h4v1_stage2_transfer(h4v1_stage2_pass_t *pass, uint8_t tx)
{
    uint32_t timeout = H4V1_STAGE2_SPI_BYTE_TIMEOUT;

    while(((SPI1->STATR & H4V1_STAGE2_SPI_TXE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        pass->spi_timeout_count++;
        return 0xFFu;
    }
    SPI1->DATAR = tx;

    timeout = H4V1_STAGE2_SPI_BYTE_TIMEOUT;
    while(((SPI1->STATR & H4V1_STAGE2_SPI_RXNE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        pass->spi_timeout_count++;
        return 0xFFu;
    }
    return (uint8_t)SPI1->DATAR;
}

int H4V1_POSTPASS_TEXT
h4v1_stage2_get_feature(h4v1_stage2_pass_t *pass,
                        uint8_t address,
                        uint8_t *value_out)
{
    uint32_t timeout_before = pass->spi_timeout_count;

    if(timeout_before != 0u)
    {
        return -1;
    }
    h4v1_stage2_select();
    (void)h4v1_stage2_transfer(pass, H4V1_STAGE2_CMD_GET_FEATURE);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_stage2_transfer(pass, address);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    *value_out = h4v1_stage2_transfer(pass, 0x00u);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    h4v1_stage2_deselect();
    return 0;

transfer_failed:
    h4v1_stage2_deselect();
    return -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage2_wait_ready(h4v1_stage2_pass_t *pass, uint8_t *status_out)
{
    uint32_t start = h4v1_stage2_cycle_now();
    uint8_t status = 0xFFu;

    do
    {
        if(h4v1_stage2_get_feature(pass,
                                   H4V1_STAGE2_FEATURE_STATUS,
                                   &status) != 0)
        {
            *status_out = status;
            return -1;
        }
        pass->ready_polls++;
        if((status & H4V1_STAGE2_STATUS_OIP) == 0u)
        {
            *status_out = status;
            return 0;
        }
    } while((uint32_t)(h4v1_stage2_cycle_now() - start) <
            H4V1_STAGE2_READY_TIMEOUT_CYCLES);

    pass->ready_timeout_count++;
    *status_out = status;
    return -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage2_load_page(h4v1_stage2_pass_t *pass,
                      uint32_t row,
                      uint8_t *status_out);

int H4V1_POSTPASS_TEXT
h4v1_stage2_classify_ecc(h4v1_stage2_pass_t *pass, uint8_t status)
{
    uint8_t status2 = 0xFFu;
    uint8_t ecc = status & H4V1_STAGE2_STATUS_ECC_MASK;
    uint8_t corrected_bits = 0u;

    if(h4v1_stage2_get_feature(pass,
                               H4V1_STAGE2_FEATURE_STATUS2,
                               &status2) != 0)
    {
        return -1;
    }
    pass->status_last = status;
    pass->status2_last = status2;

    if(ecc == H4V1_STAGE2_STATUS_ECC_CLEAN)
    {
        pass->ecc_clean_pages++;
        return 0;
    }
    if(ecc == H4V1_STAGE2_STATUS_ECC_FAILED)
    {
        pass->ecc_uncorrectable_pages++;
        pass->ecc_worst_bits = 9u;
        return -2;
    }

    pass->ecc_corrected_pages++;
    if(ecc == H4V1_STAGE2_STATUS_ECC_8_BITS)
    {
        corrected_bits = 8u;
    }
    else if(ecc == H4V1_STAGE2_STATUS_ECC_CORRECTED)
    {
        switch(status2 & H4V1_STAGE2_STATUS2_ECCSE_MASK)
        {
        case 0x00u:
            /* The data sheet reports this bucket as <= 4 corrected bits. */
            corrected_bits = 4u;
            break;
        case 0x10u:
            corrected_bits = 5u;
            break;
        case 0x20u:
            corrected_bits = 6u;
            break;
        default:
            corrected_bits = 7u;
            break;
        }
    }
    if(corrected_bits > pass->ecc_worst_bits)
    {
        pass->ecc_worst_bits = corrected_bits;
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage2_load_and_classify(h4v1_stage2_pass_t *pass,
                              uint32_t row,
                              uint8_t *status_out)
{
    if(h4v1_stage2_load_page(pass, row, status_out) != 0)
    {
        return -1;
    }
    return h4v1_stage2_classify_ecc(pass, *status_out);
}

int H4V1_POSTPASS_TEXT
h4v1_stage2_load_page(h4v1_stage2_pass_t *pass,
                      uint32_t row,
                      uint8_t *status_out)
{
    uint32_t timeout_before = pass->spi_timeout_count;

    if(timeout_before != 0u)
    {
        return -1;
    }
    h4v1_stage2_select();
    (void)h4v1_stage2_transfer(pass, H4V1_STAGE2_CMD_PAGE_READ);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_stage2_transfer(pass, (uint8_t)(row >> 16));
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_stage2_transfer(pass, (uint8_t)(row >> 8));
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_stage2_transfer(pass, (uint8_t)row);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    h4v1_stage2_deselect();
    return h4v1_stage2_wait_ready(pass, status_out);

transfer_failed:
    h4v1_stage2_deselect();
    return -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage2_read_cache(h4v1_stage2_pass_t *pass,
                       uint16_t column,
                       uint8_t *data,
                       uint32_t length,
                       uint8_t accumulate)
{
    uint32_t index;
    uint32_t start = h4v1_stage2_cycle_now();
    uint32_t timeout_before = pass->spi_timeout_count;

    if(timeout_before != 0u)
    {
        return -1;
    }
    h4v1_stage2_select();
    (void)h4v1_stage2_transfer(pass, H4V1_STAGE2_CMD_READ_CACHE);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_stage2_transfer(pass, (uint8_t)(column >> 8));
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_stage2_transfer(pass, (uint8_t)column);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_stage2_transfer(pass, 0x00u);
    if(pass->spi_timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    for(index = 0u; index < length; ++index)
    {
        uint8_t value = h4v1_stage2_transfer(pass, 0x00u);

        if(pass->spi_timeout_count != timeout_before)
        {
            goto transfer_failed;
        }
        data[index] = value;
        if(accumulate != 0u)
        {
            pass->hash = (pass->hash ^ value) * H4V1_STAGE2_FNV_PRIME;
            if(value == 0xFFu)
            {
                pass->ff_count++;
            }
        }
    }
    h4v1_stage2_deselect();
    pass->cache_cycles += h4v1_stage2_cycle_now() - start;
    return 0;

transfer_failed:
    h4v1_stage2_deselect();
    pass->cache_cycles += h4v1_stage2_cycle_now() - start;
    return -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage2_read_pass(h4v1_stage2_pass_t *pass, uint32_t first_row)
{
    uint8_t *scratch = (uint8_t *)(uintptr_t)H4V1_STAGE2_SCRATCH_ADDR;
    uint32_t page;
    uint32_t start = h4v1_stage2_cycle_now();

    pass->hash = H4V1_STAGE2_FNV_OFFSET;
    for(page = 0u; page < H4V1_STAGE2_PAGES; ++page)
    {
        uint8_t status = 0xFFu;

        int page_result = h4v1_stage2_load_and_classify(
            pass, first_row + page, &status);

        if(page_result != 0)
        {
            return page_result;
        }
        if(h4v1_stage2_read_cache(pass,
                                  0u,
                                  scratch,
                                  H4V1_STAGE2_PAGE_BYTES,
                                  1u) != 0)
        {
            return -1;
        }
    }
    pass->total_cycles = h4v1_stage2_cycle_now() - start;
    return 0;
}

uint32_t H4V1_POSTPASS_TEXT h4v1_stage2_kib_per_second(uint32_t cycles)
{
    uint32_t bytes = H4V1_STAGE2_PAGE_BYTES * H4V1_STAGE2_PAGES;
    uint32_t core_kib = H4V1_STAGE2_CORE_HZ / 1024u;

    return (cycles == 0u) ? 0u : (bytes * core_kib) / cycles;
}

int H4V1_POSTPASS_ENTRY h4v1_flash_postpass_stage2_run(void)
{
    h4v1_stage2_pass_t probe = {0};
    h4v1_stage2_pass_t first = {0};
    h4v1_stage2_pass_t second = {0};
    uint8_t *scratch = (uint8_t *)(uintptr_t)H4V1_STAGE2_SCRATCH_ADDR;
    uint8_t marker = 0u;
    uint8_t marker_status = 0xFFu;
    uint8_t config = 0xFFu;
    uint32_t block;
    uint32_t selected_block = 0xFFFFFFFFu;
    uint32_t selected_row = 0xFFFFFFFFu;
    uint32_t scanned_blocks = 0u;
    uint32_t ecc_clean_total;
    uint32_t ecc_corrected_total;
    uint32_t ecc_uncorrectable_total;
    uint32_t ecc_worst_total;
    int first_result = -1;
    int second_result = -1;
    int result = H4V1_FLASH_POSTPASS_STAGE2_OK;
    char line[448];

    h4v1_postpass_stage1_log(h4v1_postpass_stage2_start_text);

    if(h4v1_stage2_get_feature(&probe,
                               H4V1_STAGE2_FEATURE_CONFIG,
                               &config) != 0)
    {
        result = H4V1_FLASH_POSTPASS_STAGE2_ERR_TIMEOUT;
    }
    else if((config & H4V1_STAGE2_CONFIG_ECC_ENABLE) == 0u)
    {
        result = H4V1_FLASH_POSTPASS_STAGE2_ERR_ECC_DISABLED;
    }

    for(block = H4V1_STAGE2_FIRST_BLOCK;
        (result == H4V1_FLASH_POSTPASS_STAGE2_OK) &&
        (block <= H4V1_STAGE2_LAST_BLOCK);
        ++block)
    {
        int marker_page_result;
        uint32_t row = block * H4V1_STAGE2_PAGES_PER_BLOCK;

        scanned_blocks++;
        marker_page_result = h4v1_stage2_load_and_classify(
            &probe, row, &marker_status);
        if(marker_page_result == -1)
        {
            result = H4V1_FLASH_POSTPASS_STAGE2_ERR_TIMEOUT;
            break;
        }
        if(marker_page_result == -2)
        {
            /* An uncorrectable marker page cannot establish a good block. */
            continue;
        }
        if(h4v1_stage2_read_cache(&probe,
                                  H4V1_STAGE2_BAD_MARK_COLUMN,
                                  scratch,
                                  1u,
                                  0u) != 0)
        {
            result = H4V1_FLASH_POSTPASS_STAGE2_ERR_TIMEOUT;
            break;
        }
        marker = scratch[0];
        if(marker == 0xFFu)
        {
            selected_block = block;
            selected_row = row;
            break;
        }
    }

    if((result == H4V1_FLASH_POSTPASS_STAGE2_OK) &&
       (selected_block == 0xFFFFFFFFu))
    {
        result = H4V1_FLASH_POSTPASS_STAGE2_ERR_NO_GOOD_BLOCK;
    }

    if(result == H4V1_FLASH_POSTPASS_STAGE2_OK)
    {
        first_result = h4v1_stage2_read_pass(&first, selected_row);
        if(first_result == 0)
        {
            second_result = h4v1_stage2_read_pass(&second, selected_row);
        }
    }

    h4v1_stage2_deselect();
    if((result == H4V1_FLASH_POSTPASS_STAGE2_OK) &&
       ((probe.spi_timeout_count != 0u) ||
        (first.spi_timeout_count != 0u) ||
        (second.spi_timeout_count != 0u) ||
        (probe.ready_timeout_count != 0u) ||
        (first.ready_timeout_count != 0u) ||
        (second.ready_timeout_count != 0u) ||
        (first_result == -1) ||
        ((first_result == 0) && (second_result == -1))))
    {
        result = H4V1_FLASH_POSTPASS_STAGE2_ERR_TIMEOUT;
    }
    else if((result == H4V1_FLASH_POSTPASS_STAGE2_OK) &&
            ((first_result == -2) || (second_result == -2)))
    {
        result = H4V1_FLASH_POSTPASS_STAGE2_ERR_UNCORRECTABLE;
    }
    else if((result == H4V1_FLASH_POSTPASS_STAGE2_OK) &&
            ((first.hash != second.hash) ||
             (first.ff_count != second.ff_count)))
    {
        result = H4V1_FLASH_POSTPASS_STAGE2_ERR_UNSTABLE;
    }

    ecc_clean_total = first.ecc_clean_pages + second.ecc_clean_pages;
    ecc_corrected_total = first.ecc_corrected_pages +
                          second.ecc_corrected_pages;
    ecc_uncorrectable_total = first.ecc_uncorrectable_pages +
                              second.ecc_uncorrectable_pages;
    ecc_worst_total = (first.ecc_worst_bits > second.ecc_worst_bits) ?
        first.ecc_worst_bits : second.ecc_worst_bits;

    if(result == H4V1_FLASH_POSTPASS_STAGE2_OK)
    {
        (void)rt_snprintf(line, sizeof(line),
                          h4v1_postpass_stage2_pass_format,
                          (unsigned int)selected_block,
                          (unsigned int)selected_row,
                          (unsigned int)scanned_blocks,
                          (unsigned int)marker,
                          (unsigned int)config,
                          (unsigned int)first.hash,
                          (unsigned int)second.hash,
                          (unsigned int)first.ff_count,
                          (unsigned int)second.ff_count,
                          (unsigned int)first.status_last,
                          (unsigned int)second.status_last,
                          (unsigned int)first.status2_last,
                          (unsigned int)second.status2_last,
                          (unsigned int)ecc_clean_total,
                          (unsigned int)ecc_corrected_total,
                          (unsigned int)ecc_uncorrectable_total,
                          (unsigned int)ecc_worst_total,
                          (unsigned int)probe.ecc_clean_pages,
                          (unsigned int)probe.ecc_corrected_pages,
                          (unsigned int)probe.ecc_uncorrectable_pages,
                          (unsigned int)first.ready_polls,
                          (unsigned int)second.ready_polls,
                          (unsigned int)first.total_cycles,
                          (unsigned int)second.total_cycles,
                          (unsigned int)first.cache_cycles,
                          (unsigned int)second.cache_cycles,
                          (unsigned int)h4v1_stage2_kib_per_second(
                              second.total_cycles),
                          (unsigned int)h4v1_stage2_kib_per_second(
                              second.cache_cycles),
                          (unsigned int)(probe.spi_timeout_count +
                                         first.spi_timeout_count +
                                         second.spi_timeout_count));
    }
    else
    {
        (void)rt_snprintf(line, sizeof(line),
                          h4v1_postpass_stage2_fail_format,
                          result,
                          (unsigned int)selected_block,
                          (unsigned int)scanned_blocks,
                          (unsigned int)marker,
                          (unsigned int)config,
                          (unsigned int)first.hash,
                          (unsigned int)second.hash,
                          (unsigned int)first.status_last,
                          (unsigned int)second.status_last,
                          (unsigned int)first.status2_last,
                          (unsigned int)second.status2_last,
                          (unsigned int)ecc_clean_total,
                          (unsigned int)ecc_corrected_total,
                          (unsigned int)ecc_uncorrectable_total,
                          (unsigned int)ecc_worst_total,
                          (unsigned int)probe.ecc_clean_pages,
                          (unsigned int)probe.ecc_corrected_pages,
                          (unsigned int)probe.ecc_uncorrectable_pages,
                          (unsigned int)(probe.spi_timeout_count +
                                         first.spi_timeout_count +
                                         second.spi_timeout_count),
                          (unsigned int)(probe.ready_timeout_count +
                                         first.ready_timeout_count +
                                         second.ready_timeout_count));
    }
    h4v1_postpass_stage1_log(line);
    return result;
}
