/*
 * H4V1 Flash Stage 5: destructive, single-block NAND write probe.
 *
 * This translation unit is linked only by the explicit write-probe target.
 * It owns fixed scratch block 1015 for the duration of the probe and never
 * searches into the adjacent L8 asset blocks.  The operation is fail closed:
 * the factory bad-block marker and feature state are checked before the first
 * erase, and every exit attempts WRDI, A0 restoration and hardware restore.
 */
#include "h4v1_flash_postpass_stage5.h"

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

/* The complete and deliberately small command whitelist. */
#define H4V1_STAGE5_CMD_PROGRAM_LOAD       0x02u
#define H4V1_STAGE5_CMD_READ_CACHE         0x03u
#define H4V1_STAGE5_CMD_WRITE_DISABLE      0x04u
#define H4V1_STAGE5_CMD_WRITE_ENABLE       0x06u
#define H4V1_STAGE5_CMD_GET_FEATURE        0x0Fu
#define H4V1_STAGE5_CMD_PROGRAM_EXECUTE    0x10u
#define H4V1_STAGE5_CMD_PAGE_READ          0x13u
#define H4V1_STAGE5_CMD_SET_FEATURE        0x1Fu
#define H4V1_STAGE5_CMD_READ_ID            0x9Fu
#define H4V1_STAGE5_CMD_BLOCK_ERASE        0xD8u

#define H4V1_STAGE5_FEATURE_PROTECTION     0xA0u
#define H4V1_STAGE5_FEATURE_CONFIG         0xB0u
#define H4V1_STAGE5_FEATURE_STATUS         0xC0u
#define H4V1_STAGE5_FEATURE_STATUS2        0xF0u
#define H4V1_STAGE5_EXPECTED_MID           0xC8u
#define H4V1_STAGE5_EXPECTED_DID           0x91u
#define H4V1_STAGE5_A0_EXPECTED_LOCKED     0x38u
#define H4V1_STAGE5_A0_TEMP                0x0Au
#define H4V1_STAGE5_A0_BRWD                0x80u
#define H4V1_STAGE5_B0_ECC_ENABLE          0x10u
#define H4V1_STAGE5_B0_BPL                 0x08u
#define H4V1_STAGE5_B0_OTP_ENABLE          0x40u
#define H4V1_STAGE5_B0_QUAD_ENABLE         0x01u
#define H4V1_STAGE5_STATUS_OIP             0x01u
#define H4V1_STAGE5_STATUS_WEL             0x02u
#define H4V1_STAGE5_STATUS_E_FAIL          0x04u
#define H4V1_STAGE5_STATUS_P_FAIL          0x08u
#define H4V1_STAGE5_STATUS_ECC_MASK        0x30u
#define H4V1_STAGE5_STATUS_ECC_CLEAN       0x00u
#define H4V1_STAGE5_STATUS_ECC_CORRECTED   0x10u
#define H4V1_STAGE5_STATUS_ECC_FAILED      0x20u
#define H4V1_STAGE5_STATUS_ECC_8_BITS      0x30u
#define H4V1_STAGE5_STATUS2_ECCSE_MASK     0x30u

#define H4V1_STAGE5_SPI_SPE                0x0040u
#define H4V1_STAGE5_SPI_BR_MASK            0x0038u
#define H4V1_STAGE5_SPI_RXNE               0x0001u
#define H4V1_STAGE5_SPI_TXE                0x0002u
#define H4V1_STAGE5_SPI_BSY                0x0080u
#define H4V1_STAGE5_SPI_RXDMAEN            0x0001u
#define H4V1_STAGE5_SPI_TXDMAEN            0x0002u
#define H4V1_STAGE5_SPI_HSRXEN             0x0001u
#define H4V1_STAGE5_SPI_HSRXEN2            0x0004u

#define H4V1_STAGE5_DMA_EN                 0x0001u
#define H4V1_STAGE5_DMA_DIR                0x0010u
#define H4V1_STAGE5_DMA_MINC               0x0080u
#define H4V1_STAGE5_DMA_PRIORITY_VERY_HIGH 0x3000u
#define H4V1_STAGE5_DMA_GL2                0x00000010u
#define H4V1_STAGE5_DMA_TC2                0x00000020u
#define H4V1_STAGE5_DMA_TE2                0x00000080u
#define H4V1_STAGE5_DMA_GL3                0x00000100u
#define H4V1_STAGE5_DMA_TC3                0x00000200u
#define H4V1_STAGE5_DMA_TE3                0x00000800u
#define H4V1_STAGE5_DMA_DONE_MASK          \
    (H4V1_STAGE5_DMA_TC2 | H4V1_STAGE5_DMA_TC3)
#define H4V1_STAGE5_DMA_ERROR_MASK         \
    (H4V1_STAGE5_DMA_TE2 | H4V1_STAGE5_DMA_TE3)
#define H4V1_STAGE5_DMAMUX_CH2_SHIFT       8u
#define H4V1_STAGE5_DMAMUX_CH3_SHIFT       16u
#define H4V1_STAGE5_DMAMUX_FIELD_MASK      0x7Fu
#define H4V1_STAGE5_DMA_REQUEST_SPI1_TX    63u
#define H4V1_STAGE5_DMA_REQUEST_SPI1_RX    64u
#define H4V1_STAGE5_RCC_DMA1               0x00000001u

#define H4V1_STAGE5_SPI_BYTE_TIMEOUT       1000000u
#define H4V1_STAGE5_DMA_TIMEOUT            4000000u
#define H4V1_STAGE5_READY_TIMEOUT          80000000u
#define H4V1_STAGE5_BLOCK                  1015u
#define H4V1_STAGE5_ROW                    0x0000FDC0u
#define H4V1_STAGE5_PAGE_BYTES             2048u
#define H4V1_STAGE5_BAD_MARK_COLUMN        2048u
#define H4V1_STAGE5_DATA_ADDR              0x20174000u
#define H4V1_STAGE5_AUX_ADDR               0x20174800u
#define H4V1_STAGE5_FNV_OFFSET             2166136261u
#define H4V1_STAGE5_FNV_PRIME              16777619u

