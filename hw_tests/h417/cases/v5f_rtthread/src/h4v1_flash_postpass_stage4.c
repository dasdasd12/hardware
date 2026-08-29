/*
 * H4V1 Flash Stage 4: isolated, read-only NAND array-to-SRAM E2E sweep.
 *
 * Stage 2 established that block 768 is factory-good and blank on this test
 * board.  Stage 3 established the 50 MHz normal-receiver SPI/DMA path.  This
 * stage measures the complete per-page service path: PAGE READ, OIP/ECC
 * polling and a 2 KiB DMA READ FROM CACHE.  It never resets, configures,
 * erases or programs the NAND and never touches DMA2, SDRAM, LTDC or USB.
 */
#include "h4v1_flash_postpass_stage4.h"

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

#define H4V1_STAGE4_CMD_GET_FEATURE       0x0Fu
#define H4V1_STAGE4_CMD_PAGE_READ         0x13u
#define H4V1_STAGE4_CMD_READ_CACHE        0x03u
#define H4V1_STAGE4_CMD_READ_ID           0x9Fu
#define H4V1_STAGE4_FEATURE_CONFIG        0xB0u
#define H4V1_STAGE4_FEATURE_STATUS        0xC0u
#define H4V1_STAGE4_FEATURE_STATUS2       0xF0u
#define H4V1_STAGE4_EXPECTED_MID           0xC8u
#define H4V1_STAGE4_EXPECTED_DID           0x91u
#define H4V1_STAGE4_CONFIG_ECC_ENABLE      0x10u
#define H4V1_STAGE4_STATUS_OIP             0x01u
#define H4V1_STAGE4_STATUS_ECC_MASK        0x30u
#define H4V1_STAGE4_STATUS_ECC_CLEAN       0x00u
#define H4V1_STAGE4_STATUS_ECC_CORRECTED   0x10u
#define H4V1_STAGE4_STATUS_ECC_FAILED      0x20u
#define H4V1_STAGE4_STATUS_ECC_8_BITS      0x30u
#define H4V1_STAGE4_STATUS2_ECCSE_MASK     0x30u

#define H4V1_STAGE4_SPI_SPE                0x0040u
#define H4V1_STAGE4_SPI_BR_MASK            0x0038u
#define H4V1_STAGE4_SPI_RXNE               0x0001u
#define H4V1_STAGE4_SPI_TXE                0x0002u
#define H4V1_STAGE4_SPI_BSY                0x0080u
#define H4V1_STAGE4_SPI_RXDMAEN            0x0001u
#define H4V1_STAGE4_SPI_TXDMAEN            0x0002u
#define H4V1_STAGE4_SPI_HSRXEN             0x0001u
#define H4V1_STAGE4_SPI_HSRXEN2            0x0004u

#define H4V1_STAGE4_DMA_EN                 0x0001u
#define H4V1_STAGE4_DMA_DIR                0x0010u
#define H4V1_STAGE4_DMA_MINC               0x0080u
#define H4V1_STAGE4_DMA_PRIORITY_VERY_HIGH 0x3000u
#define H4V1_STAGE4_DMA_GL2                0x00000010u
#define H4V1_STAGE4_DMA_TC2                0x00000020u
#define H4V1_STAGE4_DMA_TE2                0x00000080u
#define H4V1_STAGE4_DMA_GL3                0x00000100u
#define H4V1_STAGE4_DMA_TC3                0x00000200u
#define H4V1_STAGE4_DMA_TE3                0x00000800u
#define H4V1_STAGE4_DMA_DONE_MASK          \
    (H4V1_STAGE4_DMA_TC2 | H4V1_STAGE4_DMA_TC3)
#define H4V1_STAGE4_DMA_ERROR_MASK         \
    (H4V1_STAGE4_DMA_TE2 | H4V1_STAGE4_DMA_TE3)

#define H4V1_STAGE4_DMAMUX_CH2_SHIFT       8u
#define H4V1_STAGE4_DMAMUX_CH3_SHIFT       16u
#define H4V1_STAGE4_DMAMUX_FIELD_MASK      0x7Fu
#define H4V1_STAGE4_DMA_REQUEST_SPI1_TX    63u
#define H4V1_STAGE4_DMA_REQUEST_SPI1_RX    64u
#define H4V1_STAGE4_RCC_DMA1               0x00000001u

#define H4V1_STAGE4_SPI_BYTE_TIMEOUT       1000000u
#define H4V1_STAGE4_OPERATION_TIMEOUT      4000000u
#define H4V1_STAGE4_CORE_HZ                400000000u
#define H4V1_STAGE4_BLOCK                  768u
#define H4V1_STAGE4_PAGES_PER_BLOCK        64u
#define H4V1_STAGE4_REPEATS                8u
#define H4V1_STAGE4_PAGE_BYTES             2048u
#define H4V1_STAGE4_REPEAT_BYTES           \
    (H4V1_STAGE4_PAGES_PER_BLOCK * H4V1_STAGE4_PAGE_BYTES)
#define H4V1_STAGE4_TOTAL_BYTES            \
    (H4V1_STAGE4_REPEAT_BYTES * H4V1_STAGE4_REPEATS)
#define H4V1_STAGE4_FIRST_ROW              \
    (H4V1_STAGE4_BLOCK * H4V1_STAGE4_PAGES_PER_BLOCK)
