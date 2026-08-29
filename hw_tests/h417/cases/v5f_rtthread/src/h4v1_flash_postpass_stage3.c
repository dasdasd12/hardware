/*
 * H4V1 Flash Stage 3: isolated, read-only SPI1 cache bandwidth sweep.
 *
 * Stage 2 has already established that the selected block is factory-good,
 * that its first four pages are blank, and that the final page is resident in
 * the NAND cache.  This stage deliberately emits no PAGE READ: it repeatedly
 * reads that known cache image while changing only the SPI receive timing and
 * divider.  CPU polling gives a software baseline; DMA1 channels 2/3 give the
 * practical single-line SPI ceiling without touching DMA2, SDRAM or USB.
 *
 * The only NAND opcodes in this translation unit are READ ID (9f), GET
 * FEATURE (0f) and READ FROM CACHE (03).  Every failed byte transfer raises CS
 * immediately, and all dummy bytes are 00.
 */
#include "h4v1_flash_postpass_stage3.h"

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

#define H4V1_STAGE3_CMD_GET_FEATURE       0x0Fu
#define H4V1_STAGE3_CMD_READ_CACHE        0x03u
#define H4V1_STAGE3_CMD_READ_ID           0x9Fu
#define H4V1_STAGE3_FEATURE_CONFIG        0xB0u
#define H4V1_STAGE3_EXPECTED_MID           0xC8u
#define H4V1_STAGE3_EXPECTED_DID           0x91u
#define H4V1_STAGE3_CONFIG_ECC_ENABLE      0x10u

#define H4V1_STAGE3_SPI_SPE                0x0040u
#define H4V1_STAGE3_SPI_BR_MASK            0x0038u
#define H4V1_STAGE3_SPI_RXNE               0x0001u
#define H4V1_STAGE3_SPI_TXE                0x0002u
#define H4V1_STAGE3_SPI_BSY                0x0080u
#define H4V1_STAGE3_SPI_RXDMAEN            0x0001u
#define H4V1_STAGE3_SPI_TXDMAEN            0x0002u
#define H4V1_STAGE3_SPI_HSRXEN             0x0001u
#define H4V1_STAGE3_SPI_HSRXEN2            0x0004u

#define H4V1_STAGE3_DMA_EN                 0x0001u
#define H4V1_STAGE3_DMA_DIR                0x0010u
#define H4V1_STAGE3_DMA_MINC               0x0080u
#define H4V1_STAGE3_DMA_PRIORITY_VERY_HIGH 0x3000u
#define H4V1_STAGE3_DMA_GL2                0x00000010u
#define H4V1_STAGE3_DMA_TC2                0x00000020u
#define H4V1_STAGE3_DMA_TE2                0x00000080u
#define H4V1_STAGE3_DMA_GL3                0x00000100u
#define H4V1_STAGE3_DMA_TC3                0x00000200u
#define H4V1_STAGE3_DMA_TE3                0x00000800u
#define H4V1_STAGE3_DMA_DONE_MASK          \
    (H4V1_STAGE3_DMA_TC2 | H4V1_STAGE3_DMA_TC3)
#define H4V1_STAGE3_DMA_ERROR_MASK         \
    (H4V1_STAGE3_DMA_TE2 | H4V1_STAGE3_DMA_TE3)

#define H4V1_STAGE3_DMAMUX_CH2_SHIFT       8u
#define H4V1_STAGE3_DMAMUX_CH3_SHIFT       16u
#define H4V1_STAGE3_DMAMUX_FIELD_MASK      0x7Fu
#define H4V1_STAGE3_DMA_REQUEST_SPI1_TX    63u
#define H4V1_STAGE3_DMA_REQUEST_SPI1_RX    64u