#if (H4V1_STAGE5_BLOCK <= 768u) || (H4V1_STAGE5_BLOCK >= 1016u)
#error "Stage5 scratch block must stay strictly between manifest and L8 blocks"
#endif
#if H4V1_STAGE5_ROW != (H4V1_STAGE5_BLOCK * 64u)
#error "Stage5 row must be page zero of the fixed scratch block"
#endif
#if (H4V1_STAGE5_DATA_ADDR + H4V1_STAGE5_PAGE_BYTES) > H4V1_STAGE5_AUX_ADDR
#error "Stage5 page buffer overlaps the auxiliary DMA byte"
#endif
#if H4V1_STAGE5_AUX_ADDR >= 0x20175000u
#error "Stage5 scratch allocation escapes the approved shared-SRAM tail"
#endif

typedef struct
{
    uint32_t spi_timeout_count;
    uint32_t ready_timeout_count;
    uint32_t dma_timeout_count;
    uint32_t dma_error_count;
    uint32_t illegal_command_count;
    uint32_t ready_polls;
    uint32_t first_bad_offset;
    uint32_t expected_hash;
    uint32_t programmed_hash;
    uint32_t erased_hash_initial;
    uint32_t erased_hash_final;
    uint32_t erase_initial_cycles;
    uint32_t program_cycles;
    uint32_t read_cycles;
    uint32_t erase_final_cycles;
    uint8_t mid;
    uint8_t did;
    uint8_t a0_saved;
    uint8_t b0_saved;
    uint8_t a0_temp_readback;
    uint8_t a0_restored;
    uint8_t b0_readback;
    uint8_t marker_before;
    uint8_t marker_after;
    uint8_t status_last;
    uint8_t status2_last;
    uint8_t ecc_worst_bits;
    uint8_t first_bad_value;
    uint8_t features_saved;
    uint8_t flash_owned;
    uint8_t destructive_started;
    uint8_t final_erase_done;
} h4v1_stage5_result_t;

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
} h4v1_stage5_saved_t;

extern void h4v1_postpass_stage1_log(const char *text);