#define H4V1_STAGE4_LAST_ROW               \
    (H4V1_STAGE4_FIRST_ROW + H4V1_STAGE4_PAGES_PER_BLOCK - 1u)
#define H4V1_STAGE4_BAD_MARK_COLUMN        2048u
#define H4V1_STAGE4_PROBE_REPEATS          8u
#define H4V1_STAGE4_RX_ADDR                0x20174000u
#define H4V1_STAGE4_TX_ZERO_ADDR           0x20174800u
#define H4V1_STAGE4_FNV_OFFSET             2166136261u
#define H4V1_STAGE4_FNV_PRIME              16777619u

typedef struct
{
    uint32_t spi_timeout_count;
    uint32_t ready_timeout_count;
    uint32_t dma_timeout_count;
    uint32_t dma_error_count;
    uint32_t ready_polls;
    uint32_t ready_polls_max;
    uint32_t total_cycles;
    uint32_t array_cycles;
    uint32_t cache_cycles;
    uint32_t wall_cycles;
    uint32_t array_min_cycles;
    uint32_t array_max_cycles;
    uint32_t processed_pages;
    uint32_t ff_count;
    uint32_t ecc_clean_pages;
    uint32_t ecc_corrected_pages;
    uint32_t ecc_uncorrectable_pages;
    uint32_t first_corrected_row;
    uint32_t first_uncorrectable_row;
    uint32_t first_bad_row;
    uint32_t first_bad_offset;
    uint8_t mid;
    uint8_t did;
    uint8_t config;
    uint8_t marker;
    uint8_t status_last;
    uint8_t status2_last;
    uint8_t ecc_worst_bits;
    uint8_t first_bad_value;
} h4v1_stage4_result_t;

typedef struct
{
    uint32_t dma_clock_was_enabled;
    uint32_t dma2_cfgr;
    uint32_t dma2_cntr;
    uint32_t dma2_paddr;
    uint32_t dma2_maddr;
    uint32_t dma2_m1addr;
    uint32_t dma3_cfgr;
    uint32_t dma3_cntr;
    uint32_t dma3_paddr;
    uint32_t dma3_maddr;
    uint32_t dma3_m1addr;
    uint32_t dmamux_cfgr0_3;
    uint16_t spi_ctlr1;
    uint16_t spi_ctlr2;
    uint16_t spi_hscr;
} h4v1_stage4_saved_t;

extern void h4v1_postpass_stage1_log(const char *text);