#define H4V1_STAGE3_RCC_DMA1               0x00000001u
#define H4V1_STAGE3_SPI_BYTE_TIMEOUT       1000000u
#define H4V1_STAGE3_DMA_TIMEOUT_CYCLES     40000000u
#define H4V1_STAGE3_CORE_HZ                400000000u
#define H4V1_STAGE3_HCLK_HZ                100000000u
#define H4V1_STAGE3_PAGE_BYTES             2048u
#define H4V1_STAGE3_CPU_REPEATS            16u
#define H4V1_STAGE3_DMA_REPEATS            128u
#define H4V1_STAGE3_PROBE_REPEATS          16u
#define H4V1_STAGE3_RX_ADDR                0x20174000u
#define H4V1_STAGE3_TX_ZERO_ADDR           0x20174800u
#define H4V1_STAGE3_FNV_OFFSET             2166136261u
#define H4V1_STAGE3_FNV_PRIME              16777619u

typedef struct
{
    uint8_t br;
    uint8_t divisor;
    uint8_t hsrx;
    uint8_t reserved;
} h4v1_stage3_rate_t;

typedef struct
{
    uint32_t cycles;
    uint32_t hash;
    uint32_t ff_count;
    uint32_t spi_timeout_count;
    uint32_t dma_timeout_count;
    uint32_t dma_error_count;
    uint32_t bad_id_count;
    uint32_t bad_feature_count;
    uint8_t mid;
    uint8_t did;
    uint8_t config;
    uint8_t valid;
} h4v1_stage3_result_t;

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
} h4v1_stage3_saved_t;

extern void h4v1_postpass_stage1_log(const char *text);

const h4v1_stage3_rate_t h4v1_stage3_rates[] H4V1_POSTPASS_RODATA =
{
    { 0x10u, 8u, 0u, 0u }, /* BR2, normal: HCLK/8  = 12.5 MHz. */
    { 0x08u, 4u, 0u, 0u }, /* BR1, normal: HCLK/4  = 25 MHz. */
    { 0x10u, 4u, 1u, 0u }, /* BR2, HSRX1: HCLK/4  = 25 MHz. */
    { 0x08u, 3u, 1u, 0u }, /* BR1, HSRX1: HCLK/3  = 33.3 MHz. */
    { 0x00u, 2u, 0u, 0u }, /* BR0, normal: HCLK/2  = 50 MHz. */
    { 0x00u, 2u, 1u, 0u }, /* BR0, HSRX1: HCLK/2  = 50 MHz. */
};