const char h4v1_stage5_start_text[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE5 START destructive=1 target=write_probe block=1015 "
    "row=0000fdc0 page=0 bytes=2048 marker_required=ff "
    "sequence=marker,unlock,erase,ff,02dma,06wel,10,13+03dma,erase,ff+marker "
    "sck_khz=50000 hsrx=0 scratch=20174000 aux=20174800 "
    "restore=wrdi+a0+dma+spi no_reset=1 no_dma2=1 no_sdram=1";
const char h4v1_stage5_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE5 PASS block=%u row=%08x marker=%02x/%02x "
    "id=%02x%02x a0=%02x/%02x/%02x b0=%02x/%02x "
    "hash=%08x/%08x erased=%08x/%08x ecc_worst=%u polls=%u "
    "cycles=%u/%u/%u/%u timeouts=%u/%u/%u dma_error=%u illegal=%u "
    "blank=1 cleanup=wrdi+a0_restore+feature_verify";
const char h4v1_stage5_fail_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE5 FAIL result=%d primary=%d block=%u row=%08x "
    "marker=%02x/%02x id=%02x%02x a0=%02x/%02x/%02x b0=%02x/%02x "
    "status=%02x f0=%02x bad=%08x/%02x hash=%08x/%08x "
    "timeouts=%u/%u/%u dma_error=%u illegal=%u cleanup=%u "
    "blank=%u dirty_possible=%u";

uint32_t H4V1_POSTPASS_TEXT h4v1_stage5_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

void H4V1_POSTPASS_TEXT h4v1_stage5_fence(void)
{
    __asm volatile ("fence iorw, iorw" ::: "memory");
}

void H4V1_POSTPASS_TEXT h4v1_stage5_select(void)
{
    GPIOF->BCR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage5_deselect(void)
{
    GPIOF->BSHR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage5_drain_rx(void)
{
    uint32_t guard = 16u;

    while((guard != 0u) && ((SPI1->STATR & H4V1_STAGE5_SPI_RXNE) != 0u))
    {
        (void)SPI1->DATAR;
        guard--;
    }
    (void)SPI1->STATR;
}

int H4V1_POSTPASS_TEXT h4v1_stage5_command_allowed(uint8_t command)
{
    return (command == H4V1_STAGE5_CMD_PROGRAM_LOAD) ||
           (command == H4V1_STAGE5_CMD_READ_CACHE) ||
           (command == H4V1_STAGE5_CMD_WRITE_DISABLE) ||
           (command == H4V1_STAGE5_CMD_WRITE_ENABLE) ||
           (command == H4V1_STAGE5_CMD_GET_FEATURE) ||
           (command == H4V1_STAGE5_CMD_PROGRAM_EXECUTE) ||
           (command == H4V1_STAGE5_CMD_PAGE_READ) ||
           (command == H4V1_STAGE5_CMD_SET_FEATURE) ||
           (command == H4V1_STAGE5_CMD_READ_ID) ||
           (command == H4V1_STAGE5_CMD_BLOCK_ERASE);
}

uint8_t H4V1_POSTPASS_TEXT
h4v1_stage5_transfer(h4v1_stage5_result_t *result, uint8_t tx)
{
    uint32_t timeout = H4V1_STAGE5_SPI_BYTE_TIMEOUT;

    while(((SPI1->STATR & H4V1_STAGE5_SPI_TXE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        result->spi_timeout_count++;
        return 0xFFu;
    }
    SPI1->DATAR = tx;
    timeout = H4V1_STAGE5_SPI_BYTE_TIMEOUT;
    while(((SPI1->STATR & H4V1_STAGE5_SPI_RXNE) == 0u) && (timeout != 0u))
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
h4v1_stage5_send(h4v1_stage5_result_t *result, uint8_t value,
                 uint32_t timeout_before)
{
    (void)h4v1_stage5_transfer(result, value);
    return (result->spi_timeout_count == timeout_before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_begin(h4v1_stage5_result_t *result, uint8_t command,
                  uint32_t timeout_before)
{
    if(h4v1_stage5_command_allowed(command) == 0)
    {
        result->illegal_command_count++;
        return -1;
    }
    /* A timed-out CPU/DMA transaction may leave RXNE/OVR pending.  Every
     * command starts from a clean receive state before CS is asserted. */
    h4v1_stage5_drain_rx();
    h4v1_stage5_select();
    if(h4v1_stage5_send(result, command, timeout_before) != 0)
    {
        h4v1_stage5_deselect();
        return -1;
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_command_only(h4v1_stage5_result_t *result, uint8_t command)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage5_begin(result, command, before) != 0)
    {
        return -1;
    }
    h4v1_stage5_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_get_feature(h4v1_stage5_result_t *result,
                        uint8_t address, uint8_t *value)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage5_begin(result, H4V1_STAGE5_CMD_GET_FEATURE, before) != 0 ||
       h4v1_stage5_send(result, address, before) != 0)
    {
        h4v1_stage5_deselect();
        return -1;
    }
    *value = h4v1_stage5_transfer(result, 0x00u);
    h4v1_stage5_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_set_a0(h4v1_stage5_result_t *result, uint8_t value)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage5_begin(result, H4V1_STAGE5_CMD_SET_FEATURE, before) != 0 ||
       h4v1_stage5_send(result, H4V1_STAGE5_FEATURE_PROTECTION, before) != 0 ||
       h4v1_stage5_send(result, value, before) != 0)
    {
        h4v1_stage5_deselect();
        return -1;
    }
    h4v1_stage5_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT h4v1_stage5_read_id(h4v1_stage5_result_t *result)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage5_begin(result, H4V1_STAGE5_CMD_READ_ID, before) != 0 ||
       h4v1_stage5_send(result, 0x00u, before) != 0)
    {
        h4v1_stage5_deselect();
        return -1;
    }
    result->mid = h4v1_stage5_transfer(result, 0x00u);
    result->did = h4v1_stage5_transfer(result, 0x00u);
    h4v1_stage5_deselect();
    if(result->spi_timeout_count != before)
    {
        return -1;
    }
    return ((result->mid == H4V1_STAGE5_EXPECTED_MID) &&
            (result->did == H4V1_STAGE5_EXPECTED_DID)) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_wait_ready(h4v1_stage5_result_t *result, uint8_t *status_out)
{
    uint32_t start = h4v1_stage5_cycle_now();
    uint8_t status = 0xFFu;

    do
    {
        if(h4v1_stage5_get_feature(result,
                                   H4V1_STAGE5_FEATURE_STATUS,
                                   &status) != 0)
        {
            *status_out = status;
            return -1;
        }
        result->ready_polls++;
        if((status & H4V1_STAGE5_STATUS_OIP) == 0u)
        {
            result->status_last = status;
            *status_out = status;
            return 0;
        }
    } while((uint32_t)(h4v1_stage5_cycle_now() - start) <
            H4V1_STAGE5_READY_TIMEOUT);

    result->ready_timeout_count++;
    result->status_last = status;
    *status_out = status;
    return -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_classify_ecc(h4v1_stage5_result_t *result, uint8_t status)
{
    uint8_t ecc = status & H4V1_STAGE5_STATUS_ECC_MASK;
    uint8_t corrected_bits = 0u;

    result->status_last = status;
    if(ecc == H4V1_STAGE5_STATUS_ECC_CLEAN)
    {
        return 0;
    }
    if(ecc == H4V1_STAGE5_STATUS_ECC_FAILED)
    {
        result->ecc_worst_bits = 9u;
        return -2;
    }
    if(ecc == H4V1_STAGE5_STATUS_ECC_8_BITS)
    {
        corrected_bits = 8u;
    }
    else
    {
        uint8_t status2 = 0xFFu;

        if(h4v1_stage5_get_feature(result,
                                   H4V1_STAGE5_FEATURE_STATUS2,
                                   &status2) != 0)
        {
            return -1;
        }
        result->status2_last = status2;
        if((status2 & H4V1_STAGE5_STATUS2_ECCSE_MASK) == 0x00u)
        {
            corrected_bits = 4u;
        }
        else if((status2 & H4V1_STAGE5_STATUS2_ECCSE_MASK) == 0x10u)
        {
            corrected_bits = 5u;
        }
        else if((status2 & H4V1_STAGE5_STATUS2_ECCSE_MASK) == 0x20u)
        {
            corrected_bits = 6u;
        }
        else
        {
            corrected_bits = 7u;
        }
    }
    if(corrected_bits > result->ecc_worst_bits)
    {
        result->ecc_worst_bits = corrected_bits;
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_load_page(h4v1_stage5_result_t *result, uint32_t row,
                       uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t start = h4v1_stage5_cycle_now();
    uint8_t status = 0xFFu;
    int ready;

    if(h4v1_stage5_begin(result, H4V1_STAGE5_CMD_PAGE_READ, before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)(row >> 16), before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)(row >> 8), before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)row, before) != 0)
    {
        h4v1_stage5_deselect();
        *cycles_out = h4v1_stage5_cycle_now() - start;
        return -1;
    }
    h4v1_stage5_deselect();
    ready = h4v1_stage5_wait_ready(result, &status);
    *cycles_out = h4v1_stage5_cycle_now() - start;
    if(ready != 0)
    {
        return (ready == -2) ? -2 : -1;
    }
    ready = h4v1_stage5_classify_ecc(result, status);
    return (ready == -2) ? -3 : ready;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_read_cache_cpu(h4v1_stage5_result_t *result,
                           uint16_t column, uint8_t *data,
                           uint32_t length)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t index;

    if(h4v1_stage5_begin(result, H4V1_STAGE5_CMD_READ_CACHE, before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)(column >> 8), before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)column, before) != 0 ||
       h4v1_stage5_send(result, 0x00u, before) != 0)
    {
        h4v1_stage5_deselect();
        return -1;
    }
    for(index = 0u; index < length; ++index)
    {
        data[index] = h4v1_stage5_transfer(result, 0x00u);
        if(result->spi_timeout_count != before)
        {
            h4v1_stage5_deselect();
            return -1;
        }
    }
    h4v1_stage5_deselect();
    return 0;
}

void H4V1_POSTPASS_TEXT h4v1_stage5_dma_stop(void)
{
    DMA1_Channel3->CFGR &= ~H4V1_STAGE5_DMA_EN;
    DMA1_Channel2->CFGR &= ~H4V1_STAGE5_DMA_EN;
    SPI1->CTLR2 &= (uint16_t)~(H4V1_STAGE5_SPI_RXDMAEN |
                                H4V1_STAGE5_SPI_TXDMAEN);
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_dma_stop_wait(h4v1_stage5_result_t *result)
{
    uint32_t start = h4v1_stage5_cycle_now();

    h4v1_stage5_dma_stop();
    while(((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) &
           H4V1_STAGE5_DMA_EN) != 0u)
    {
        if((uint32_t)(h4v1_stage5_cycle_now() - start) >=
           H4V1_STAGE5_DMA_TIMEOUT)
        {
            result->dma_timeout_count++;
            return -1;
        }
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_spi_wait_idle(h4v1_stage5_result_t *result)
{
    uint32_t start = h4v1_stage5_cycle_now();

    while((SPI1->STATR & H4V1_STAGE5_SPI_BSY) != 0u)
    {
        if((uint32_t)(h4v1_stage5_cycle_now() - start) >=
           H4V1_STAGE5_DMA_TIMEOUT)
        {
            result->spi_timeout_count++;
            return -1;
        }
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_dma_payload(h4v1_stage5_result_t *result,
                        uint8_t command, uint8_t address_bytes,
                        uint8_t address0, uint8_t address1, uint8_t address2,
                        uint32_t rx_address, uint8_t rx_increment,
                        uint32_t tx_address, uint8_t tx_increment,
                        uint32_t length, uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t start;

    if(!(((command == H4V1_STAGE5_CMD_PROGRAM_LOAD) &&
          (address_bytes == 2u) &&
          (address0 == 0u) && (address1 == 0u)) ||
         ((command == H4V1_STAGE5_CMD_READ_CACHE) &&
          (address_bytes == 3u) &&
          (address0 == 0u) && (address1 == 0u) && (address2 == 0u))))
    {
        result->illegal_command_count++;
        *cycles_out = 0u;
        return -1;
    }

    if(h4v1_stage5_dma_stop_wait(result) != 0)
    {
        *cycles_out = 0u;
        return -2;
    }
    h4v1_stage5_drain_rx();
    DMA1_Channel2->CFGR = H4V1_STAGE5_DMA_PRIORITY_VERY_HIGH |
        ((rx_increment != 0u) ? H4V1_STAGE5_DMA_MINC : 0u);
    DMA1_Channel2->MADDR = rx_address;
    DMA1_Channel2->CNTR = length;
    DMA1_Channel3->CFGR = H4V1_STAGE5_DMA_PRIORITY_VERY_HIGH |
        H4V1_STAGE5_DMA_DIR |
        ((tx_increment != 0u) ? H4V1_STAGE5_DMA_MINC : 0u);
    DMA1_Channel3->MADDR = tx_address;
    DMA1_Channel3->CNTR = length;
    DMA1->INTFCR = H4V1_STAGE5_DMA_GL2 | H4V1_STAGE5_DMA_GL3;
    start = h4v1_stage5_cycle_now();

    if(h4v1_stage5_begin(result, command, before) != 0 ||
       ((address_bytes > 0u) &&
        (h4v1_stage5_send(result, address0, before) != 0)) ||
       ((address_bytes > 1u) &&
        (h4v1_stage5_send(result, address1, before) != 0)) ||
       ((address_bytes > 2u) &&
        (h4v1_stage5_send(result, address2, before) != 0)))
    {
        h4v1_stage5_deselect();
        *cycles_out = h4v1_stage5_cycle_now() - start;
        return -1;
    }
    SPI1->CTLR2 |= H4V1_STAGE5_SPI_RXDMAEN | H4V1_STAGE5_SPI_TXDMAEN;
    h4v1_stage5_fence();
    DMA1_Channel2->CFGR |= H4V1_STAGE5_DMA_EN;
    DMA1_Channel3->CFGR |= H4V1_STAGE5_DMA_EN;

    while((DMA1->INTFR & H4V1_STAGE5_DMA_DONE_MASK) !=
          H4V1_STAGE5_DMA_DONE_MASK)
    {
        if((DMA1->INTFR & H4V1_STAGE5_DMA_ERROR_MASK) != 0u)
        {
            result->dma_error_count++;
            goto dma_failed;
        }
        if((uint32_t)(h4v1_stage5_cycle_now() - start) >=
           H4V1_STAGE5_DMA_TIMEOUT)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    while((SPI1->STATR & H4V1_STAGE5_SPI_BSY) != 0u)
    {
        if((uint32_t)(h4v1_stage5_cycle_now() - start) >=
           H4V1_STAGE5_DMA_TIMEOUT)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    if((DMA1->INTFR & H4V1_STAGE5_DMA_ERROR_MASK) != 0u)
    {
        result->dma_error_count++;
        goto dma_failed;
    }
    if(h4v1_stage5_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
    h4v1_stage5_deselect();
    DMA1->INTFCR = H4V1_STAGE5_DMA_GL2 | H4V1_STAGE5_DMA_GL3;
    h4v1_stage5_fence();
    *cycles_out = h4v1_stage5_cycle_now() - start;
    return 0;

dma_failed:
    if(h4v1_stage5_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
    h4v1_stage5_deselect();
    DMA1->INTFCR = H4V1_STAGE5_DMA_GL2 | H4V1_STAGE5_DMA_GL3;
    *cycles_out = h4v1_stage5_cycle_now() - start;
    return -1;

dma_stuck:
    h4v1_stage5_deselect();
    DMA1->INTFCR = H4V1_STAGE5_DMA_GL2 | H4V1_STAGE5_DMA_GL3;
    *cycles_out = h4v1_stage5_cycle_now() - start;
    return -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_read_cache_dma(h4v1_stage5_result_t *result,
                           uint8_t *data, uint32_t *cycles_out)
{
    *(volatile uint8_t *)(uintptr_t)H4V1_STAGE5_AUX_ADDR = 0u;
    return h4v1_stage5_dma_payload(
        result, H4V1_STAGE5_CMD_READ_CACHE, 3u, 0u, 0u, 0u,
        (uint32_t)(uintptr_t)data, 1u,
        H4V1_STAGE5_AUX_ADDR, 0u,
        H4V1_STAGE5_PAGE_BYTES, cycles_out);
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_program_load_dma(h4v1_stage5_result_t *result,
                             const uint8_t *data, uint32_t *cycles_out)
{
    return h4v1_stage5_dma_payload(
        result, H4V1_STAGE5_CMD_PROGRAM_LOAD, 2u, 0u, 0u, 0u,
        H4V1_STAGE5_AUX_ADDR, 0u,
        (uint32_t)(uintptr_t)data, 1u,
        H4V1_STAGE5_PAGE_BYTES, cycles_out);
}

int H4V1_POSTPASS_TEXT h4v1_stage5_write_enable(h4v1_stage5_result_t *result)
{
    uint8_t status = 0xFFu;

    if(h4v1_stage5_command_only(result, H4V1_STAGE5_CMD_WRITE_ENABLE) != 0 ||
       h4v1_stage5_get_feature(result,
                               H4V1_STAGE5_FEATURE_STATUS,
                               &status) != 0)
    {
        return -1;
    }
    result->status_last = status;
    return ((status & H4V1_STAGE5_STATUS_WEL) != 0u) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT h4v1_stage5_write_disable(h4v1_stage5_result_t *result)
{
    uint8_t status = 0xFFu;

    if(h4v1_stage5_command_only(result, H4V1_STAGE5_CMD_WRITE_DISABLE) != 0 ||
       h4v1_stage5_get_feature(result,
                               H4V1_STAGE5_FEATURE_STATUS,
                               &status) != 0)
    {
        return -1;
    }
    result->status_last = status;
    return ((status & H4V1_STAGE5_STATUS_WEL) == 0u) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_erase(h4v1_stage5_result_t *result, uint32_t row,
                  uint32_t *cycles_out)
{
    uint32_t before;
    uint32_t start = h4v1_stage5_cycle_now();
    uint8_t status = 0xFFu;
    int ready;

    if(row != H4V1_STAGE5_ROW)
    {
        result->illegal_command_count++;
        *cycles_out = 0u;
        return -4;
    }

    if(h4v1_stage5_write_enable(result) != 0)
    {
        *cycles_out = h4v1_stage5_cycle_now() - start;
        return -1;
    }
    before = result->spi_timeout_count;
    if(h4v1_stage5_begin(result, H4V1_STAGE5_CMD_BLOCK_ERASE, before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)(row >> 16), before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)(row >> 8), before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)row, before) != 0)
    {
        h4v1_stage5_deselect();
        *cycles_out = h4v1_stage5_cycle_now() - start;
        return -1;
    }
    h4v1_stage5_deselect();
    ready = h4v1_stage5_wait_ready(result, &status);
    *cycles_out = h4v1_stage5_cycle_now() - start;
    if(ready != 0)
    {
        return (ready == -2) ? -2 : -1;
    }
    return ((status & H4V1_STAGE5_STATUS_E_FAIL) == 0u) ? 0 : -3;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_program_execute(h4v1_stage5_result_t *result, uint32_t row,
                            uint32_t load_cycles, uint32_t *cycles_out)
{
    uint32_t before;
    uint32_t start = h4v1_stage5_cycle_now();
    uint8_t status = 0xFFu;
    int ready;

    if(row != H4V1_STAGE5_ROW)
    {
        result->illegal_command_count++;
        *cycles_out = load_cycles;
        return -4;
    }

    /* Required ordering: 02 PROGRAM LOAD completed before this 06 WREN. */
    if(h4v1_stage5_write_enable(result) != 0)
    {
        *cycles_out = load_cycles + (h4v1_stage5_cycle_now() - start);
        return -1;
    }
    before = result->spi_timeout_count;
    if(h4v1_stage5_begin(result,
                         H4V1_STAGE5_CMD_PROGRAM_EXECUTE,
                         before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)(row >> 16), before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)(row >> 8), before) != 0 ||
       h4v1_stage5_send(result, (uint8_t)row, before) != 0)
    {
        h4v1_stage5_deselect();
        *cycles_out = load_cycles + (h4v1_stage5_cycle_now() - start);
        return -1;
    }
    h4v1_stage5_deselect();
    ready = h4v1_stage5_wait_ready(result, &status);
    *cycles_out = load_cycles + (h4v1_stage5_cycle_now() - start);
    if(ready != 0)
    {
        return (ready == -2) ? -2 : -1;
    }
    return ((status & H4V1_STAGE5_STATUS_P_FAIL) == 0u) ? 0 : -3;
}

uint8_t H4V1_POSTPASS_TEXT h4v1_stage5_pattern_byte(uint32_t index)
{
    uint32_t value = (index * 73u) ^ (index >> 3) ^
                     (index * index * 3u) ^ 0xA5u;

    value ^= value >> 8;
    return (uint8_t)value;
}

uint32_t H4V1_POSTPASS_TEXT
h4v1_stage5_fill_pattern(uint8_t *data)
{
    uint32_t index;
    uint32_t hash = H4V1_STAGE5_FNV_OFFSET;

    for(index = 0u; index < H4V1_STAGE5_PAGE_BYTES; ++index)
    {
        uint8_t value = h4v1_stage5_pattern_byte(index);

        *(volatile uint8_t *)&data[index] = value;
        hash = (hash ^ value) * H4V1_STAGE5_FNV_PRIME;
    }
    h4v1_stage5_fence();
    return hash;
}

void H4V1_POSTPASS_TEXT
h4v1_stage5_poison(uint8_t *data, uint8_t value)
{
    uint32_t index;

    for(index = 0u; index < H4V1_STAGE5_PAGE_BYTES; ++index)
    {
        *(volatile uint8_t *)&data[index] = value;
    }
    h4v1_stage5_fence();
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_validate_pattern(h4v1_stage5_result_t *result,
                             const uint8_t *data, uint32_t *hash_out)
{
    uint32_t index;
    uint32_t hash = H4V1_STAGE5_FNV_OFFSET;

    for(index = 0u; index < H4V1_STAGE5_PAGE_BYTES; ++index)
    {
        uint8_t value = *(volatile const uint8_t *)&data[index];
        uint8_t expected = h4v1_stage5_pattern_byte(index);

        hash = (hash ^ value) * H4V1_STAGE5_FNV_PRIME;
        if(value != expected)
        {
            result->first_bad_offset = index;
            result->first_bad_value = value;
            *hash_out = hash;
            return -1;
        }
    }
    *hash_out = hash;
    return (hash == result->expected_hash) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_validate_erased(h4v1_stage5_result_t *result,
                            const uint8_t *data, uint32_t *hash_out)
{
    uint32_t index;
    uint32_t hash = H4V1_STAGE5_FNV_OFFSET;

    for(index = 0u; index < H4V1_STAGE5_PAGE_BYTES; ++index)
    {
        uint8_t value = *(volatile const uint8_t *)&data[index];

        hash = (hash ^ value) * H4V1_STAGE5_FNV_PRIME;
        if(value != 0xFFu)
        {
            result->first_bad_offset = index;
            result->first_bad_value = value;
            *hash_out = hash;
            return -1;
        }
    }
    *hash_out = hash;
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_read_data(h4v1_stage5_result_t *result, uint8_t *data,
                      uint32_t *hash_out, uint8_t expect_pattern,
                      uint8_t *marker_out, uint32_t *cycles_out)
{
    uint32_t load_cycles = 0u;
    uint32_t cache_cycles = 0u;
    uint32_t start = h4v1_stage5_cycle_now();
    int operation;

    operation = h4v1_stage5_load_page(result, H4V1_STAGE5_ROW,
                                       &load_cycles);
    if(operation != 0)
    {
        *cycles_out = h4v1_stage5_cycle_now() - start;
        return operation;
    }
    operation = h4v1_stage5_read_cache_dma(result, data, &cache_cycles);
    if(operation != 0)
    {
        *cycles_out = h4v1_stage5_cycle_now() - start;
        return (operation == -2) ? -4 : -1;
    }
    if(expect_pattern != 0u)
    {
        operation = h4v1_stage5_validate_pattern(result, data, hash_out);
    }
    else
    {
        operation = h4v1_stage5_validate_erased(result, data, hash_out);
    }
    if((operation == 0) && (marker_out != 0))
    {
        operation = h4v1_stage5_read_cache_cpu(
            result, H4V1_STAGE5_BAD_MARK_COLUMN, marker_out, 1u);
    }
    *cycles_out = h4v1_stage5_cycle_now() - start;
    (void)load_cycles;
    (void)cache_cycles;
    return (operation == 0) ? 0 : -5;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_read_marker(h4v1_stage5_result_t *result, uint8_t *marker)
{
    uint32_t cycles = 0u;
    int operation = h4v1_stage5_load_page(result,
                                           H4V1_STAGE5_ROW,
                                           &cycles);

    if(operation != 0)
    {
        return operation;
    }
    return (h4v1_stage5_read_cache_cpu(result,
                                       H4V1_STAGE5_BAD_MARK_COLUMN,
                                       marker, 1u) == 0) ? 0 : -1;
}

void H4V1_POSTPASS_TEXT h4v1_stage5_apply_50mhz(void)
{
    uint16_t ctlr1 = SPI1->CTLR1;
    uint16_t hscr = SPI1->HSCR;

    SPI1->CTLR1 = ctlr1 & (uint16_t)~H4V1_STAGE5_SPI_SPE;
    h4v1_stage5_drain_rx();
    hscr &= (uint16_t)~(H4V1_STAGE5_SPI_HSRXEN |
                        H4V1_STAGE5_SPI_HSRXEN2);
    SPI1->HSCR = hscr;
    ctlr1 &= (uint16_t)~H4V1_STAGE5_SPI_BR_MASK;
    SPI1->CTLR1 = ctlr1 | H4V1_STAGE5_SPI_SPE;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_save_and_configure(h4v1_stage5_saved_t *saved)
{
    saved->dma_clock_was_enabled = RCC->HBPCENR & H4V1_STAGE5_RCC_DMA1;
    RCC->HBPCENR |= H4V1_STAGE5_RCC_DMA1;
    h4v1_stage5_fence();
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
    if(((saved->dma2_cfgr | saved->dma3_cfgr) & H4V1_STAGE5_DMA_EN) != 0u)
    {
        return -1;
    }
    h4v1_stage5_dma_stop();
    DMA1_Channel2->CNTR = 0u;
    DMA1_Channel2->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel2->MADDR = H4V1_STAGE5_DATA_ADDR;
    DMA1_Channel2->M1ADDR = 0u;
    DMA1_Channel3->CNTR = 0u;
    DMA1_Channel3->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel3->MADDR = H4V1_STAGE5_AUX_ADDR;
    DMA1_Channel3->M1ADDR = 0u;
    DMAMUX->CFGR0_3 =
        (saved->dmamux_cfgr0_3 &
         ~((H4V1_STAGE5_DMAMUX_FIELD_MASK << H4V1_STAGE5_DMAMUX_CH2_SHIFT) |
           (H4V1_STAGE5_DMAMUX_FIELD_MASK << H4V1_STAGE5_DMAMUX_CH3_SHIFT))) |
        ((H4V1_STAGE5_DMA_REQUEST_SPI1_RX - 1u) <<
         H4V1_STAGE5_DMAMUX_CH2_SHIFT) |
        ((H4V1_STAGE5_DMA_REQUEST_SPI1_TX - 1u) <<
         H4V1_STAGE5_DMAMUX_CH3_SHIFT);
    *(volatile uint8_t *)(uintptr_t)H4V1_STAGE5_AUX_ADDR = 0u;
    DMA1->INTFCR = H4V1_STAGE5_DMA_GL2 | H4V1_STAGE5_DMA_GL3;
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_restore(const h4v1_stage5_saved_t *saved)
{
    h4v1_stage5_result_t stop_result = {0};

    if(((saved->dma2_cfgr | saved->dma3_cfgr) & H4V1_STAGE5_DMA_EN) != 0u)
    {
        if(saved->dma_clock_was_enabled == 0u)
        {
            RCC->HBPCENR &= ~H4V1_STAGE5_RCC_DMA1;
        }
        return 0;
    }
    if(h4v1_stage5_dma_stop_wait(&stop_result) != 0)
    {
        h4v1_stage5_deselect();
        return -1;
    }
    h4v1_stage5_deselect();
    if(h4v1_stage5_spi_wait_idle(&stop_result) != 0)
    {
        h4v1_stage5_drain_rx();
        return -1;
    }
    h4v1_stage5_drain_rx();
    DMA1->INTFCR = H4V1_STAGE5_DMA_GL2 | H4V1_STAGE5_DMA_GL3;
    DMA1_Channel2->CFGR = saved->dma2_cfgr & ~H4V1_STAGE5_DMA_EN;
    DMA1_Channel2->CNTR = saved->dma2_cntr;
    DMA1_Channel2->PADDR = saved->dma2_paddr;
    DMA1_Channel2->MADDR = saved->dma2_maddr;
    DMA1_Channel2->M1ADDR = saved->dma2_m1addr;
    DMA1_Channel3->CFGR = saved->dma3_cfgr & ~H4V1_STAGE5_DMA_EN;
    DMA1_Channel3->CNTR = saved->dma3_cntr;
    DMA1_Channel3->PADDR = saved->dma3_paddr;
    DMA1_Channel3->MADDR = saved->dma3_maddr;
    DMA1_Channel3->M1ADDR = saved->dma3_m1addr;
    DMAMUX->CFGR0_3 = saved->dmamux_cfgr0_3;
    SPI1->CTLR1 &= (uint16_t)~H4V1_STAGE5_SPI_SPE;
    h4v1_stage5_drain_rx();
    SPI1->HSCR = saved->spi_hscr;
    SPI1->CTLR2 = saved->spi_ctlr2;
    SPI1->CTLR1 = saved->spi_ctlr1;
    if(saved->dma_clock_was_enabled == 0u)
    {
        RCC->HBPCENR &= ~H4V1_STAGE5_RCC_DMA1;
    }
    h4v1_stage5_fence();
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage5_cleanup_flash(h4v1_stage5_result_t *result)
{
    uint8_t value = 0xFFu;
    uint8_t status = 0xFFu;
    int failed = 0;

    if(result->flash_owned == 0u)
    {
        return 0;
    }
    if(h4v1_stage5_dma_stop_wait(result) != 0)
    {
        h4v1_stage5_deselect();
        return -1;
    }
    h4v1_stage5_deselect();
    if(h4v1_stage5_spi_wait_idle(result) != 0)
    {
        h4v1_stage5_drain_rx();
        return -1;
    }
    h4v1_stage5_drain_rx();
    if((result->destructive_started != 0u) &&
       (result->final_erase_done == 0u))
    {
        uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE5_DATA_ADDR;
        uint8_t marker = 0x00u;
        uint32_t hash = 0u;
        uint32_t cycles = 0u;

        if(h4v1_stage5_erase(result, H4V1_STAGE5_ROW, &cycles) != 0)
        {
            failed = 1;
        }
        else
        {
            result->erase_final_cycles = cycles;
            h4v1_stage5_poison(data, 0xA5u);
            if(h4v1_stage5_read_data(result, data, &hash, 0u,
                                     &marker, &cycles) != 0 ||
               marker != 0xFFu)
            {
                failed = 1;
            }
            else
            {
                result->erased_hash_final = hash;
                result->marker_after = marker;
                result->final_erase_done = 1u;
            }
        }
    }
    if(h4v1_stage5_write_disable(result) != 0)
    {
        failed = 1;
    }
    if(result->features_saved != 0u)
    {
        if(h4v1_stage5_set_a0(result, result->a0_saved) != 0 ||
           h4v1_stage5_get_feature(result,
                                   H4V1_STAGE5_FEATURE_PROTECTION,
                                   &value) != 0 ||
           value != result->a0_saved)
        {
            failed = 1;
        }
        result->a0_restored = value;
        value = 0xFFu;
        if(h4v1_stage5_get_feature(result,
                                   H4V1_STAGE5_FEATURE_CONFIG,
                                   &value) != 0 ||
           value != result->b0_saved)
        {
            failed = 1;
        }
        result->b0_readback = value;
    }
    if(h4v1_stage5_get_feature(result,
                               H4V1_STAGE5_FEATURE_STATUS,
                               &status) != 0 ||
       (status & H4V1_STAGE5_STATUS_WEL) != 0u)
    {
        failed = 1;
    }
    result->status_last = status;
    return (failed == 0) ? 0 : -1;
}

int H4V1_POSTPASS_ENTRY h4v1_flash_postpass_stage5_run(void)
{
    h4v1_stage5_saved_t saved;
    h4v1_stage5_result_t run = {0};
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE5_DATA_ADDR;
    uint32_t load_cycles = 0u;
    uint32_t cleanup_failed = 0u;
    int primary = H4V1_FLASH_POSTPASS_STAGE5_OK;
    int result = H4V1_FLASH_POSTPASS_STAGE5_OK;
    char line[600];

    run.first_bad_offset = 0xFFFFFFFFu;
    run.status2_last = 0xFFu;
    run.marker_before = 0x00u;
    run.marker_after = 0x00u;
    run.a0_temp_readback = 0xFFu;
    run.a0_restored = 0xFFu;
    run.b0_readback = 0xFFu;
    h4v1_postpass_stage1_log(h4v1_stage5_start_text);

    if(h4v1_stage5_save_and_configure(&saved) != 0)
    {
        primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_DMA_BUSY;
    }
    else
    {
        run.flash_owned = 1u;
        h4v1_stage5_apply_50mhz();
        if(h4v1_stage5_read_id(&run) != 0)
        {
            primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_PROBE;
        }
        else if(h4v1_stage5_get_feature(&run,
                                        H4V1_STAGE5_FEATURE_PROTECTION,
                                        &run.a0_saved) != 0 ||
                h4v1_stage5_get_feature(&run,
                                        H4V1_STAGE5_FEATURE_CONFIG,
                                        &run.b0_saved) != 0)
        {
            primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_SPI;
        }
        else
        {
            run.features_saved = 1u;
            run.a0_restored = run.a0_saved;
            run.b0_readback = run.b0_saved;
            if(run.a0_saved != H4V1_STAGE5_A0_EXPECTED_LOCKED ||
               (run.a0_saved & H4V1_STAGE5_A0_BRWD) != 0u ||
               (run.b0_saved & H4V1_STAGE5_B0_ECC_ENABLE) == 0u ||
               (run.b0_saved & H4V1_STAGE5_B0_BPL) != 0u ||
               (run.b0_saved & H4V1_STAGE5_B0_OTP_ENABLE) != 0u ||
               (run.b0_saved & H4V1_STAGE5_B0_QUAD_ENABLE) != 0u)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_FEATURE;
            }
        }

        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            int marker_result = h4v1_stage5_read_marker(&run,
                                                        &run.marker_before);
            if(marker_result == -2)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_READY;
            }
            else if(marker_result == -3)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_ECC;
            }
            else if(marker_result != 0)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_SPI;
            }
            else if(run.marker_before != 0xFFu)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_MARKER;
            }
        }
        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            uint8_t a0 = 0xFFu;

            if(h4v1_stage5_set_a0(&run, H4V1_STAGE5_A0_TEMP) != 0 ||
               h4v1_stage5_get_feature(&run,
                                       H4V1_STAGE5_FEATURE_PROTECTION,
                                       &a0) != 0 ||
               a0 != H4V1_STAGE5_A0_TEMP)
            {
                run.a0_temp_readback = a0;
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_FEATURE;
            }
            else
            {
                run.a0_temp_readback = a0;
            }
        }
        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            int erase = h4v1_stage5_erase(&run, H4V1_STAGE5_ROW,
                                           &run.erase_initial_cycles);

            run.destructive_started = 1u;
            if(erase == -2)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_READY;
            }
            else if(erase != 0)
            {
                primary = (run.spi_timeout_count != 0u) ?
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_SPI :
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_ERASE;
            }
        }
        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            h4v1_stage5_poison(data, 0x00u);
            if(h4v1_stage5_read_data(&run, data,
                                     &run.erased_hash_initial, 0u,
                                     0, &run.read_cycles) != 0)
            {
                primary = (run.ecc_worst_bits == 9u) ?
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_ECC :
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_VERIFY;
            }
        }
        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            int program;

            run.expected_hash = h4v1_stage5_fill_pattern(data);
            program = h4v1_stage5_program_load_dma(&run, data, &load_cycles);
            if(program == -2)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_DMA_QUIESCE;
            }
            else if(program != 0)
            {
                primary = (run.spi_timeout_count != 0u) ?
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_SPI :
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_DMA;
            }
            else
            {
                program = h4v1_stage5_program_execute(
                    &run, H4V1_STAGE5_ROW, load_cycles, &run.program_cycles);
                if(program == -2)
                {
                    primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_READY;
                }
                else if(program != 0)
                {
                    primary = (run.spi_timeout_count != 0u) ?
                        H4V1_FLASH_POSTPASS_STAGE5_ERR_SPI :
                        H4V1_FLASH_POSTPASS_STAGE5_ERR_PROGRAM;
                }
            }
        }
        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            h4v1_stage5_poison(data, 0x5Au);
            if(h4v1_stage5_read_data(&run, data, &run.programmed_hash,
                                     1u, 0, &run.read_cycles) != 0 ||
               run.programmed_hash != run.expected_hash)
            {
                primary = (run.ecc_worst_bits == 9u) ?
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_ECC :
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_VERIFY;
            }
        }
        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            int erase = h4v1_stage5_erase(&run, H4V1_STAGE5_ROW,
                                           &run.erase_final_cycles);
            if(erase == -2)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_READY;
            }
            else if(erase != 0)
            {
                primary = (run.spi_timeout_count != 0u) ?
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_SPI :
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_ERASE;
            }
        }
        if(primary == H4V1_FLASH_POSTPASS_STAGE5_OK)
        {
            h4v1_stage5_poison(data, 0xA5u);
            if(h4v1_stage5_read_data(&run, data,
                                     &run.erased_hash_final, 0u,
                                     &run.marker_after,
                                     &run.read_cycles) != 0)
            {
                primary = (run.ecc_worst_bits == 9u) ?
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_ECC :
                    H4V1_FLASH_POSTPASS_STAGE5_ERR_VERIFY;
            }
            else if(run.marker_after != 0xFFu)
            {
                primary = H4V1_FLASH_POSTPASS_STAGE5_ERR_MARKER;
            }
            else
            {
                run.final_erase_done = 1u;
            }
        }
    }

    result = primary;
    if(h4v1_stage5_cleanup_flash(&run) != 0)
    {
        cleanup_failed = 1u;
        result = H4V1_FLASH_POSTPASS_STAGE5_ERR_CLEANUP;
    }
    if(h4v1_stage5_restore(&saved) != 0)
    {
        cleanup_failed = 1u;
        result = H4V1_FLASH_POSTPASS_STAGE5_ERR_CLEANUP;
    }
    if(run.illegal_command_count != 0u)
    {
        result = H4V1_FLASH_POSTPASS_STAGE5_ERR_ILLEGAL_COMMAND;
    }

    if(result == H4V1_FLASH_POSTPASS_STAGE5_OK)
    {
        (void)rt_snprintf(line, sizeof(line), h4v1_stage5_pass_format,
                          (unsigned int)H4V1_STAGE5_BLOCK,
                          (unsigned int)H4V1_STAGE5_ROW,
                          (unsigned int)run.marker_before,
                          (unsigned int)run.marker_after,
                          (unsigned int)run.mid,
                          (unsigned int)run.did,
                          (unsigned int)run.a0_saved,
                          (unsigned int)run.a0_temp_readback,
                          (unsigned int)run.a0_restored,
                          (unsigned int)run.b0_saved,
                          (unsigned int)run.b0_readback,
                          (unsigned int)run.expected_hash,
                          (unsigned int)run.programmed_hash,
                          (unsigned int)run.erased_hash_initial,
                          (unsigned int)run.erased_hash_final,
                          (unsigned int)run.ecc_worst_bits,
                          (unsigned int)run.ready_polls,
                          (unsigned int)run.erase_initial_cycles,
                          (unsigned int)run.program_cycles,
                          (unsigned int)run.read_cycles,
                          (unsigned int)run.erase_final_cycles,
                          (unsigned int)run.spi_timeout_count,
                          (unsigned int)run.ready_timeout_count,
                          (unsigned int)run.dma_timeout_count,
                          (unsigned int)run.dma_error_count,
                          (unsigned int)run.illegal_command_count);
    }
    else
    {
        (void)rt_snprintf(line, sizeof(line), h4v1_stage5_fail_format,
                          result,
                          primary,
                          (unsigned int)H4V1_STAGE5_BLOCK,
                          (unsigned int)H4V1_STAGE5_ROW,
                          (unsigned int)run.marker_before,
                          (unsigned int)run.marker_after,
                          (unsigned int)run.mid,
                          (unsigned int)run.did,
                          (unsigned int)run.a0_saved,
                          (unsigned int)run.a0_temp_readback,
                          (unsigned int)run.a0_restored,
                          (unsigned int)run.b0_saved,
                          (unsigned int)run.b0_readback,
                          (unsigned int)run.status_last,
                          (unsigned int)run.status2_last,
                          (unsigned int)run.first_bad_offset,
                          (unsigned int)run.first_bad_value,
                          (unsigned int)run.expected_hash,
                          (unsigned int)run.programmed_hash,
                          (unsigned int)run.spi_timeout_count,
                          (unsigned int)run.ready_timeout_count,
                          (unsigned int)run.dma_timeout_count,
                          (unsigned int)run.dma_error_count,
                          (unsigned int)run.illegal_command_count,
                          (unsigned int)cleanup_failed,
                          (unsigned int)run.final_erase_done,
                          (unsigned int)((run.destructive_started != 0u) &&
                                         (run.final_erase_done == 0u)));
    }
    h4v1_postpass_stage1_log(line);
    return result;
}