const char h4v1_stage4_start_text[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE4 START access=array_e2e read_only=1 block=768 "
    "rows=49152..49215 pages=64 repeats=8 bytes=1048576 "
    "sck_khz=50000 hsrx=0 sequence=13+oip+ecc+03_dma2048 "
    "scratch=20174000 dma=DMA1_CH2_RX64+CH3_TX63 "
    "no_reset=1 no_set_feature=1 no_write=1 no_dma2=1 no_sdram=1";
const char h4v1_stage4_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE4 PASS block=%u rows=%u..%u pages=%u repeats=%u "
    "events=%u bytes=%u marker=%02x id=%02x%02x b0=%02x "
    "hash=%08x ff=%u/%u e2e_KiBps=%u cache_KiBps=%u wall_KiBps=%u "
    "array_us_x10=%u/%u/%u polls=%u/%u ecc=c%u/r%u/u%u "
    "worst_upper=%u first_corr=%08x cycles=%u/%u/%u/%u "
    "spi_timeout=%u ready_timeout=%u dma_timeout=%u dma_error=%u "
    "scope=array_to_shared_sram_e2e";
const char h4v1_stage4_fail_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE4 FAIL result=%d block=%u row=%08x processed=%u/%u "
    "marker=%02x id=%02x%02x b0=%02x status=%02x f0=%02x "
    "ecc=c%u/r%u/u%u worst_upper=%u first_unc=%08x "
    "bad=%08x:%08x/%02x spi_timeout=%u ready_timeout=%u "
    "dma_timeout=%u dma_error=%u restore=%u";

uint32_t H4V1_POSTPASS_TEXT h4v1_stage4_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

void H4V1_POSTPASS_TEXT h4v1_stage4_fence(void)
{
    __asm volatile ("fence iorw, iorw" ::: "memory");
}

void H4V1_POSTPASS_TEXT h4v1_stage4_select(void)
{
    GPIOF->BCR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage4_deselect(void)
{
    GPIOF->BSHR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage4_drain_rx(void)
{
    uint32_t guard = 16u;

    while((guard != 0u) && ((SPI1->STATR & H4V1_STAGE4_SPI_RXNE) != 0u))
    {
        (void)SPI1->DATAR;
        guard--;
    }
    (void)SPI1->STATR;
}

uint8_t H4V1_POSTPASS_TEXT
h4v1_stage4_transfer(h4v1_stage4_result_t *result, uint8_t tx)
{
    uint32_t timeout = H4V1_STAGE4_SPI_BYTE_TIMEOUT;

    while(((SPI1->STATR & H4V1_STAGE4_SPI_TXE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        result->spi_timeout_count++;
        return 0xFFu;
    }
    SPI1->DATAR = tx;

    timeout = H4V1_STAGE4_SPI_BYTE_TIMEOUT;
    while(((SPI1->STATR & H4V1_STAGE4_SPI_RXNE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        result->spi_timeout_count++;
        return 0xFFu;
    }
    return (uint8_t)SPI1->DATAR;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_send(h4v1_stage4_result_t *result, uint8_t value,
                 uint32_t timeout_before)
{
    (void)h4v1_stage4_transfer(result, value);
    return (result->spi_timeout_count == timeout_before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_get_feature(h4v1_stage4_result_t *result,
                        uint8_t address, uint8_t *value)
{
    uint32_t before = result->spi_timeout_count;

    h4v1_stage4_select();
    if(h4v1_stage4_send(result, H4V1_STAGE4_CMD_GET_FEATURE, before) != 0 ||
       h4v1_stage4_send(result, address, before) != 0)
    {
        h4v1_stage4_deselect();
        return -1;
    }
    *value = h4v1_stage4_transfer(result, 0x00u);
    h4v1_stage4_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT h4v1_stage4_read_id(h4v1_stage4_result_t *result)
{
    uint32_t before = result->spi_timeout_count;

    h4v1_stage4_select();
    if(h4v1_stage4_send(result, H4V1_STAGE4_CMD_READ_ID, before) != 0 ||
       h4v1_stage4_send(result, 0x00u, before) != 0)
    {
        h4v1_stage4_deselect();
        return -1;
    }
    result->mid = h4v1_stage4_transfer(result, 0x00u);
    if(result->spi_timeout_count != before)
    {
        h4v1_stage4_deselect();
        return -1;
    }
    result->did = h4v1_stage4_transfer(result, 0x00u);
    h4v1_stage4_deselect();
    if(result->spi_timeout_count != before)
    {
        return -1;
    }
    return ((result->mid == H4V1_STAGE4_EXPECTED_MID) &&
            (result->did == H4V1_STAGE4_EXPECTED_DID)) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT h4v1_stage4_probe(h4v1_stage4_result_t *result)
{
    uint32_t repeat;

    for(repeat = 0u; repeat < H4V1_STAGE4_PROBE_REPEATS; ++repeat)
    {
        uint8_t config = 0xFFu;

        if(h4v1_stage4_read_id(result) != 0)
        {
            return -1;
        }
        if(h4v1_stage4_get_feature(result,
                                    H4V1_STAGE4_FEATURE_CONFIG,
                                    &config) != 0)
        {
            return -1;
        }
        result->config = config;
        if((config & H4V1_STAGE4_CONFIG_ECC_ENABLE) == 0u)
        {
            return -2;
        }
    }
    return 0;
}

void H4V1_POSTPASS_TEXT h4v1_stage4_apply_50mhz(void)
{
    uint16_t ctlr1 = SPI1->CTLR1;
    uint16_t hscr = SPI1->HSCR;

    SPI1->CTLR1 = ctlr1 & (uint16_t)~H4V1_STAGE4_SPI_SPE;
    h4v1_stage4_drain_rx();
    hscr &= (uint16_t)~(H4V1_STAGE4_SPI_HSRXEN |
                        H4V1_STAGE4_SPI_HSRXEN2);
    SPI1->HSCR = hscr;
    ctlr1 &= (uint16_t)~H4V1_STAGE4_SPI_BR_MASK;
    SPI1->CTLR1 = ctlr1 | H4V1_STAGE4_SPI_SPE;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_wait_ready(h4v1_stage4_result_t *result,
                       uint8_t *status_out, uint32_t *polls_out)
{
    uint32_t start = h4v1_stage4_cycle_now();
    uint32_t polls = 0u;
    uint8_t status = 0xFFu;

    do
    {
        if(h4v1_stage4_get_feature(result,
                                   H4V1_STAGE4_FEATURE_STATUS,
                                   &status) != 0)
        {
            *status_out = status;
            *polls_out = polls;
            return -1;
        }
        polls++;
        if((status & H4V1_STAGE4_STATUS_OIP) == 0u)
        {
            *status_out = status;
            *polls_out = polls;
            return 0;
        }
    } while((uint32_t)(h4v1_stage4_cycle_now() - start) <
            H4V1_STAGE4_OPERATION_TIMEOUT);

    result->ready_timeout_count++;
    *status_out = status;
    *polls_out = polls;
    return -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_load_page(h4v1_stage4_result_t *result,
                       uint32_t row, uint8_t *status_out,
                       uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t polls = 0u;
    uint32_t start = h4v1_stage4_cycle_now();
    int ready_result;

    h4v1_stage4_select();
    if(h4v1_stage4_send(result, H4V1_STAGE4_CMD_PAGE_READ, before) != 0 ||
       h4v1_stage4_send(result, (uint8_t)(row >> 16), before) != 0 ||
       h4v1_stage4_send(result, (uint8_t)(row >> 8), before) != 0 ||
       h4v1_stage4_send(result, (uint8_t)row, before) != 0)
    {
        h4v1_stage4_deselect();
        *cycles_out = h4v1_stage4_cycle_now() - start;
        return -1;
    }
    h4v1_stage4_deselect();
    ready_result = h4v1_stage4_wait_ready(result, status_out, &polls);
    *cycles_out = h4v1_stage4_cycle_now() - start;
    result->ready_polls += polls;
    if(polls > result->ready_polls_max)
    {
        result->ready_polls_max = polls;
    }
    return ready_result;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_classify_ecc(h4v1_stage4_result_t *result,
                          uint32_t row, uint8_t status,
                          uint8_t count_event)
{
    uint8_t ecc = status & H4V1_STAGE4_STATUS_ECC_MASK;
    uint8_t corrected_bits = 0u;

    result->status_last = status;
    if(ecc == H4V1_STAGE4_STATUS_ECC_CLEAN)
    {
        if(count_event != 0u)
        {
            result->ecc_clean_pages++;
        }
        return 0;
    }
    if(ecc == H4V1_STAGE4_STATUS_ECC_FAILED)
    {
        if(count_event != 0u)
        {
            result->ecc_uncorrectable_pages++;
            if(result->first_uncorrectable_row == 0xFFFFFFFFu)
            {
                result->first_uncorrectable_row = row;
            }
        }
        result->ecc_worst_bits = 9u;
        return -2;
    }

    if(count_event != 0u)
    {
        result->ecc_corrected_pages++;
        if(result->first_corrected_row == 0xFFFFFFFFu)
        {
            result->first_corrected_row = row;
        }
    }
    if(ecc == H4V1_STAGE4_STATUS_ECC_8_BITS)
    {
        corrected_bits = 8u;
    }
    else
    {
        uint8_t status2 = 0xFFu;

        if(h4v1_stage4_get_feature(result,
                                    H4V1_STAGE4_FEATURE_STATUS2,
                                    &status2) != 0)
        {
            return -1;
        }
        result->status2_last = status2;
        switch(status2 & H4V1_STAGE4_STATUS2_ECCSE_MASK)
        {
        case 0x00u:
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
    if(corrected_bits > result->ecc_worst_bits)
    {
        result->ecc_worst_bits = corrected_bits;
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_read_cache_cpu(h4v1_stage4_result_t *result,
                            uint16_t column, uint8_t *data,
                            uint32_t length)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t index;

    h4v1_stage4_select();
    if(h4v1_stage4_send(result, H4V1_STAGE4_CMD_READ_CACHE, before) != 0 ||
       h4v1_stage4_send(result, (uint8_t)(column >> 8), before) != 0 ||
       h4v1_stage4_send(result, (uint8_t)column, before) != 0 ||
       h4v1_stage4_send(result, 0x00u, before) != 0)
    {
        h4v1_stage4_deselect();
        return -1;
    }
    for(index = 0u; index < length; ++index)
    {
        data[index] = h4v1_stage4_transfer(result, 0x00u);
        if(result->spi_timeout_count != before)
        {
            h4v1_stage4_deselect();
            return -1;
        }
    }
    h4v1_stage4_deselect();
    return 0;
}

void H4V1_POSTPASS_TEXT h4v1_stage4_dma_stop(void)
{
    DMA1_Channel3->CFGR &= ~H4V1_STAGE4_DMA_EN;
    DMA1_Channel2->CFGR &= ~H4V1_STAGE4_DMA_EN;
    SPI1->CTLR2 &= (uint16_t)~(H4V1_STAGE4_SPI_RXDMAEN |
                                H4V1_STAGE4_SPI_TXDMAEN);
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_dma_stop_wait(h4v1_stage4_result_t *result)
{
    uint32_t start = h4v1_stage4_cycle_now();

    h4v1_stage4_dma_stop();
    while(((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) &
           H4V1_STAGE4_DMA_EN) != 0u)
    {
        if((uint32_t)(h4v1_stage4_cycle_now() - start) >=
           H4V1_STAGE4_OPERATION_TIMEOUT)
        {
            result->dma_timeout_count++;
            return -1;
        }
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_read_cache_dma(h4v1_stage4_result_t *result,
                            uint8_t *data, uint32_t length,
                            uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t start = h4v1_stage4_cycle_now();

    if(h4v1_stage4_dma_stop_wait(result) != 0)
    {
        h4v1_stage4_deselect();
        *cycles_out = h4v1_stage4_cycle_now() - start;
        return -2;
    }
    h4v1_stage4_drain_rx();
    DMA1_Channel2->MADDR = (uint32_t)(uintptr_t)data;
    DMA1_Channel2->CNTR = length;
    DMA1_Channel3->MADDR = H4V1_STAGE4_TX_ZERO_ADDR;
    DMA1_Channel3->CNTR = length;
    DMA1->INTFCR = H4V1_STAGE4_DMA_GL2 | H4V1_STAGE4_DMA_GL3;
    start = h4v1_stage4_cycle_now();

    h4v1_stage4_select();
    if(h4v1_stage4_send(result, H4V1_STAGE4_CMD_READ_CACHE, before) != 0 ||
       h4v1_stage4_send(result, 0x00u, before) != 0 ||
       h4v1_stage4_send(result, 0x00u, before) != 0 ||
       h4v1_stage4_send(result, 0x00u, before) != 0)
    {
        goto spi_failed;
    }

    SPI1->CTLR2 |= H4V1_STAGE4_SPI_RXDMAEN | H4V1_STAGE4_SPI_TXDMAEN;
    h4v1_stage4_fence();
    DMA1_Channel2->CFGR |= H4V1_STAGE4_DMA_EN;
    DMA1_Channel3->CFGR |= H4V1_STAGE4_DMA_EN;

    while((DMA1->INTFR & H4V1_STAGE4_DMA_DONE_MASK) !=
          H4V1_STAGE4_DMA_DONE_MASK)
    {
        if((DMA1->INTFR & H4V1_STAGE4_DMA_ERROR_MASK) != 0u)
        {
            result->dma_error_count++;
            goto dma_failed;
        }
        if((uint32_t)(h4v1_stage4_cycle_now() - start) >=
           H4V1_STAGE4_OPERATION_TIMEOUT)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    if((DMA1->INTFR & H4V1_STAGE4_DMA_ERROR_MASK) != 0u)
    {
        result->dma_error_count++;
        goto dma_failed;
    }
    while((SPI1->STATR & H4V1_STAGE4_SPI_BSY) != 0u)
    {
        if((uint32_t)(h4v1_stage4_cycle_now() - start) >=
           H4V1_STAGE4_OPERATION_TIMEOUT)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    if((DMA1->INTFR & H4V1_STAGE4_DMA_ERROR_MASK) != 0u)
    {
        result->dma_error_count++;
        goto dma_failed;
    }
    if(h4v1_stage4_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
    h4v1_stage4_deselect();
    DMA1->INTFCR = H4V1_STAGE4_DMA_GL2 | H4V1_STAGE4_DMA_GL3;
    h4v1_stage4_fence();
    *cycles_out = h4v1_stage4_cycle_now() - start;
    return 0;

dma_failed:
    if(h4v1_stage4_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
spi_failed:
    h4v1_stage4_deselect();
    DMA1->INTFCR = H4V1_STAGE4_DMA_GL2 | H4V1_STAGE4_DMA_GL3;
    *cycles_out = h4v1_stage4_cycle_now() - start;
    return -1;

dma_stuck:
    h4v1_stage4_deselect();
    DMA1->INTFCR = H4V1_STAGE4_DMA_GL2 | H4V1_STAGE4_DMA_GL3;
    *cycles_out = h4v1_stage4_cycle_now() - start;
    return -2;
}

void H4V1_POSTPASS_TEXT
h4v1_stage4_poison(uint8_t *data, uint32_t length, uint8_t value)
{
    uint32_t index;

    for(index = 0u; index < length; ++index)
    {
        *(volatile uint8_t *)&data[index] = value;
    }
    h4v1_stage4_fence();
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_validate(h4v1_stage4_result_t *result,
                      const uint8_t *data, uint32_t length,
                      uint32_t row, uint32_t *hash,
                      uint32_t *ff_count)
{
    uint32_t index;

    for(index = 0u; index < length; ++index)
    {
        uint8_t value = *(volatile const uint8_t *)&data[index];

        *hash = (*hash ^ value) * H4V1_STAGE4_FNV_PRIME;
        if(value == 0xFFu)
        {
            (*ff_count)++;
            result->ff_count++;
        }
        else
        {
            result->first_bad_row = row;
            result->first_bad_offset = index;
            result->first_bad_value = value;
            return -1;
        }
    }
    return 0;
}

uint32_t H4V1_POSTPASS_TEXT
h4v1_stage4_kib_per_second(uint32_t bytes, uint32_t cycles)
{
    uint32_t elapsed_kcycles = (cycles + 500u) / 1000u;
    uint32_t sample_kib = bytes / 1024u;

    return (elapsed_kcycles == 0u) ? 0u :
        (sample_kib * (H4V1_STAGE4_CORE_HZ / 1000u)) / elapsed_kcycles;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_save_and_configure(h4v1_stage4_saved_t *saved)
{
    saved->dma_clock_was_enabled = RCC->HBPCENR & H4V1_STAGE4_RCC_DMA1;
    RCC->HBPCENR |= H4V1_STAGE4_RCC_DMA1;
    h4v1_stage4_fence();

    saved->dma2_cfgr = DMA1_Channel2->CFGR;
    saved->dma2_cntr = DMA1_Channel2->CNTR;
    saved->dma2_paddr = DMA1_Channel2->PADDR;
    saved->dma2_maddr = DMA1_Channel2->MADDR;
    saved->dma2_m1addr = DMA1_Channel2->M1ADDR;
    saved->dma3_cfgr = DMA1_Channel3->CFGR;
    saved->dma3_cntr = DMA1_Channel3->CNTR;
    saved->dma3_paddr = DMA1_Channel3->PADDR;
    saved->dma3_maddr = DMA1_Channel3->MADDR;
    saved->dma3_m1addr = DMA1_Channel3->M1ADDR;
    saved->dmamux_cfgr0_3 = DMAMUX->CFGR0_3;
    saved->spi_ctlr1 = SPI1->CTLR1;
    saved->spi_ctlr2 = SPI1->CTLR2;
    saved->spi_hscr = SPI1->HSCR;

    if(((saved->dma2_cfgr | saved->dma3_cfgr) &
        H4V1_STAGE4_DMA_EN) != 0u)
    {
        return -1;
    }

    h4v1_stage4_dma_stop();
    DMA1_Channel2->CFGR = H4V1_STAGE4_DMA_PRIORITY_VERY_HIGH |
                              H4V1_STAGE4_DMA_MINC;
    DMA1_Channel2->CNTR = 0u;
    DMA1_Channel2->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel2->MADDR = H4V1_STAGE4_RX_ADDR;
    DMA1_Channel2->M1ADDR = 0u;
    DMA1_Channel3->CFGR = H4V1_STAGE4_DMA_PRIORITY_VERY_HIGH |
                              H4V1_STAGE4_DMA_DIR;
    DMA1_Channel3->CNTR = 0u;
    DMA1_Channel3->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel3->MADDR = H4V1_STAGE4_TX_ZERO_ADDR;
    DMA1_Channel3->M1ADDR = 0u;
    DMAMUX->CFGR0_3 =
        (saved->dmamux_cfgr0_3 &
         ~((H4V1_STAGE4_DMAMUX_FIELD_MASK << H4V1_STAGE4_DMAMUX_CH2_SHIFT) |
           (H4V1_STAGE4_DMAMUX_FIELD_MASK << H4V1_STAGE4_DMAMUX_CH3_SHIFT))) |
        ((H4V1_STAGE4_DMA_REQUEST_SPI1_RX - 1u) <<
         H4V1_STAGE4_DMAMUX_CH2_SHIFT) |
        ((H4V1_STAGE4_DMA_REQUEST_SPI1_TX - 1u) <<
         H4V1_STAGE4_DMAMUX_CH3_SHIFT);
    *(volatile uint8_t *)(uintptr_t)H4V1_STAGE4_TX_ZERO_ADDR = 0u;
    DMA1->INTFCR = H4V1_STAGE4_DMA_GL2 | H4V1_STAGE4_DMA_GL3;
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage4_restore(const h4v1_stage4_saved_t *saved)
{
    h4v1_stage4_result_t stop_result = {0};

    if(((saved->dma2_cfgr | saved->dma3_cfgr) &
        H4V1_STAGE4_DMA_EN) != 0u)
    {
        if(saved->dma_clock_was_enabled == 0u)
        {
            RCC->HBPCENR &= ~H4V1_STAGE4_RCC_DMA1;
        }
        return 0;
    }
    if(h4v1_stage4_dma_stop_wait(&stop_result) != 0)
    {
        h4v1_stage4_deselect();
        return -1;
    }
    h4v1_stage4_deselect();
    DMA1->INTFCR = H4V1_STAGE4_DMA_GL2 | H4V1_STAGE4_DMA_GL3;

    DMA1_Channel2->CFGR = saved->dma2_cfgr & ~H4V1_STAGE4_DMA_EN;
    DMA1_Channel2->CNTR = saved->dma2_cntr;
    DMA1_Channel2->PADDR = saved->dma2_paddr;
    DMA1_Channel2->MADDR = saved->dma2_maddr;
    DMA1_Channel2->M1ADDR = saved->dma2_m1addr;
    DMA1_Channel3->CFGR = saved->dma3_cfgr & ~H4V1_STAGE4_DMA_EN;
    DMA1_Channel3->CNTR = saved->dma3_cntr;
    DMA1_Channel3->PADDR = saved->dma3_paddr;
    DMA1_Channel3->MADDR = saved->dma3_maddr;
    DMA1_Channel3->M1ADDR = saved->dma3_m1addr;
    DMAMUX->CFGR0_3 = saved->dmamux_cfgr0_3;

    SPI1->CTLR1 &= (uint16_t)~H4V1_STAGE4_SPI_SPE;
    h4v1_stage4_drain_rx();
    SPI1->HSCR = saved->spi_hscr;
    SPI1->CTLR2 = saved->spi_ctlr2;
    SPI1->CTLR1 = saved->spi_ctlr1;
    if(saved->dma_clock_was_enabled == 0u)
    {
        RCC->HBPCENR &= ~H4V1_STAGE4_RCC_DMA1;
    }
    h4v1_stage4_fence();
    return 0;
}

int H4V1_POSTPASS_ENTRY h4v1_flash_postpass_stage4_run(void)
{
    h4v1_stage4_saved_t saved;
    h4v1_stage4_result_t preflight = {0};
    h4v1_stage4_result_t run = {0};
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE4_RX_ADDR;
    uint32_t reference_hash = 0u;
    uint32_t fail_row = 0xFFFFFFFFu;
    uint32_t repeat;
    uint32_t wall_start = 0u;
    uint32_t restore_failed = 0u;
    int result = H4V1_FLASH_POSTPASS_STAGE4_OK;
    char line[640];

    preflight.first_corrected_row = 0xFFFFFFFFu;
    preflight.first_uncorrectable_row = 0xFFFFFFFFu;
    preflight.first_bad_row = 0xFFFFFFFFu;
    preflight.first_bad_offset = 0xFFFFFFFFu;
    preflight.array_min_cycles = 0xFFFFFFFFu;
    preflight.status2_last = 0xFFu;
    run.first_corrected_row = 0xFFFFFFFFu;
    run.first_uncorrectable_row = 0xFFFFFFFFu;
    run.first_bad_row = 0xFFFFFFFFu;
    run.first_bad_offset = 0xFFFFFFFFu;
    run.array_min_cycles = 0xFFFFFFFFu;
    run.status2_last = 0xFFu;

    h4v1_postpass_stage1_log(h4v1_stage4_start_text);
    if(h4v1_stage4_save_and_configure(&saved) != 0)
    {
        result = H4V1_FLASH_POSTPASS_STAGE4_ERR_DMA_BUSY;
    }
    else
    {
        uint8_t marker_status = 0xFFu;
        uint32_t marker_cycles = 0u;

        h4v1_stage4_apply_50mhz();
        if(h4v1_stage4_probe(&preflight) != 0)
        {
            result = H4V1_FLASH_POSTPASS_STAGE4_ERR_PROBE;
        }
        else if(h4v1_stage4_load_page(&preflight,
                                       H4V1_STAGE4_FIRST_ROW,
                                       &marker_status,
                                       &marker_cycles) == -2)
        {
            result = H4V1_FLASH_POSTPASS_STAGE4_ERR_READY;
        }
        else if(preflight.spi_timeout_count != 0u)
        {
            result = H4V1_FLASH_POSTPASS_STAGE4_ERR_SPI;
        }
        else if(h4v1_stage4_classify_ecc(&preflight,
                                          H4V1_STAGE4_FIRST_ROW,
                                          marker_status, 0u) != 0)
        {
            result = (preflight.spi_timeout_count != 0u) ?
                H4V1_FLASH_POSTPASS_STAGE4_ERR_SPI :
                H4V1_FLASH_POSTPASS_STAGE4_ERR_ECC;
        }
        else if(h4v1_stage4_read_cache_cpu(&preflight,
                                            H4V1_STAGE4_BAD_MARK_COLUMN,
                                            &preflight.marker, 1u) != 0)
        {
            result = H4V1_FLASH_POSTPASS_STAGE4_ERR_SPI;
        }
        else if(preflight.marker != 0xFFu)
        {
            result = H4V1_FLASH_POSTPASS_STAGE4_ERR_MARKER;
        }
    }

    if(result == H4V1_FLASH_POSTPASS_STAGE4_OK)
    {
        wall_start = h4v1_stage4_cycle_now();
        for(repeat = 0u;
            (repeat < H4V1_STAGE4_REPEATS) &&
            (result == H4V1_FLASH_POSTPASS_STAGE4_OK);
            ++repeat)
        {
            uint32_t repeat_hash = H4V1_STAGE4_FNV_OFFSET;
            uint32_t repeat_ff = 0u;
            uint32_t page;

            for(page = 0u;
                (page < H4V1_STAGE4_PAGES_PER_BLOCK) &&
                (result == H4V1_FLASH_POSTPASS_STAGE4_OK);
                ++page)
            {
                uint32_t row = H4V1_STAGE4_FIRST_ROW + page;
                uint32_t page_start;
                uint32_t page_cycles;
                uint32_t array_cycles = 0u;
                uint32_t cache_cycles = 0u;
                uint8_t status = 0xFFu;
                int load_result;
                int ecc_result;
                int dma_result;

                fail_row = row;
                h4v1_stage4_poison(data, H4V1_STAGE4_PAGE_BYTES,
                                    ((repeat + page) & 1u) ? 0xA5u : 0x00u);
                page_start = h4v1_stage4_cycle_now();
                load_result = h4v1_stage4_load_page(&run, row,
                                                     &status, &array_cycles);
                if(load_result != 0)
                {
                    result = (load_result == -2) ?
                        H4V1_FLASH_POSTPASS_STAGE4_ERR_READY :
                        H4V1_FLASH_POSTPASS_STAGE4_ERR_SPI;
                    break;
                }
                run.array_cycles += array_cycles;
                if(array_cycles < run.array_min_cycles)
                {
                    run.array_min_cycles = array_cycles;
                }
                if(array_cycles > run.array_max_cycles)
                {
                    run.array_max_cycles = array_cycles;
                }

                ecc_result = h4v1_stage4_classify_ecc(&run, row, status, 1u);
                if(ecc_result != 0)
                {
                    result = (ecc_result == -2) ?
                        H4V1_FLASH_POSTPASS_STAGE4_ERR_ECC :
                        H4V1_FLASH_POSTPASS_STAGE4_ERR_SPI;
                    break;
                }

                dma_result = h4v1_stage4_read_cache_dma(
                    &run, data, H4V1_STAGE4_PAGE_BYTES, &cache_cycles);
                if(dma_result != 0)
                {
                    if(dma_result == -2)
                    {
                        result = H4V1_FLASH_POSTPASS_STAGE4_ERR_DMA_QUIESCE;
                    }
                    else if(run.spi_timeout_count != 0u)
                    {
                        result = H4V1_FLASH_POSTPASS_STAGE4_ERR_SPI;
                    }
                    else
                    {
                        result = H4V1_FLASH_POSTPASS_STAGE4_ERR_DMA;
                    }
                    break;
                }
                page_cycles = h4v1_stage4_cycle_now() - page_start;
                run.cache_cycles += cache_cycles;
                run.total_cycles += page_cycles;
                run.processed_pages++;

                if(h4v1_stage4_validate(&run, data,
                                         H4V1_STAGE4_PAGE_BYTES,
                                         row, &repeat_hash,
                                         &repeat_ff) != 0)
                {
                    result = H4V1_FLASH_POSTPASS_STAGE4_ERR_DATA;
                    break;
                }
            }
            if(result == H4V1_FLASH_POSTPASS_STAGE4_OK)
            {
                if(repeat_ff != H4V1_STAGE4_REPEAT_BYTES)
                {
                    result = H4V1_FLASH_POSTPASS_STAGE4_ERR_DATA;
                }
                else if(repeat == 0u)
                {
                    reference_hash = repeat_hash;
                }
                else if(repeat_hash != reference_hash)
                {
                    result = H4V1_FLASH_POSTPASS_STAGE4_ERR_UNSTABLE;
                }
            }
        }
        run.wall_cycles = h4v1_stage4_cycle_now() - wall_start;
    }

    if((result == H4V1_FLASH_POSTPASS_STAGE4_OK) &&
       (h4v1_stage4_probe(&preflight) != 0))
    {
        result = H4V1_FLASH_POSTPASS_STAGE4_ERR_PROBE;
    }
    if(h4v1_stage4_restore(&saved) != 0)
    {
        restore_failed = 1u;
        result = H4V1_FLASH_POSTPASS_STAGE4_ERR_RESTORE;
    }

    if(result == H4V1_FLASH_POSTPASS_STAGE4_OK)
    {
        uint32_t events = H4V1_STAGE4_PAGES_PER_BLOCK *
                          H4V1_STAGE4_REPEATS;
        uint32_t average_cycles = run.array_cycles / events;
        uint32_t average_us_x10 = (average_cycles + 20u) / 40u;
        uint32_t minimum_us_x10 = (run.array_min_cycles + 20u) / 40u;
        uint32_t maximum_us_x10 = (run.array_max_cycles + 20u) / 40u;
        uint32_t average_polls_x10 =
            ((run.ready_polls * 10u) + (events / 2u)) / events;

        (void)rt_snprintf(line, sizeof(line), h4v1_stage4_pass_format,
                          (unsigned int)H4V1_STAGE4_BLOCK,
                          (unsigned int)H4V1_STAGE4_FIRST_ROW,
                          (unsigned int)H4V1_STAGE4_LAST_ROW,
                          (unsigned int)H4V1_STAGE4_PAGES_PER_BLOCK,
                          (unsigned int)H4V1_STAGE4_REPEATS,
                          (unsigned int)events,
                          (unsigned int)H4V1_STAGE4_TOTAL_BYTES,
                          (unsigned int)preflight.marker,
                          (unsigned int)preflight.mid,
                          (unsigned int)preflight.did,
                          (unsigned int)preflight.config,
                          (unsigned int)reference_hash,
                          (unsigned int)run.ff_count,
                          (unsigned int)H4V1_STAGE4_TOTAL_BYTES,
                          (unsigned int)h4v1_stage4_kib_per_second(
                              H4V1_STAGE4_TOTAL_BYTES, run.total_cycles),
                          (unsigned int)h4v1_stage4_kib_per_second(
                              H4V1_STAGE4_TOTAL_BYTES, run.cache_cycles),
                          (unsigned int)h4v1_stage4_kib_per_second(
                              H4V1_STAGE4_TOTAL_BYTES, run.wall_cycles),
                          (unsigned int)average_us_x10,
                          (unsigned int)minimum_us_x10,
                          (unsigned int)maximum_us_x10,
                          (unsigned int)average_polls_x10,
                          (unsigned int)run.ready_polls_max,
                          (unsigned int)run.ecc_clean_pages,
                          (unsigned int)run.ecc_corrected_pages,
                          (unsigned int)run.ecc_uncorrectable_pages,
                          (unsigned int)run.ecc_worst_bits,
                          (unsigned int)run.first_corrected_row,
                          (unsigned int)run.total_cycles,
                          (unsigned int)run.array_cycles,
                          (unsigned int)run.cache_cycles,
                          (unsigned int)run.wall_cycles,
                          (unsigned int)(preflight.spi_timeout_count +
                                         run.spi_timeout_count),
                          (unsigned int)(preflight.ready_timeout_count +
                                         run.ready_timeout_count),
                          (unsigned int)(preflight.dma_timeout_count +
                                         run.dma_timeout_count),
                          (unsigned int)(preflight.dma_error_count +
                                         run.dma_error_count));
    }
    else
    {
        (void)rt_snprintf(line, sizeof(line), h4v1_stage4_fail_format,
                          result,
                          (unsigned int)H4V1_STAGE4_BLOCK,
                          (unsigned int)fail_row,
                          (unsigned int)run.processed_pages,
                          (unsigned int)(H4V1_STAGE4_PAGES_PER_BLOCK *
                                         H4V1_STAGE4_REPEATS),
                          (unsigned int)preflight.marker,
                          (unsigned int)preflight.mid,
                          (unsigned int)preflight.did,
                          (unsigned int)preflight.config,
                          (unsigned int)run.status_last,
                          (unsigned int)run.status2_last,
                          (unsigned int)run.ecc_clean_pages,
                          (unsigned int)run.ecc_corrected_pages,
                          (unsigned int)run.ecc_uncorrectable_pages,
                          (unsigned int)run.ecc_worst_bits,
                          (unsigned int)run.first_uncorrectable_row,
                          (unsigned int)run.first_bad_row,
                          (unsigned int)run.first_bad_offset,
                          (unsigned int)run.first_bad_value,
                          (unsigned int)(preflight.spi_timeout_count +
                                         run.spi_timeout_count),
                          (unsigned int)(preflight.ready_timeout_count +
                                         run.ready_timeout_count),
                          (unsigned int)(preflight.dma_timeout_count +
                                         run.dma_timeout_count),
                          (unsigned int)(preflight.dma_error_count +
                                         run.dma_error_count),
                          (unsigned int)restore_failed);
    }
    h4v1_postpass_stage1_log(line);
    return result;
}