const char h4v1_stage3_start_text[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE3 START access=cache_only read_only=1 source=stage2_last_page "
    "rates=12.5,25,25h,33.3h,50,50hMHz cpu_bytes=32768 dma_bytes=262144 "
    "dma=DMA1_CH2_RX64+CH3_TX63 scratch=20174000 blank_only=1 "
    "no_page_read=1 no_reset=1 no_set_feature=1 no_dma2=1 no_sdram=1";
const char h4v1_stage3_rate_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE3 RATE idx=%u div=%u hsrx=%u sck_khz=%u valid=%u "
    "id=%02x%02x b0=%02x cpu_KiBps=%u dma_KiBps=%u "
    "cpu_hash=%08x dma_hash=%08x cpu_ff=%u/%u dma_ff=%u/%u "
    "spi_timeout=%u dma_timeout=%u dma_error=%u bad_id=%u bad_b0=%u";
const char h4v1_stage3_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE3 PASS valid_mask=%02x best_idx=%u best_div=%u best_hsrx=%u "
    "best_sck_khz=%u best_dma_KiBps=%u raw_limit_KiBps=%u "
    "sample=cache_blank_256KiB scope=cache_bus_not_array_e2e hsrx2=off";
const char h4v1_stage3_fail_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE3 FAIL result=%d valid_mask=%02x dma_busy=%u "
    "scope=cache_bus_not_array_e2e";

uint32_t H4V1_POSTPASS_TEXT h4v1_stage3_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

void H4V1_POSTPASS_TEXT h4v1_stage3_fence(void)
{
    __asm volatile ("fence iorw, iorw" ::: "memory");
}

void H4V1_POSTPASS_TEXT h4v1_stage3_select(void)
{
    GPIOF->BCR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage3_deselect(void)
{
    GPIOF->BSHR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage3_drain_rx(void)
{
    uint32_t guard = 16u;

    while((guard != 0u) && ((SPI1->STATR & H4V1_STAGE3_SPI_RXNE) != 0u))
    {
        (void)SPI1->DATAR;
        guard--;
    }
    (void)SPI1->STATR;
}

uint8_t H4V1_POSTPASS_TEXT
h4v1_stage3_transfer(h4v1_stage3_result_t *result, uint8_t tx)
{
    uint32_t timeout = H4V1_STAGE3_SPI_BYTE_TIMEOUT;

    while(((SPI1->STATR & H4V1_STAGE3_SPI_TXE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        result->spi_timeout_count++;
        return 0xFFu;
    }
    SPI1->DATAR = tx;

    timeout = H4V1_STAGE3_SPI_BYTE_TIMEOUT;
    while(((SPI1->STATR & H4V1_STAGE3_SPI_RXNE) == 0u) && (timeout != 0u))
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
h4v1_stage3_send(h4v1_stage3_result_t *result, uint8_t value,
                 uint32_t timeout_before)
{
    (void)h4v1_stage3_transfer(result, value);
    return (result->spi_timeout_count == timeout_before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_get_feature(h4v1_stage3_result_t *result,
                        uint8_t address, uint8_t *value)
{
    uint32_t before = result->spi_timeout_count;

    h4v1_stage3_select();
    if(h4v1_stage3_send(result, H4V1_STAGE3_CMD_GET_FEATURE, before) != 0 ||
       h4v1_stage3_send(result, address, before) != 0)
    {
        h4v1_stage3_deselect();
        return -1;
    }
    *value = h4v1_stage3_transfer(result, 0x00u);
    h4v1_stage3_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT h4v1_stage3_read_id(h4v1_stage3_result_t *result)
{
    uint32_t before = result->spi_timeout_count;

    h4v1_stage3_select();
    if(h4v1_stage3_send(result, H4V1_STAGE3_CMD_READ_ID, before) != 0 ||
       h4v1_stage3_send(result, 0x00u, before) != 0)
    {
        h4v1_stage3_deselect();
        return -1;
    }
    result->mid = h4v1_stage3_transfer(result, 0x00u);
    if(result->spi_timeout_count != before)
    {
        h4v1_stage3_deselect();
        return -1;
    }
    result->did = h4v1_stage3_transfer(result, 0x00u);
    h4v1_stage3_deselect();
    if(result->spi_timeout_count != before)
    {
        return -1;
    }
    return ((result->mid == H4V1_STAGE3_EXPECTED_MID) &&
            (result->did == H4V1_STAGE3_EXPECTED_DID)) ? 0 : -2;
}

void H4V1_POSTPASS_TEXT h4v1_stage3_apply_rate(const h4v1_stage3_rate_t *rate)
{
    uint16_t ctlr1 = SPI1->CTLR1;
    uint16_t hscr = SPI1->HSCR;

    SPI1->CTLR1 = ctlr1 & (uint16_t)~H4V1_STAGE3_SPI_SPE;
    h4v1_stage3_drain_rx();
    hscr &= (uint16_t)~(H4V1_STAGE3_SPI_HSRXEN |
                        H4V1_STAGE3_SPI_HSRXEN2);
    if(rate->hsrx != 0u)
    {
        hscr |= H4V1_STAGE3_SPI_HSRXEN;
    }
    SPI1->HSCR = hscr;
    ctlr1 &= (uint16_t)~H4V1_STAGE3_SPI_BR_MASK;
    ctlr1 |= rate->br;
    SPI1->CTLR1 = ctlr1 | H4V1_STAGE3_SPI_SPE;
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_probe_rate(h4v1_stage3_result_t *result)
{
    uint32_t repeat;

    for(repeat = 0u; repeat < H4V1_STAGE3_PROBE_REPEATS; ++repeat)
    {
        uint8_t config = 0xFFu;
        int id_result = h4v1_stage3_read_id(result);

        if(id_result != 0)
        {
            result->bad_id_count++;
            return -1;
        }
        if((h4v1_stage3_get_feature(result,
                                    H4V1_STAGE3_FEATURE_CONFIG,
                                    &config) != 0) ||
           ((config & H4V1_STAGE3_CONFIG_ECC_ENABLE) == 0u))
        {
            result->bad_feature_count++;
            result->config = config;
            return -1;
        }
        result->config = config;
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_read_cache_cpu(h4v1_stage3_result_t *result,
                           uint8_t *data, uint32_t length,
                           uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t index;
    uint32_t start = h4v1_stage3_cycle_now();

    h4v1_stage3_select();
    if(h4v1_stage3_send(result, H4V1_STAGE3_CMD_READ_CACHE, before) != 0 ||
       h4v1_stage3_send(result, 0x00u, before) != 0 ||
       h4v1_stage3_send(result, 0x00u, before) != 0 ||
       h4v1_stage3_send(result, 0x00u, before) != 0)
    {
        h4v1_stage3_deselect();
        *cycles_out = h4v1_stage3_cycle_now() - start;
        return -1;
    }
    for(index = 0u; index < length; ++index)
    {
        data[index] = h4v1_stage3_transfer(result, 0x00u);
        if(result->spi_timeout_count != before)
        {
            h4v1_stage3_deselect();
            *cycles_out = h4v1_stage3_cycle_now() - start;
            return -1;
        }
    }
    h4v1_stage3_deselect();
    *cycles_out = h4v1_stage3_cycle_now() - start;
    return 0;
}

void H4V1_POSTPASS_TEXT h4v1_stage3_dma_stop(void)
{
    DMA1_Channel3->CFGR &= ~H4V1_STAGE3_DMA_EN;
    DMA1_Channel2->CFGR &= ~H4V1_STAGE3_DMA_EN;
    SPI1->CTLR2 &= (uint16_t)~(H4V1_STAGE3_SPI_RXDMAEN |
                                H4V1_STAGE3_SPI_TXDMAEN);
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_dma_stop_wait(h4v1_stage3_result_t *result)
{
    uint32_t start = h4v1_stage3_cycle_now();

    h4v1_stage3_dma_stop();
    while((((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) &
            H4V1_STAGE3_DMA_EN) != 0u))
    {
        if((uint32_t)(h4v1_stage3_cycle_now() - start) >=
           H4V1_STAGE3_DMA_TIMEOUT_CYCLES)
        {
            result->dma_timeout_count++;
            return -1;
        }
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_read_cache_dma(h4v1_stage3_result_t *result,
                           uint8_t *data, uint32_t length,
                           uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t start = h4v1_stage3_cycle_now();

    if(h4v1_stage3_dma_stop_wait(result) != 0)
    {
        h4v1_stage3_deselect();
        *cycles_out = h4v1_stage3_cycle_now() - start;
        return -2;
    }
    h4v1_stage3_drain_rx();
    DMA1_Channel2->MADDR = (uint32_t)(uintptr_t)data;
    DMA1_Channel2->CNTR = length;
    DMA1_Channel3->MADDR = H4V1_STAGE3_TX_ZERO_ADDR;
    DMA1_Channel3->CNTR = length;
    DMA1->INTFCR = H4V1_STAGE3_DMA_GL2 | H4V1_STAGE3_DMA_GL3;
    start = h4v1_stage3_cycle_now();

    h4v1_stage3_select();
    if(h4v1_stage3_send(result, H4V1_STAGE3_CMD_READ_CACHE, before) != 0 ||
       h4v1_stage3_send(result, 0x00u, before) != 0 ||
       h4v1_stage3_send(result, 0x00u, before) != 0 ||
       h4v1_stage3_send(result, 0x00u, before) != 0)
    {
        goto spi_failed;
    }

    SPI1->CTLR2 |= H4V1_STAGE3_SPI_RXDMAEN | H4V1_STAGE3_SPI_TXDMAEN;
    h4v1_stage3_fence();
    DMA1_Channel2->CFGR |= H4V1_STAGE3_DMA_EN;
    DMA1_Channel3->CFGR |= H4V1_STAGE3_DMA_EN;

    while((DMA1->INTFR & H4V1_STAGE3_DMA_DONE_MASK) !=
          H4V1_STAGE3_DMA_DONE_MASK)
    {
        if((DMA1->INTFR & H4V1_STAGE3_DMA_ERROR_MASK) != 0u)
        {
            result->dma_error_count++;
            goto dma_failed;
        }
        if((uint32_t)(h4v1_stage3_cycle_now() - start) >=
           H4V1_STAGE3_DMA_TIMEOUT_CYCLES)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    if((DMA1->INTFR & H4V1_STAGE3_DMA_ERROR_MASK) != 0u)
    {
        result->dma_error_count++;
        goto dma_failed;
    }
    while((SPI1->STATR & H4V1_STAGE3_SPI_BSY) != 0u)
    {
        if((uint32_t)(h4v1_stage3_cycle_now() - start) >=
           H4V1_STAGE3_DMA_TIMEOUT_CYCLES)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    if((DMA1->INTFR & H4V1_STAGE3_DMA_ERROR_MASK) != 0u)
    {
        result->dma_error_count++;
        goto dma_failed;
    }

    if(h4v1_stage3_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
    h4v1_stage3_deselect();
    DMA1->INTFCR = H4V1_STAGE3_DMA_GL2 | H4V1_STAGE3_DMA_GL3;
    h4v1_stage3_fence();
    *cycles_out = h4v1_stage3_cycle_now() - start;
    return 0;

dma_failed:
    if(h4v1_stage3_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
spi_failed:
    h4v1_stage3_deselect();
    DMA1->INTFCR = H4V1_STAGE3_DMA_GL2 | H4V1_STAGE3_DMA_GL3;
    *cycles_out = h4v1_stage3_cycle_now() - start;
    return -1;

dma_stuck:
    h4v1_stage3_deselect();
    DMA1->INTFCR = H4V1_STAGE3_DMA_GL2 | H4V1_STAGE3_DMA_GL3;
    *cycles_out = h4v1_stage3_cycle_now() - start;
    return -2;
}

void H4V1_POSTPASS_TEXT
h4v1_stage3_accumulate(h4v1_stage3_result_t *result,
                       const uint8_t *data, uint32_t length)
{
    uint32_t index;

    for(index = 0u; index < length; ++index)
    {
        uint8_t value = *(volatile const uint8_t *)&data[index];

        result->hash = (result->hash ^ value) * H4V1_STAGE3_FNV_PRIME;
        if(value == 0xFFu)
        {
            result->ff_count++;
        }
    }
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_run_cpu(h4v1_stage3_result_t *result)
{
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE3_RX_ADDR;
    uint32_t repeat;

    result->hash = H4V1_STAGE3_FNV_OFFSET;
    for(repeat = 0u; repeat < H4V1_STAGE3_CPU_REPEATS; ++repeat)
    {
        uint32_t cycles = 0u;

        if(h4v1_stage3_read_cache_cpu(result, data,
                                      H4V1_STAGE3_PAGE_BYTES,
                                      &cycles) != 0)
        {
            return -1;
        }
        result->cycles += cycles;
        h4v1_stage3_accumulate(result, data, H4V1_STAGE3_PAGE_BYTES);
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_run_dma(h4v1_stage3_result_t *result)
{
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE3_RX_ADDR;
    uint32_t repeat;

    result->hash = H4V1_STAGE3_FNV_OFFSET;
    for(repeat = 0u; repeat < H4V1_STAGE3_DMA_REPEATS; ++repeat)
    {
        uint32_t cycles = 0u;

        int transfer_result =
            h4v1_stage3_read_cache_dma(result, data,
                                       H4V1_STAGE3_PAGE_BYTES,
                                       &cycles);

        if(transfer_result != 0)
        {
            return transfer_result;
        }
        result->cycles += cycles;
        h4v1_stage3_accumulate(result, data, H4V1_STAGE3_PAGE_BYTES);
    }
    return 0;
}

uint32_t H4V1_POSTPASS_TEXT
h4v1_stage3_kib_per_second(uint32_t bytes, uint32_t cycles)
{
    uint32_t elapsed_kcycles = (cycles + 500u) / 1000u;
    uint32_t sample_kib = bytes / 1024u;

    /* Keep the calculation 32-bit so the isolated tail cannot pull a new
     * 64-bit division helper into the qualified image. */
    return (elapsed_kcycles == 0u) ? 0u :
        (sample_kib * (H4V1_STAGE3_CORE_HZ / 1000u)) / elapsed_kcycles;
}

void H4V1_POSTPASS_TEXT
h4v1_stage3_save_and_configure_dma(h4v1_stage3_saved_t *saved)
{
    saved->dma_clock_was_enabled = RCC->HBPCENR & H4V1_STAGE3_RCC_DMA1;
    RCC->HBPCENR |= H4V1_STAGE3_RCC_DMA1;
    h4v1_stage3_fence();

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
        H4V1_STAGE3_DMA_EN) != 0u)
    {
        /* Fail closed without disturbing an owner that is already active. */
        return;
    }

    h4v1_stage3_dma_stop();
    DMA1_Channel2->CFGR = H4V1_STAGE3_DMA_PRIORITY_VERY_HIGH |
                              H4V1_STAGE3_DMA_MINC;
    DMA1_Channel2->CNTR = 0u;
    DMA1_Channel2->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel2->MADDR = H4V1_STAGE3_RX_ADDR;
    DMA1_Channel2->M1ADDR = 0u;
    DMA1_Channel3->CFGR = H4V1_STAGE3_DMA_PRIORITY_VERY_HIGH |
                              H4V1_STAGE3_DMA_DIR;
    DMA1_Channel3->CNTR = 0u;
    DMA1_Channel3->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel3->MADDR = H4V1_STAGE3_TX_ZERO_ADDR;
    DMA1_Channel3->M1ADDR = 0u;
    DMAMUX->CFGR0_3 =
        (saved->dmamux_cfgr0_3 &
         ~((H4V1_STAGE3_DMAMUX_FIELD_MASK << H4V1_STAGE3_DMAMUX_CH2_SHIFT) |
           (H4V1_STAGE3_DMAMUX_FIELD_MASK << H4V1_STAGE3_DMAMUX_CH3_SHIFT))) |
        ((H4V1_STAGE3_DMA_REQUEST_SPI1_RX - 1u) <<
         H4V1_STAGE3_DMAMUX_CH2_SHIFT) |
        ((H4V1_STAGE3_DMA_REQUEST_SPI1_TX - 1u) <<
         H4V1_STAGE3_DMAMUX_CH3_SHIFT);
    *(volatile uint8_t *)(uintptr_t)H4V1_STAGE3_TX_ZERO_ADDR = 0u;
    DMA1->INTFCR = H4V1_STAGE3_DMA_GL2 | H4V1_STAGE3_DMA_GL3;
}

int H4V1_POSTPASS_TEXT
h4v1_stage3_restore(const h4v1_stage3_saved_t *saved)
{
    h4v1_stage3_result_t stop_result = {0};

    if(((saved->dma2_cfgr | saved->dma3_cfgr) &
        H4V1_STAGE3_DMA_EN) != 0u)
    {
        /* save_and_configure_dma() made no peripheral changes in this case. */
        if(saved->dma_clock_was_enabled == 0u)
        {
            RCC->HBPCENR &= ~H4V1_STAGE3_RCC_DMA1;
        }
        return 0;
    }

    if(h4v1_stage3_dma_stop_wait(&stop_result) != 0)
    {
        h4v1_stage3_deselect();
        return -1;
    }
    h4v1_stage3_deselect();
    DMA1->INTFCR = H4V1_STAGE3_DMA_GL2 | H4V1_STAGE3_DMA_GL3;

    DMA1_Channel2->CFGR = saved->dma2_cfgr & ~H4V1_STAGE3_DMA_EN;
    DMA1_Channel2->CNTR = saved->dma2_cntr;
    DMA1_Channel2->PADDR = saved->dma2_paddr;
    DMA1_Channel2->MADDR = saved->dma2_maddr;
    DMA1_Channel2->M1ADDR = saved->dma2_m1addr;
    DMA1_Channel3->CFGR = saved->dma3_cfgr & ~H4V1_STAGE3_DMA_EN;
    DMA1_Channel3->CNTR = saved->dma3_cntr;
    DMA1_Channel3->PADDR = saved->dma3_paddr;
    DMA1_Channel3->MADDR = saved->dma3_maddr;
    DMA1_Channel3->M1ADDR = saved->dma3_m1addr;
    DMAMUX->CFGR0_3 = saved->dmamux_cfgr0_3;

    SPI1->CTLR1 &= (uint16_t)~H4V1_STAGE3_SPI_SPE;
    h4v1_stage3_drain_rx();
    SPI1->HSCR = saved->spi_hscr;
    SPI1->CTLR2 = saved->spi_ctlr2;
    SPI1->CTLR1 = saved->spi_ctlr1;
    if(saved->dma_clock_was_enabled == 0u)
    {
        RCC->HBPCENR &= ~H4V1_STAGE3_RCC_DMA1;
    }
    h4v1_stage3_fence();
    return 0;
}

int H4V1_POSTPASS_ENTRY h4v1_flash_postpass_stage3_run(void)
{
    h4v1_stage3_saved_t saved;
    uint32_t rate_count = (uint32_t)(sizeof(h4v1_stage3_rates) /
                                      sizeof(h4v1_stage3_rates[0]));
    uint32_t index;
    uint32_t valid_mask = 0u;
    uint32_t best_index = 0u;
    uint32_t best_kib = 0u;
    uint32_t dma_busy = 0u;
    int result = H4V1_FLASH_POSTPASS_STAGE3_OK;
    char line[384];

    h4v1_postpass_stage1_log(h4v1_stage3_start_text);
    h4v1_stage3_save_and_configure_dma(&saved);

    if(((saved.dma2_cfgr | saved.dma3_cfgr) & H4V1_STAGE3_DMA_EN) != 0u)
    {
        dma_busy = 1u;
        result = H4V1_FLASH_POSTPASS_STAGE3_ERR_DMA_BUSY;
    }

    for(index = 0u; (index < rate_count) && (result == 0); ++index)
    {
        const h4v1_stage3_rate_t *rate = &h4v1_stage3_rates[index];
        h4v1_stage3_result_t probe = {0};
        h4v1_stage3_result_t cpu = {0};
        h4v1_stage3_result_t dma = {0};
        uint32_t cpu_bytes = H4V1_STAGE3_PAGE_BYTES *
                             H4V1_STAGE3_CPU_REPEATS;
        uint32_t dma_bytes = H4V1_STAGE3_PAGE_BYTES *
                             H4V1_STAGE3_DMA_REPEATS;
        uint32_t cpu_kib = 0u;
        uint32_t dma_kib = 0u;
        uint32_t sck_khz = (H4V1_STAGE3_HCLK_HZ / rate->divisor) / 1000u;
        int probe_result;
        int cpu_result = -1;
        int dma_result = -1;

        h4v1_stage3_apply_rate(rate);
        probe_result = h4v1_stage3_probe_rate(&probe);
        if(probe_result == 0)
        {
            cpu_result = h4v1_stage3_run_cpu(&cpu);
            if(cpu_result == 0)
            {
                cpu_kib = h4v1_stage3_kib_per_second(cpu_bytes,
                                                       cpu.cycles);
            }
            if((cpu_result == 0) && (cpu.ff_count == cpu_bytes))
            {
                dma_result = h4v1_stage3_run_dma(&dma);
                if(dma_result == -2)
                {
                    result = H4V1_FLASH_POSTPASS_STAGE3_ERR_DMA_QUIESCE;
                }
                if(dma_result == 0)
                {
                    dma_kib = h4v1_stage3_kib_per_second(dma_bytes,
                                                           dma.cycles);
                }
            }
        }
        if((probe_result == 0) &&
           (cpu_result == 0) && (cpu.ff_count == cpu_bytes) &&
           (dma_result == 0) && (dma.ff_count == dma_bytes))
        {
            probe.valid = 1u;
            valid_mask |= 1u << index;
            cpu_kib = h4v1_stage3_kib_per_second(cpu_bytes, cpu.cycles);
            dma_kib = h4v1_stage3_kib_per_second(dma_bytes, dma.cycles);
            if(dma_kib > best_kib)
            {
                best_kib = dma_kib;
                best_index = index;
            }
        }

        (void)rt_snprintf(line, sizeof(line), h4v1_stage3_rate_format,
                          (unsigned int)index,
                          (unsigned int)rate->divisor,
                          (unsigned int)rate->hsrx,
                          (unsigned int)sck_khz,
                          (unsigned int)probe.valid,
                          (unsigned int)probe.mid,
                          (unsigned int)probe.did,
                          (unsigned int)probe.config,
                          (unsigned int)cpu_kib,
                          (unsigned int)dma_kib,
                          (unsigned int)cpu.hash,
                          (unsigned int)dma.hash,
                          (unsigned int)cpu.ff_count,
                          (unsigned int)cpu_bytes,
                          (unsigned int)dma.ff_count,
                          (unsigned int)dma_bytes,
                          (unsigned int)(probe.spi_timeout_count +
                                         cpu.spi_timeout_count +
                                         dma.spi_timeout_count),
                          (unsigned int)dma.dma_timeout_count,
                          (unsigned int)dma.dma_error_count,
                          (unsigned int)probe.bad_id_count,
                          (unsigned int)probe.bad_feature_count);
        h4v1_postpass_stage1_log(line);
    }

    if((h4v1_stage3_restore(&saved) != 0) &&
       (result == H4V1_FLASH_POSTPASS_STAGE3_OK))
    {
        result = H4V1_FLASH_POSTPASS_STAGE3_ERR_DMA_QUIESCE;
    }
    if((result == H4V1_FLASH_POSTPASS_STAGE3_OK) &&
       ((valid_mask & 0x01u) == 0u))
    {
        result = H4V1_FLASH_POSTPASS_STAGE3_ERR_BASELINE;
    }
    else if((result == H4V1_FLASH_POSTPASS_STAGE3_OK) &&
            (valid_mask == 0u))
    {
        result = H4V1_FLASH_POSTPASS_STAGE3_ERR_NO_VALID_RATE;
    }

    if(result == H4V1_FLASH_POSTPASS_STAGE3_OK)
    {
        const h4v1_stage3_rate_t *best = &h4v1_stage3_rates[best_index];
        uint32_t best_sck = (H4V1_STAGE3_HCLK_HZ / best->divisor) / 1000u;
        uint32_t raw_limit = (H4V1_STAGE3_HCLK_HZ / best->divisor) /
                             (8u * 1024u);

        (void)rt_snprintf(line, sizeof(line), h4v1_stage3_pass_format,
                          (unsigned int)valid_mask,
                          (unsigned int)best_index,
                          (unsigned int)best->divisor,
                          (unsigned int)best->hsrx,
                          (unsigned int)best_sck,
                          (unsigned int)best_kib,
                          (unsigned int)raw_limit);
    }
    else
    {
        (void)rt_snprintf(line, sizeof(line), h4v1_stage3_fail_format,
                          result,
                          (unsigned int)valid_mask,
                          (unsigned int)dma_busy);
    }
    h4v1_postpass_stage1_log(line);
    return result;
}
