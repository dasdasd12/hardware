/*
 * Read-only H4V1 cold-boot source provider.
 *
 * This object deliberately knows nothing about the private video-config type.
 * A post-link hook substitutes the two qualified USB receive call sites with
 * the providers below.  The qualified path therefore retains SDRAM init,
 * DMA2 writes, upload CRC, DMA2 readback, decoder and LTDC sequencing.
 *
 * NAND command whitelist: READ ID, GET FEATURE, PAGE READ and READ CACHE.
 * There is no persistent-state-changing NAND operation in this translation
 * unit.  Manifest commit is checked before descriptor/map data is trusted.
 */
#include "h4v1_flash_coldboot_core.h"

#include <stdint.h>

#include <rtthread.h>

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"

#if H4V1_FLASH_COLDBOOT_PRODUCT
#define H4CB_TEXT \
    __attribute__((noinline))
#define H4CB_ENTRY \
    __attribute__((noinline, used))
#define H4CB_RODATA \
    __attribute__((aligned(4), used))
#define H4CB_BSS \
    __attribute__((aligned(8), used))
#else
#define H4CB_TEXT \
    __attribute__((section(".h4v1_coldboot.text"), noinline))
#define H4CB_ENTRY \
    __attribute__((section(".h4v1_coldboot.text"), noinline, used))
#define H4CB_RODATA \
    __attribute__((section(".h4v1_coldboot.rodata"), aligned(4), used))
#define H4CB_BSS \
    __attribute__((section(".h4v1_coldboot.bss"), aligned(8), used))
#endif

/* Complete NAND opcode whitelist. */
#define H4CB_CMD_READ_CACHE             0x03u
#define H4CB_CMD_GET_FEATURE            0x0Fu
#define H4CB_CMD_PAGE_READ              0x13u
#define H4CB_CMD_READ_ID                0x9Fu

#define H4CB_FEATURE_PROTECTION         0xA0u
#define H4CB_FEATURE_CONFIG             0xB0u
#define H4CB_FEATURE_STATUS             0xC0u
#define H4CB_FEATURE_STATUS2            0xF0u
#define H4CB_EXPECTED_MID               0xC8u
#define H4CB_EXPECTED_DID               0x91u
#define H4CB_EXPECTED_A0                0x38u
#define H4CB_EXPECTED_B0                0x10u

#define H4CB_STATUS_OIP                 0x01u
#define H4CB_STATUS_WEL                 0x02u
#define H4CB_STATUS_E_FAIL              0x04u
#define H4CB_STATUS_P_FAIL              0x08u
#define H4CB_STATUS_ECC_MASK            0x30u
#define H4CB_STATUS_ECC_CLEAN           0x00u
#define H4CB_STATUS_ECC_CORRECTED       0x10u
#define H4CB_STATUS_ECC_FAILED          0x20u
#define H4CB_STATUS_ECC_8_BITS          0x30u
#define H4CB_STATUS2_ECCSE_MASK         0x30u

#define H4CB_SPI_SPE                    0x0040u
#define H4CB_SPI_MASTER                 0x0004u
#define H4CB_SPI_CPOL                   0x0002u
#define H4CB_SPI_CPHA                   0x0001u
#define H4CB_SPI_SSM                    0x0200u
#define H4CB_SPI_SSI                    0x0100u
#define H4CB_SPI_RXNE                   0x0001u
#define H4CB_SPI_TXE                    0x0002u
#define H4CB_SPI_BSY                    0x0080u
#define H4CB_SPI_RXDMAEN                0x0001u
#define H4CB_SPI_TXDMAEN                0x0002u
#define H4CB_SPI_HSRXEN                 0x0001u
#define H4CB_SPI_HSRXEN2                0x0004u
#define H4CB_SPI_MODE_SELECT_MASK       0xF7FFu

#define H4CB_DMA_EN                     0x0001u
#define H4CB_DMA_DIR                    0x0010u
#define H4CB_DMA_MINC                   0x0080u
#define H4CB_DMA_PRIORITY_VERY_HIGH     0x3000u
#define H4CB_DMA_GL2                    0x00000010u
#define H4CB_DMA_TC2                    0x00000020u
#define H4CB_DMA_TE2                    0x00000080u
#define H4CB_DMA_GL3                    0x00000100u
#define H4CB_DMA_TC3                    0x00000200u
#define H4CB_DMA_TE3                    0x00000800u
#define H4CB_DMA_DONE_MASK              (H4CB_DMA_TC2 | H4CB_DMA_TC3)
#define H4CB_DMA_ERROR_MASK             (H4CB_DMA_TE2 | H4CB_DMA_TE3)
#define H4CB_DMAMUX_CH2_SHIFT           8u
#define H4CB_DMAMUX_CH3_SHIFT           16u
#define H4CB_DMAMUX_FIELD_MASK          0x7Fu
#define H4CB_DMA_REQUEST_SPI1_TX        63u
#define H4CB_DMA_REQUEST_SPI1_RX        64u

#define H4CB_SPI_TIMEOUT                1000000u
#define H4CB_DMA_TIMEOUT                4000000u
#define H4CB_READY_TIMEOUT              80000000u
#define H4CB_PAGE_BYTES                 2048u
#define H4CB_PAGES_PER_BLOCK            64u
#define H4CB_STAGE_BYTES                16384u
#define H4CB_STAGE_ADDR                 0x20174000u
#define H4CB_STAGE_END                  (H4CB_STAGE_ADDR + H4CB_STAGE_BYTES)
#define H4CB_BAD_MARK_COLUMN            2048u
#if H4V1_FLASH_COLDBOOT_PRODUCT
/* UI/LTDC ownership is suspended during preload; stay below its 16 KiB page. */
#define H4CB_DMA_TX_AUX_ADDR            0x20173FFCu
#define H4CB_DMA_TX_AUX_SAFE_FIRST      0x20173FFCu
#define H4CB_DMA_TX_AUX_SAFE_END        0x20174000u
#else
#define H4CB_DMA_TX_AUX_ADDR            0x20178040u
#define H4CB_DMA_TX_AUX_SAFE_FIRST      0x20178040u
#define H4CB_DMA_TX_AUX_SAFE_END        0x20178200u
#endif
#define H4CB_MANIFEST_BLOCK             768u
#define H4CB_PAYLOAD_FIRST_BLOCK        769u
#define H4CB_PAYLOAD_LAST_BLOCK         1014u
#define H4CB_RESERVED_STAGE5_BLOCK      1015u
#define H4CB_L8_FIRST_BLOCK             1016u
#define H4CB_REQUIRED_PAGES             15120u
#define H4CB_LAST_BLOCK_PAGES           16u

#define H4CB_DESCRIPTOR_MAGIC           0x36493448u
#define H4CB_COMMIT_MAGIC               0x36433448u
#define H4CB_MANIFEST_VERSION           1u
#define H4CB_GENERATION                 1u
#define H4CB_DESCRIPTOR_MAP_OFFSET      128u
#define H4CB_DESCRIPTOR_CRC_OFFSET      88u
#define H4CB_DESCRIPTOR_BLOCK_CRC_OFFSET 604u
#define H4CB_COMMIT_CRC_OFFSET          28u

#define H4CB_CONTAINER_BYTES            30933600u
#define H4CB_CONTAINER_CRC              0x4097F39Au
#define H4CB_WIDTH                      800u
#define H4CB_HEIGHT                     480u
#define H4CB_FRAME_BYTES                768000u
#define H4CB_PIXEL_FORMAT               1u
#define H4CB_FLAGS                      0x0000000Eu
#define H4CB_DATA_OFFSET                4096u
#define H4CB_RAW_STREAM_CRC             0x15F11FC2u
#define H4CB_INDEX_CRC                  0x6E4E0785u
#define H4CB_HEADER_CRC                 0x88557C24u

#define H4CB_CONTEXT_MAGIC              0x31424348u
#define H4CB_IWDG_FEED_KEY              0xAAAAu

#define H4CB_FLASH_CS_PIN               GPIO_Pin_6
#define H4CB_FLASH_SCK_PIN              GPIO_Pin_7
#define H4CB_FLASH_MOSI_PIN             GPIO_Pin_8
#define H4CB_FLASH_MISO_PIN             GPIO_Pin_9
#define H4CB_GPIO_CFGLR_MASK            0xFF000000u
#define H4CB_GPIO_CFGHR_MASK            0x000000FFu
#define H4CB_GPIO_OUTDR_MASK            0x000003C0u
#define H4CB_GPIO_SPEED_MASK            0x000FF000u
#define H4CB_AFIO_AFLR_MASK             0xF0000000u
#define H4CB_AFIO_AFHR_MASK             0x000000FFu

#if (H4V1_FLASH_COLDBOOT_TRANSFER_BYTES % H4CB_STAGE_BYTES) != 0u
#error "Cold-boot transfer must contain whole qualified staging blocks"
#endif
#if (H4V1_FLASH_COLDBOOT_TRANSFER_BYTES / H4CB_PAGE_BYTES) != H4CB_REQUIRED_PAGES
#error "Cold-boot transfer/page geometry changed"
#endif
#if (H4V1_FLASH_COLDBOOT_MAP_BLOCKS * H4CB_PAGES_PER_BLOCK - H4CB_REQUIRED_PAGES) != 48u
#error "Cold-boot final logical block geometry changed"
#endif
#if H4CB_PAYLOAD_LAST_BLOCK >= H4CB_RESERVED_STAGE5_BLOCK
#error "Cold-boot payload may reach the Stage-5 scratch block"
#endif
#if H4CB_RESERVED_STAGE5_BLOCK >= H4CB_L8_FIRST_BLOCK
#error "Cold-boot Stage-5 reservation may reach L8"
#endif
#if (H4CB_DMA_TX_AUX_ADDR < H4CB_DMA_TX_AUX_SAFE_FIRST) || \
    ((H4CB_DMA_TX_AUX_ADDR + 4u) > H4CB_DMA_TX_AUX_SAFE_END) || \
    ((H4CB_DMA_TX_AUX_ADDR & 3u) != 0u)
#error "Cold-boot DMA TX byte escaped the qualified shared-SRAM gap"
#endif

typedef struct
{
    uint32_t rcc_hbpcenr;
    uint32_t rcc_hb2pcenr;
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
    uint32_t gpio_cfglr;
    uint32_t gpio_cfghr;
    uint32_t gpio_outdr;
    uint32_t gpio_speed;
    uint32_t afio_gpiof_aflr;
    uint32_t afio_gpiof_afhr;
    uint16_t spi_ctlr1;
    uint16_t spi_ctlr2;
    uint16_t spi_hscr;
    uint16_t spi_i2scfgr;
    uint16_t spi_crcr;
} h4cb_saved_t;

typedef struct
{
    uint32_t magic;
    uint32_t magic_inverse;
    h4v1_flash_coldboot_status_t public_status;
    uint32_t command_issued;
    uint32_t raw_disabled;
    uint32_t failure_logged;
    uint32_t manifest_logged;
    uint32_t load_logged;
    uint32_t play_logged;
    uint32_t owned;
    uint32_t running_crc;
    uint32_t running_block_crc;
    uint16_t block_map[H4V1_FLASH_COLDBOOT_MAP_BLOCKS];
    uint32_t block_crc[H4V1_FLASH_COLDBOOT_MAP_BLOCKS];
    h4cb_saved_t saved;
} h4cb_context_t;

static h4cb_context_t h4cb_context H4CB_BSS;

const char h4v1_flash_coldboot_banner[] H4CB_RODATA =
    "H417 FLASH H4V1 COLD BOOT v001 READONLY PLAY V01";
const char h4v1_flash_coldboot_manifest_pass_format[] H4CB_RODATA =
    "FLASH COLD BOOT MANIFEST PASS bytes=30965760 crc=e32a6c99 "
    "map=237 frames=165 fps=30 descriptor=%08x commit=%08x "
    "first=%u last=%u bad=%u ecc_worst=%u";
const char h4v1_flash_coldboot_load_pass_format[] H4CB_RODATA =
    "FLASH COLD BOOT LOAD PASS bytes=30965760 crc=e32a6c99 "
    "map=237 frames=165 fps=30 descriptor=%08x commit=%08x "
    "pages=15120 flash_crc=%08x ecc_worst=%u";
const char h4v1_flash_coldboot_play_pass_format[] H4CB_RODATA =
    "FLASH COLD BOOT PLAY PASS bytes=30965760 crc=e32a6c99 "
    "map=237 frames=165 fps=30 descriptor=%08x commit=%08x";
static const char h4cb_manifest_reject_format[] H4CB_RODATA =
    "FLASH COLD BOOT MANIFEST REJECT error=%d state=%u descriptor=%08x "
    "commit=%08x ecc_worst=%u";
static const char h4cb_load_reject_format[] H4CB_RODATA =
    "FLASH COLD BOOT LOAD REJECT error=%d bytes=%u pages=%u crc=%08x "
    "logical=%u block=%u page=%u ecc_worst=%u";
static const char h4cb_play_reject_format[] H4CB_RODATA =
    "FLASH COLD BOOT PLAY REJECT error=%d state=%u bytes=%u crc=%08x";
static const char h4cb_command[] H4CB_RODATA =
    "H4V1 30965760 e32a6c99";

static const uint32_t h4cb_crc_table[16] H4CB_RODATA =
{
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

#if H4V1_FLASH_COLDBOOT_PRODUCT
extern int h4v1_flash_coldboot_product_raw_disable(void);
#else
extern int ch32h417_usb_cdc_raw_rx_enable(uint8_t enable);
#endif

static uint32_t H4CB_TEXT h4cb_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static void H4CB_TEXT h4cb_fence(void)
{
    __asm volatile ("fence iorw, iorw" ::: "memory");
}

static void H4CB_TEXT h4cb_watchdog_feed(void)
{
#if H4V1_FLASH_COLDBOOT_PRODUCT
    /* Product mode never starts IWDG and does not touch its control key. */
#else
    IWDG->CTLR = H4CB_IWDG_FEED_KEY;
#endif
}

static uint16_t H4CB_TEXT h4cb_get16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t H4CB_TEXT h4cb_get32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t H4CB_TEXT h4cb_crc32_update(uint32_t previous,
                                             const uint8_t *data,
                                             uint32_t length)
{
    uint32_t crc = ~previous;

    while(length-- != 0u)
    {
        crc ^= *data++;
        crc = h4cb_crc_table[crc & 0x0Fu] ^ (crc >> 4);
        crc = h4cb_crc_table[crc & 0x0Fu] ^ (crc >> 4);
    }
    return ~crc;
}

static uint32_t H4CB_TEXT h4cb_crc32_zero_field(const uint8_t *data,
                                                 uint32_t length,
                                                 uint32_t field)
{
    uint8_t zeros[4];
    uint32_t crc;

    zeros[0] = 0u;
    zeros[1] = 0u;
    zeros[2] = 0u;
    zeros[3] = 0u;
    crc = h4cb_crc32_update(0u, data, field);
    crc = h4cb_crc32_update(crc, zeros, sizeof(zeros));
    return h4cb_crc32_update(crc, &data[field + 4u],
                             length - field - 4u);
}

static int H4CB_TEXT h4cb_all_ff(const uint8_t *data, uint32_t length)
{
    while(length-- != 0u)
    {
        if(*data++ != 0xFFu)
        {
            return 0;
        }
    }
    return 1;
}

static int H4CB_TEXT h4cb_context_valid(void)
{
    return (h4cb_context.magic == H4CB_CONTEXT_MAGIC) &&
           (h4cb_context.magic_inverse == ~H4CB_CONTEXT_MAGIC);
}

static void H4CB_TEXT h4cb_clear_context(void)
{
    volatile uint8_t *data = (volatile uint8_t *)&h4cb_context;
    uint32_t count = (uint32_t)sizeof(h4cb_context);

    while(count-- != 0u)
    {
        *data++ = 0u;
    }
    h4cb_context.public_status.logical_block = 0xFFFFFFFFu;
    h4cb_context.public_status.physical_block = 0xFFFFFFFFu;
    h4cb_context.public_status.page = 0xFFFFFFFFu;
    h4cb_context.magic_inverse = ~H4CB_CONTEXT_MAGIC;
    h4cb_fence();
    h4cb_context.magic = H4CB_CONTEXT_MAGIC;
    h4cb_fence();
}

static void H4CB_TEXT h4cb_select(void)
{
    GPIOF->BCR = H4CB_FLASH_CS_PIN;
}

static void H4CB_TEXT h4cb_deselect(void)
{
    GPIOF->BSHR = H4CB_FLASH_CS_PIN;
}

static void H4CB_TEXT h4cb_drain_rx(void)
{
    uint32_t guard = 16u;

    while((guard != 0u) && ((SPI1->STATR & H4CB_SPI_RXNE) != 0u))
    {
        (void)SPI1->DATAR;
        guard--;
    }
    (void)SPI1->STATR;
}

static uint8_t H4CB_TEXT h4cb_transfer(uint8_t tx)
{
    uint32_t timeout = H4CB_SPI_TIMEOUT;

    while(((SPI1->STATR & H4CB_SPI_TXE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        h4cb_context.public_status.spi_timeouts++;
        return 0xFFu;
    }
    SPI1->DATAR = tx;
    timeout = H4CB_SPI_TIMEOUT;
    while(((SPI1->STATR & H4CB_SPI_RXNE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        h4cb_context.public_status.spi_timeouts++;
        return 0xFFu;
    }
    return (uint8_t)SPI1->DATAR;
}

static int H4CB_TEXT h4cb_command_allowed(uint8_t command)
{
    return (command == H4CB_CMD_READ_ID) ||
           (command == H4CB_CMD_GET_FEATURE) ||
           (command == H4CB_CMD_PAGE_READ) ||
           (command == H4CB_CMD_READ_CACHE);
}

static int H4CB_TEXT h4cb_send(uint8_t value, uint32_t before)
{
    (void)h4cb_transfer(value);
    return (h4cb_context.public_status.spi_timeouts == before) ? 0 : -1;
}

static int H4CB_TEXT h4cb_begin(uint8_t command, uint32_t before)
{
    if(h4cb_command_allowed(command) == 0)
    {
        return -1;
    }
    h4cb_drain_rx();
    h4cb_select();
    if(h4cb_send(command, before) != 0)
    {
        h4cb_deselect();
        return -1;
    }
    return 0;
}

static int H4CB_TEXT h4cb_get_feature(uint8_t address, uint8_t *value)
{
    uint32_t before = h4cb_context.public_status.spi_timeouts;

    if((value == RT_NULL) ||
       (h4cb_begin(H4CB_CMD_GET_FEATURE, before) != 0) ||
       (h4cb_send(address, before) != 0))
    {
        h4cb_deselect();
        return -1;
    }
    *value = h4cb_transfer(0u);
    h4cb_deselect();
    return (h4cb_context.public_status.spi_timeouts == before) ? 0 : -1;
}

static int H4CB_TEXT h4cb_read_id(void)
{
    uint32_t before = h4cb_context.public_status.spi_timeouts;

    if((h4cb_begin(H4CB_CMD_READ_ID, before) != 0) ||
       (h4cb_send(0u, before) != 0))
    {
        h4cb_deselect();
        return -1;
    }
    h4cb_context.public_status.mid = h4cb_transfer(0u);
    h4cb_context.public_status.did = h4cb_transfer(0u);
    h4cb_deselect();
    if(h4cb_context.public_status.spi_timeouts != before)
    {
        return -1;
    }
    return ((h4cb_context.public_status.mid == H4CB_EXPECTED_MID) &&
            (h4cb_context.public_status.did == H4CB_EXPECTED_DID)) ? 0 : -2;
}

static int H4CB_TEXT h4cb_wait_ready(uint8_t *status_out)
{
    uint32_t start = h4cb_cycle_now();
    uint8_t status = 0xFFu;

    do
    {
        if(h4cb_get_feature(H4CB_FEATURE_STATUS, &status) != 0)
        {
            *status_out = status;
            return -1;
        }
        if((status & H4CB_STATUS_OIP) == 0u)
        {
            h4cb_context.public_status.c0 = status;
            *status_out = status;
            return 0;
        }
    } while((uint32_t)(h4cb_cycle_now() - start) < H4CB_READY_TIMEOUT);

    *status_out = status;
    return -2;
}

static int H4CB_TEXT h4cb_classify_ecc(uint8_t status)
{
    uint8_t ecc = status & H4CB_STATUS_ECC_MASK;
    uint8_t corrected = 0u;

    if(ecc == H4CB_STATUS_ECC_CLEAN)
    {
        return 0;
    }
    if(ecc == H4CB_STATUS_ECC_FAILED)
    {
        h4cb_context.public_status.ecc_worst_bits = 9u;
        return -1;
    }
    if(ecc == H4CB_STATUS_ECC_8_BITS)
    {
        corrected = 8u;
    }
    else if(ecc == H4CB_STATUS_ECC_CORRECTED)
    {
        uint8_t status2 = 0xFFu;

        if(h4cb_get_feature(H4CB_FEATURE_STATUS2, &status2) != 0)
        {
            return -2;
        }
        h4cb_context.public_status.f0 = status2;
        if((status2 & H4CB_STATUS2_ECCSE_MASK) == 0x00u)
        {
            corrected = 4u;
        }
        else if((status2 & H4CB_STATUS2_ECCSE_MASK) == 0x10u)
        {
            corrected = 5u;
        }
        else if((status2 & H4CB_STATUS2_ECCSE_MASK) == 0x20u)
        {
            corrected = 6u;
        }
        else
        {
            corrected = 7u;
        }
    }
    else
    {
        return -1;
    }
    if(corrected > h4cb_context.public_status.ecc_worst_bits)
    {
        h4cb_context.public_status.ecc_worst_bits = corrected;
    }
    return 0;
}

static int H4CB_TEXT h4cb_row_allowed(uint32_t row)
{
    uint32_t block = row / H4CB_PAGES_PER_BLOCK;
    uint32_t page = row & (H4CB_PAGES_PER_BLOCK - 1u);

    if((block == H4CB_MANIFEST_BLOCK) && (page <= 1u))
    {
        return 1;
    }
    return (block >= H4CB_PAYLOAD_FIRST_BLOCK) &&
           (block <= H4CB_PAYLOAD_LAST_BLOCK);
}

static int H4CB_TEXT h4cb_load_page(uint32_t row)
{
    uint32_t before = h4cb_context.public_status.spi_timeouts;
    uint8_t status = 0xFFu;
    int ecc;
    int ready;

    if((h4cb_row_allowed(row) == 0) ||
       (h4cb_begin(H4CB_CMD_PAGE_READ, before) != 0) ||
       (h4cb_send((uint8_t)(row >> 16), before) != 0) ||
       (h4cb_send((uint8_t)(row >> 8), before) != 0) ||
       (h4cb_send((uint8_t)row, before) != 0))
    {
        h4cb_deselect();
        return -1;
    }
    h4cb_deselect();
    ready = h4cb_wait_ready(&status);
    if(ready != 0)
    {
        return (ready == -2) ? -2 : -1;
    }
    if((status & (H4CB_STATUS_WEL | H4CB_STATUS_E_FAIL |
                  H4CB_STATUS_P_FAIL)) != 0u)
    {
        return -4;
    }
    ecc = h4cb_classify_ecc(status);
    if(ecc == -2)
    {
        return -1;
    }
    return (ecc == 0) ? 0 : -3;
}

static void H4CB_TEXT h4cb_dma_stop(void)
{
    DMA1_Channel3->CFGR &= ~H4CB_DMA_EN;
    DMA1_Channel2->CFGR &= ~H4CB_DMA_EN;
    SPI1->CTLR2 &= (uint16_t)~(H4CB_SPI_RXDMAEN | H4CB_SPI_TXDMAEN);
}

static int H4CB_TEXT h4cb_dma_stop_wait(void)
{
    uint32_t start = h4cb_cycle_now();

    h4cb_dma_stop();
    while(((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) & H4CB_DMA_EN) != 0u)
    {
        if((uint32_t)(h4cb_cycle_now() - start) >= H4CB_DMA_TIMEOUT)
        {
            h4cb_context.public_status.dma_timeouts++;
            return -1;
        }
    }
    return 0;
}

static int H4CB_TEXT h4cb_spi_wait_idle(void)
{
    uint32_t start = h4cb_cycle_now();

    while((SPI1->STATR & H4CB_SPI_BSY) != 0u)
    {
        if((uint32_t)(h4cb_cycle_now() - start) >= H4CB_DMA_TIMEOUT)
        {
            h4cb_context.public_status.spi_timeouts++;
            return -1;
        }
    }
    return 0;
}

static int H4CB_TEXT h4cb_read_cache_dma(uint8_t *destination)
{
    uint32_t before = h4cb_context.public_status.spi_timeouts;
    uint32_t start;
    uint32_t flags = 0u;
    uint32_t stop_start;

    if((destination == RT_NULL) ||
       (((uintptr_t)destination & 3u) != 0u) ||
       ((DMA2_Channel3->CFGR & H4CB_DMA_EN) != 0u) ||
       (h4cb_dma_stop_wait() != 0))
    {
        h4cb_context.public_status.dma_errors++;
        return -1;
    }
    h4cb_drain_rx();
    DMA1_Channel2->CFGR = H4CB_DMA_PRIORITY_VERY_HIGH | H4CB_DMA_MINC;
    DMA1_Channel2->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel2->MADDR = (uint32_t)(uintptr_t)destination;
    DMA1_Channel2->M1ADDR = 0u;
    DMA1_Channel2->CNTR = H4CB_PAGE_BYTES;
    DMA1_Channel3->CFGR = H4CB_DMA_PRIORITY_VERY_HIGH | H4CB_DMA_DIR;
    DMA1_Channel3->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel3->MADDR = H4CB_DMA_TX_AUX_ADDR;
    DMA1_Channel3->M1ADDR = 0u;
    DMA1_Channel3->CNTR = H4CB_PAGE_BYTES;
    DMA1->INTFCR = H4CB_DMA_GL2 | H4CB_DMA_GL3;

    if((h4cb_begin(H4CB_CMD_READ_CACHE, before) != 0) ||
       (h4cb_send(0u, before) != 0) ||
       (h4cb_send(0u, before) != 0) ||
       (h4cb_send(0u, before) != 0))
    {
        h4cb_deselect();
        return -2;
    }
    SPI1->CTLR2 |= H4CB_SPI_RXDMAEN | H4CB_SPI_TXDMAEN;
    h4cb_fence();
    start = h4cb_cycle_now();
    DMA1_Channel2->CFGR |= H4CB_DMA_EN;
    DMA1_Channel3->CFGR |= H4CB_DMA_EN;
    while((DMA1->INTFR & H4CB_DMA_DONE_MASK) != H4CB_DMA_DONE_MASK)
    {
        flags = DMA1->INTFR;
        if((flags & H4CB_DMA_ERROR_MASK) != 0u)
        {
            h4cb_context.public_status.dma_errors++;
            goto dma_failed;
        }
        if((uint32_t)(h4cb_cycle_now() - start) >= H4CB_DMA_TIMEOUT)
        {
            h4cb_context.public_status.dma_timeouts++;
            goto dma_failed;
        }
    }
    if(h4cb_spi_wait_idle() != 0)
    {
        goto dma_failed;
    }
    flags = DMA1->INTFR;
    if((flags & H4CB_DMA_ERROR_MASK) != 0u)
    {
        h4cb_context.public_status.dma_errors++;
        goto dma_failed;
    }
    if(h4cb_dma_stop_wait() != 0)
    {
        goto dma_stuck;
    }
    h4cb_deselect();
    h4cb_drain_rx();
    DMA1->INTFCR = H4CB_DMA_GL2 | H4CB_DMA_GL3;
    h4cb_fence();
    return 0;

dma_failed:
    h4cb_dma_stop();
    stop_start = h4cb_cycle_now();
    while(((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) & H4CB_DMA_EN) != 0u)
    {
        if((uint32_t)(h4cb_cycle_now() - stop_start) >= H4CB_DMA_TIMEOUT)
        {
            h4cb_context.public_status.dma_timeouts++;
            break;
        }
    }
dma_stuck:
    h4cb_deselect();
    h4cb_drain_rx();
    DMA1->INTFCR = H4CB_DMA_GL2 | H4CB_DMA_GL3;
    return -3;
}

static int H4CB_TEXT h4cb_read_cache_byte(uint16_t column, uint8_t *value)
{
    uint32_t before = h4cb_context.public_status.spi_timeouts;

    if((value == RT_NULL) ||
       (h4cb_begin(H4CB_CMD_READ_CACHE, before) != 0) ||
       (h4cb_send((uint8_t)(column >> 8), before) != 0) ||
       (h4cb_send((uint8_t)column, before) != 0) ||
       (h4cb_send(0u, before) != 0))
    {
        h4cb_deselect();
        return -1;
    }
    *value = h4cb_transfer(0u);
    h4cb_deselect();
    return (h4cb_context.public_status.spi_timeouts == before) ? 0 : -1;
}

static int H4CB_TEXT h4cb_read_page(uint32_t row, uint8_t *destination,
                                     uint8_t check_marker)
{
    uint8_t marker = 0u;
    int result = h4cb_load_page(row);

    if(result != 0)
    {
        return (result == -3) ? H4V1_FLASH_COLDBOOT_ERR_ECC :
               (result == -4) ? H4V1_FLASH_COLDBOOT_ERR_FEATURE :
               H4V1_FLASH_COLDBOOT_ERR_SPI;
    }
    if(h4cb_read_cache_dma(destination) != 0)
    {
        return H4V1_FLASH_COLDBOOT_ERR_DMA;
    }
    if(check_marker != 0u)
    {
        if(h4cb_read_cache_byte(H4CB_BAD_MARK_COLUMN, &marker) != 0)
        {
            return H4V1_FLASH_COLDBOOT_ERR_SPI;
        }
        if(marker != 0xFFu)
        {
            return H4V1_FLASH_COLDBOOT_ERR_MARKER;
        }
    }
    return H4V1_FLASH_COLDBOOT_OK;
}

static int H4CB_TEXT h4cb_acquire(void)
{
    GPIO_InitTypeDef gpio;
    uint32_t hb2_mask = RCC_HB2Periph_AFIO |
                        RCC_HB2Periph_GPIOF |
                        RCC_HB2Periph_SPI1;

    h4cb_context.saved.rcc_hbpcenr = RCC->HBPCENR;
    h4cb_context.saved.rcc_hb2pcenr = RCC->HB2PCENR;
    if(((RCC->HBPCENR & RCC_HBPeriph_DMA2) != 0u) &&
       ((DMA2_Channel3->CFGR & H4CB_DMA_EN) != 0u))
    {
        return H4V1_FLASH_COLDBOOT_ERR_DMA_BUSY;
    }
    RCC->HBPCENR |= RCC_HBPeriph_DMA1;
    h4cb_fence();
    h4cb_context.saved.dma2_cfgr = DMA1_Channel2->CFGR;
    h4cb_context.saved.dma2_cntr = DMA1_Channel2->CNTR;
    h4cb_context.saved.dma2_paddr = DMA1_Channel2->PADDR;
    h4cb_context.saved.dma2_maddr = DMA1_Channel2->MADDR;
    h4cb_context.saved.dma2_m1addr = DMA1_Channel2->M1ADDR;
    h4cb_context.saved.dma3_cfgr = DMA1_Channel3->CFGR;
    h4cb_context.saved.dma3_cntr = DMA1_Channel3->CNTR;
    h4cb_context.saved.dma3_paddr = DMA1_Channel3->PADDR;
    h4cb_context.saved.dma3_maddr = DMA1_Channel3->MADDR;
    h4cb_context.saved.dma3_m1addr = DMA1_Channel3->M1ADDR;
    h4cb_context.saved.dmamux_cfgr0_3 = DMAMUX->CFGR0_3;
    if(((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) & H4CB_DMA_EN) != 0u)
    {
        RCC->HBPCENR = (RCC->HBPCENR & ~RCC_HBPeriph_DMA1) |
                       (h4cb_context.saved.rcc_hbpcenr & RCC_HBPeriph_DMA1);
        return H4V1_FLASH_COLDBOOT_ERR_DMA_BUSY;
    }

    RCC->HB2PCENR |= hb2_mask;
    h4cb_fence();
    h4cb_context.saved.gpio_cfglr = GPIOF->CFGLR;
    h4cb_context.saved.gpio_cfghr = GPIOF->CFGHR;
    h4cb_context.saved.gpio_outdr = GPIOF->OUTDR;
    h4cb_context.saved.gpio_speed = GPIOF->SPEED;
    h4cb_context.saved.afio_gpiof_aflr = AFIO->GPIOF_AFLR;
    h4cb_context.saved.afio_gpiof_afhr = AFIO->GPIOF_AFHR;
    h4cb_context.saved.spi_ctlr1 = SPI1->CTLR1;
    h4cb_context.saved.spi_ctlr2 = SPI1->CTLR2;
    h4cb_context.saved.spi_hscr = SPI1->HSCR;
    h4cb_context.saved.spi_i2scfgr = SPI1->I2SCFGR;
    h4cb_context.saved.spi_crcr = SPI1->CRCR;
    if(((SPI1->STATR & H4CB_SPI_BSY) != 0u) ||
       ((SPI1->CTLR2 & (H4CB_SPI_RXDMAEN | H4CB_SPI_TXDMAEN)) != 0u))
    {
        RCC->HB2PCENR = (RCC->HB2PCENR & ~hb2_mask) |
                        (h4cb_context.saved.rcc_hb2pcenr & hb2_mask);
        RCC->HBPCENR = (RCC->HBPCENR & ~RCC_HBPeriph_DMA1) |
                       (h4cb_context.saved.rcc_hbpcenr & RCC_HBPeriph_DMA1);
        return H4V1_FLASH_COLDBOOT_ERR_DMA_BUSY;
    }

    gpio.GPIO_Pin = H4CB_FLASH_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOF, &gpio);
    h4cb_deselect();
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource7, GPIO_AF3);
    gpio.GPIO_Pin = H4CB_FLASH_SCK_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOF, &gpio);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource8, GPIO_AF3);
    gpio.GPIO_Pin = H4CB_FLASH_MOSI_PIN;
    GPIO_Init(GPIOF, &gpio);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource9, GPIO_AF3);
    gpio.GPIO_Pin = H4CB_FLASH_MISO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOF, &gpio);

    SPI1->CTLR1 &= (uint16_t)~H4CB_SPI_SPE;
    h4cb_drain_rx();
    SPI1->CTLR2 = 0u;
    SPI1->HSCR &= (uint16_t)~(H4CB_SPI_HSRXEN | H4CB_SPI_HSRXEN2);
    SPI1->I2SCFGR &= H4CB_SPI_MODE_SELECT_MASK;
    SPI1->CRCR = 7u;
    SPI1->CTLR1 = H4CB_SPI_MASTER | H4CB_SPI_SSI | H4CB_SPI_SSM |
                  H4CB_SPI_CPOL | H4CB_SPI_CPHA | H4CB_SPI_SPE;

    DMA1_Channel2->CFGR = 0u;
    DMA1_Channel3->CFGR = 0u;
    DMA1_Channel2->CNTR = 0u;
    DMA1_Channel3->CNTR = 0u;
    *(volatile uint32_t *)(uintptr_t)H4CB_DMA_TX_AUX_ADDR = 0u;
    h4cb_fence();
    DMAMUX->CFGR0_3 =
        (h4cb_context.saved.dmamux_cfgr0_3 &
         ~((H4CB_DMAMUX_FIELD_MASK << H4CB_DMAMUX_CH2_SHIFT) |
           (H4CB_DMAMUX_FIELD_MASK << H4CB_DMAMUX_CH3_SHIFT))) |
        ((H4CB_DMA_REQUEST_SPI1_RX - 1u) << H4CB_DMAMUX_CH2_SHIFT) |
        ((H4CB_DMA_REQUEST_SPI1_TX - 1u) << H4CB_DMAMUX_CH3_SHIFT);
    DMA1->INTFCR = H4CB_DMA_GL2 | H4CB_DMA_GL3;
    h4cb_context.owned = 1u;
    h4cb_fence();
    return H4V1_FLASH_COLDBOOT_OK;
}

static int H4CB_TEXT h4cb_restore(void)
{
    uint32_t hb2_mask = RCC_HB2Periph_AFIO |
                        RCC_HB2Periph_GPIOF |
                        RCC_HB2Periph_SPI1;

    if(h4cb_context.owned == 0u)
    {
        return H4V1_FLASH_COLDBOOT_OK;
    }
    if((h4cb_dma_stop_wait() != 0) || (h4cb_spi_wait_idle() != 0))
    {
        h4cb_deselect();
        return H4V1_FLASH_COLDBOOT_ERR_RESTORE;
    }
    h4cb_deselect();
    h4cb_drain_rx();
    DMA1->INTFCR = H4CB_DMA_GL2 | H4CB_DMA_GL3;
    DMA1_Channel2->CFGR = h4cb_context.saved.dma2_cfgr & ~H4CB_DMA_EN;
    DMA1_Channel2->CNTR = h4cb_context.saved.dma2_cntr;
    DMA1_Channel2->PADDR = h4cb_context.saved.dma2_paddr;
    DMA1_Channel2->MADDR = h4cb_context.saved.dma2_maddr;
    DMA1_Channel2->M1ADDR = h4cb_context.saved.dma2_m1addr;
    DMA1_Channel3->CFGR = h4cb_context.saved.dma3_cfgr & ~H4CB_DMA_EN;
    DMA1_Channel3->CNTR = h4cb_context.saved.dma3_cntr;
    DMA1_Channel3->PADDR = h4cb_context.saved.dma3_paddr;
    DMA1_Channel3->MADDR = h4cb_context.saved.dma3_maddr;
    DMA1_Channel3->M1ADDR = h4cb_context.saved.dma3_m1addr;
    DMAMUX->CFGR0_3 =
        (DMAMUX->CFGR0_3 &
         ~((H4CB_DMAMUX_FIELD_MASK << H4CB_DMAMUX_CH2_SHIFT) |
           (H4CB_DMAMUX_FIELD_MASK << H4CB_DMAMUX_CH3_SHIFT))) |
        (h4cb_context.saved.dmamux_cfgr0_3 &
         ((H4CB_DMAMUX_FIELD_MASK << H4CB_DMAMUX_CH2_SHIFT) |
          (H4CB_DMAMUX_FIELD_MASK << H4CB_DMAMUX_CH3_SHIFT)));

    SPI1->CTLR1 &= (uint16_t)~H4CB_SPI_SPE;
    h4cb_drain_rx();
    SPI1->HSCR = h4cb_context.saved.spi_hscr;
    SPI1->CTLR2 = h4cb_context.saved.spi_ctlr2;
    SPI1->I2SCFGR = h4cb_context.saved.spi_i2scfgr;
    SPI1->CRCR = h4cb_context.saved.spi_crcr;
    SPI1->CTLR1 = h4cb_context.saved.spi_ctlr1;
    GPIOF->CFGLR = (GPIOF->CFGLR & ~H4CB_GPIO_CFGLR_MASK) |
                   (h4cb_context.saved.gpio_cfglr & H4CB_GPIO_CFGLR_MASK);
    GPIOF->CFGHR = (GPIOF->CFGHR & ~H4CB_GPIO_CFGHR_MASK) |
                   (h4cb_context.saved.gpio_cfghr & H4CB_GPIO_CFGHR_MASK);
    GPIOF->OUTDR = (GPIOF->OUTDR & ~H4CB_GPIO_OUTDR_MASK) |
                   (h4cb_context.saved.gpio_outdr & H4CB_GPIO_OUTDR_MASK);
    GPIOF->SPEED = (GPIOF->SPEED & ~H4CB_GPIO_SPEED_MASK) |
                   (h4cb_context.saved.gpio_speed & H4CB_GPIO_SPEED_MASK);
    AFIO->GPIOF_AFLR =
        (AFIO->GPIOF_AFLR & ~H4CB_AFIO_AFLR_MASK) |
        (h4cb_context.saved.afio_gpiof_aflr & H4CB_AFIO_AFLR_MASK);
    AFIO->GPIOF_AFHR =
        (AFIO->GPIOF_AFHR & ~H4CB_AFIO_AFHR_MASK) |
        (h4cb_context.saved.afio_gpiof_afhr & H4CB_AFIO_AFHR_MASK);

    RCC->HBPCENR = (RCC->HBPCENR & ~RCC_HBPeriph_DMA1) |
                   (h4cb_context.saved.rcc_hbpcenr & RCC_HBPeriph_DMA1);
    RCC->HB2PCENR = (RCC->HB2PCENR & ~hb2_mask) |
                    (h4cb_context.saved.rcc_hb2pcenr & hb2_mask);
    h4cb_context.owned = 0u;
    h4cb_fence();
    return H4V1_FLASH_COLDBOOT_OK;
}

static int H4CB_TEXT h4cb_probe_features(void)
{
    uint8_t status = 0xFFu;

    if(h4cb_read_id() != 0)
    {
        return H4V1_FLASH_COLDBOOT_ERR_ID;
    }
    if((h4cb_get_feature(H4CB_FEATURE_PROTECTION,
                         &h4cb_context.public_status.a0) != 0) ||
       (h4cb_get_feature(H4CB_FEATURE_CONFIG,
                         &h4cb_context.public_status.b0) != 0) ||
       (h4cb_wait_ready(&status) != 0) ||
       (h4cb_get_feature(H4CB_FEATURE_STATUS2,
                         &h4cb_context.public_status.f0) != 0))
    {
        return H4V1_FLASH_COLDBOOT_ERR_SPI;
    }
    h4cb_context.public_status.c0 = status;
    if((h4cb_context.public_status.a0 != H4CB_EXPECTED_A0) ||
       (h4cb_context.public_status.b0 != H4CB_EXPECTED_B0) ||
       ((status & (H4CB_STATUS_OIP | H4CB_STATUS_WEL |
                   H4CB_STATUS_E_FAIL | H4CB_STATUS_P_FAIL)) != 0u))
    {
        return H4V1_FLASH_COLDBOOT_ERR_FEATURE;
    }
    return H4V1_FLASH_COLDBOOT_OK;
}

static int H4CB_TEXT h4cb_validate_manifest(uint8_t *stage,
                                             uint8_t require_same)
{
    uint32_t base_row = H4CB_MANIFEST_BLOCK * H4CB_PAGES_PER_BLOCK;
    uint32_t old_descriptor = h4cb_context.public_status.descriptor_crc;
    uint32_t old_commit = h4cb_context.public_status.commit_crc;
    uint32_t commit_descriptor;
    uint32_t commit_crc;
    uint32_t descriptor_crc;
    uint32_t factory_bad;
    uint32_t index;
    uint16_t previous = 0u;
    int result;

    /* Commit page is intentionally the first trusted read. */
    result = h4cb_read_page(base_row + 1u, stage, 0u);
    if(result != H4V1_FLASH_COLDBOOT_OK)
    {
        return result;
    }
    commit_descriptor = h4cb_get32(&stage[12]);
    commit_crc = h4cb_get32(&stage[H4CB_COMMIT_CRC_OFFSET]);
    if((h4cb_get32(&stage[0]) != H4CB_COMMIT_MAGIC) ||
       (h4cb_get32(&stage[4]) != H4CB_MANIFEST_VERSION) ||
       (h4cb_get32(&stage[8]) != H4CB_GENERATION) ||
       (h4cb_get32(&stage[16]) != H4V1_FLASH_COLDBOOT_TRANSFER_CRC) ||
       (h4cb_get32(&stage[20]) != H4V1_FLASH_COLDBOOT_TRANSFER_BYTES) ||
       (h4cb_get32(&stage[24]) != H4V1_FLASH_COLDBOOT_MAP_BLOCKS) ||
       (h4cb_crc32_zero_field(stage, H4CB_PAGE_BYTES,
                              H4CB_COMMIT_CRC_OFFSET) != commit_crc) ||
       (h4cb_all_ff(&stage[32], H4CB_PAGE_BYTES - 32u) == 0))
    {
        return H4V1_FLASH_COLDBOOT_ERR_COMMIT;
    }

    result = h4cb_read_page(base_row, stage, 1u);
    if(result != H4V1_FLASH_COLDBOOT_OK)
    {
        return result;
    }
    descriptor_crc = h4cb_get32(&stage[H4CB_DESCRIPTOR_CRC_OFFSET]);
    factory_bad = h4cb_get32(&stage[84]);
    if((h4cb_get32(&stage[0]) != H4CB_DESCRIPTOR_MAGIC) ||
       (h4cb_get32(&stage[4]) != H4CB_MANIFEST_VERSION) ||
       (h4cb_get32(&stage[8]) != H4CB_GENERATION) ||
       (h4cb_get32(&stage[12]) != H4CB_PAGE_BYTES) ||
       (h4cb_get32(&stage[16]) != H4V1_FLASH_COLDBOOT_TRANSFER_BYTES) ||
       (h4cb_get32(&stage[20]) != H4V1_FLASH_COLDBOOT_TRANSFER_CRC) ||
       (h4cb_get32(&stage[24]) != H4CB_CONTAINER_BYTES) ||
       (h4cb_get32(&stage[28]) != H4CB_CONTAINER_CRC) ||
       (h4cb_get32(&stage[32]) != H4V1_FLASH_COLDBOOT_FRAMES) ||
       (h4cb_get32(&stage[36]) != H4V1_FLASH_COLDBOOT_FPS) ||
       (h4cb_get32(&stage[40]) != H4CB_WIDTH) ||
       (h4cb_get32(&stage[44]) != H4CB_HEIGHT) ||
       (h4cb_get32(&stage[48]) != H4CB_FRAME_BYTES) ||
       (h4cb_get32(&stage[52]) != H4CB_PIXEL_FORMAT) ||
       (h4cb_get32(&stage[56]) != H4CB_FLAGS) ||
       (h4cb_get32(&stage[60]) != H4CB_DATA_OFFSET) ||
       (h4cb_get32(&stage[64]) != H4V1_FLASH_COLDBOOT_MAP_BLOCKS) ||
       (h4cb_get32(&stage[68]) != H4CB_PAYLOAD_FIRST_BLOCK) ||
       (h4cb_get32(&stage[72]) != H4CB_PAYLOAD_LAST_BLOCK) ||
       (h4cb_get32(&stage[76]) != H4CB_DESCRIPTOR_MAP_OFFSET) ||
       (h4cb_get32(&stage[80]) != H4CB_DESCRIPTOR_BLOCK_CRC_OFFSET) ||
       (factory_bad > 9u) ||
       (h4cb_get32(&stage[92]) != H4CB_RAW_STREAM_CRC) ||
       (h4cb_get32(&stage[96]) != H4CB_INDEX_CRC) ||
       (h4cb_get32(&stage[100]) != H4CB_HEADER_CRC) ||
       (descriptor_crc != commit_descriptor) ||
       (h4cb_crc32_zero_field(stage, H4CB_PAGE_BYTES,
                              H4CB_DESCRIPTOR_CRC_OFFSET) != descriptor_crc) ||
       (h4cb_all_ff(&stage[104], 24u) == 0) ||
       (h4cb_all_ff(&stage[602], 2u) == 0) ||
       (h4cb_all_ff(&stage[1552], H4CB_PAGE_BYTES - 1552u) == 0))
    {
        return H4V1_FLASH_COLDBOOT_ERR_DESCRIPTOR;
    }

    for(index = 0u; index < H4V1_FLASH_COLDBOOT_MAP_BLOCKS; ++index)
    {
        uint16_t physical = h4cb_get16(
            &stage[H4CB_DESCRIPTOR_MAP_OFFSET + index * 2u]);

        if((physical < H4CB_PAYLOAD_FIRST_BLOCK) ||
           (physical > H4CB_PAYLOAD_LAST_BLOCK) ||
           ((index != 0u) && (physical <= previous)) ||
           (physical < (uint16_t)(H4CB_PAYLOAD_FIRST_BLOCK + index)))
        {
            return H4V1_FLASH_COLDBOOT_ERR_MAP;
        }
        h4cb_context.block_map[index] = physical;
        h4cb_context.block_crc[index] = h4cb_get32(
            &stage[H4CB_DESCRIPTOR_BLOCK_CRC_OFFSET + index * 4u]);
        previous = physical;
    }
    if(((uint32_t)previous -
        (H4CB_PAYLOAD_FIRST_BLOCK + H4V1_FLASH_COLDBOOT_MAP_BLOCKS - 1u)) >
       factory_bad)
    {
        return H4V1_FLASH_COLDBOOT_ERR_MAP;
    }
    if((require_same != 0u) &&
       ((descriptor_crc != old_descriptor) || (commit_crc != old_commit)))
    {
        return H4V1_FLASH_COLDBOOT_ERR_MANIFEST_CHANGED;
    }

    h4cb_context.public_status.descriptor_crc = descriptor_crc;
    h4cb_context.public_status.commit_crc = commit_crc;
    h4cb_context.public_status.first_block = h4cb_context.block_map[0];
    h4cb_context.public_status.last_block =
        h4cb_context.block_map[H4V1_FLASH_COLDBOOT_MAP_BLOCKS - 1u];
    h4cb_context.public_status.factory_bad_blocks = (uint8_t)factory_bad;
    h4cb_fence();
    return H4V1_FLASH_COLDBOOT_OK;
}

static void H4CB_TEXT h4cb_log_reject(uint8_t load_phase)
{
    char line[224];
    int used;

    if(h4cb_context.failure_logged != 0u)
    {
        return;
    }
    h4cb_context.failure_logged = 1u;
    if(load_phase != 0u)
    {
        used = rt_snprintf(
            line, sizeof(line), h4cb_load_reject_format,
            (int)h4cb_context.public_status.error,
            (unsigned int)h4cb_context.public_status.bytes_loaded,
            (unsigned int)h4cb_context.public_status.pages_loaded,
            (unsigned int)h4cb_context.public_status.flash_crc,
            (unsigned int)h4cb_context.public_status.logical_block,
            (unsigned int)h4cb_context.public_status.physical_block,
            (unsigned int)h4cb_context.public_status.page,
            (unsigned int)h4cb_context.public_status.ecc_worst_bits);
    }
    else
    {
        used = rt_snprintf(
            line, sizeof(line), h4cb_manifest_reject_format,
            (int)h4cb_context.public_status.error,
            (unsigned int)h4cb_context.public_status.state,
            (unsigned int)h4cb_context.public_status.descriptor_crc,
            (unsigned int)h4cb_context.public_status.commit_crc,
            (unsigned int)h4cb_context.public_status.ecc_worst_bits);
    }
    if((used > 0) && ((uint32_t)used < sizeof(line)))
    {
        h4v1_flash_coldboot_log_line(line);
    }
}

static int H4CB_TEXT h4cb_reject(int error, uint8_t load_phase)
{
    int restore_result;

    h4cb_context.public_status.error = error;
    restore_result = h4cb_restore();
    if((restore_result != H4V1_FLASH_COLDBOOT_OK) &&
       (error == H4V1_FLASH_COLDBOOT_OK))
    {
        h4cb_context.public_status.error = restore_result;
    }
    h4cb_context.public_status.state = H4V1_FLASH_COLDBOOT_STATE_REJECTED;
    h4cb_log_reject(load_phase);
    return h4cb_context.public_status.error;
}

static void H4CB_TEXT h4cb_log_manifest_pass(void)
{
    char line[224];
    int used;

    if(h4cb_context.manifest_logged != 0u)
    {
        return;
    }
    h4cb_context.manifest_logged = 1u;
    used = rt_snprintf(
        line, sizeof(line), h4v1_flash_coldboot_manifest_pass_format,
        (unsigned int)h4cb_context.public_status.descriptor_crc,
        (unsigned int)h4cb_context.public_status.commit_crc,
        (unsigned int)h4cb_context.public_status.first_block,
        (unsigned int)h4cb_context.public_status.last_block,
        (unsigned int)h4cb_context.public_status.factory_bad_blocks,
        (unsigned int)h4cb_context.public_status.ecc_worst_bits);
    if((used > 0) && ((uint32_t)used < sizeof(line)))
    {
        h4v1_flash_coldboot_log_line(line);
    }
}

static void H4CB_TEXT h4cb_log_load_pass(void)
{
    char line[224];
    int used;

    if(h4cb_context.load_logged != 0u)
    {
        return;
    }
    h4cb_context.load_logged = 1u;
    used = rt_snprintf(
        line, sizeof(line), h4v1_flash_coldboot_load_pass_format,
        (unsigned int)h4cb_context.public_status.descriptor_crc,
        (unsigned int)h4cb_context.public_status.commit_crc,
        (unsigned int)h4cb_context.public_status.flash_crc,
        (unsigned int)h4cb_context.public_status.ecc_worst_bits);
    if((used > 0) && ((uint32_t)used < sizeof(line)))
    {
        h4v1_flash_coldboot_log_line(line);
    }
}

#if H4V1_FLASH_COLDBOOT_PRODUCT
int H4CB_ENTRY h4v1_flash_coldboot_abort(void)
{
    int result;

    if(h4cb_context_valid() == 0)
    {
        return H4V1_FLASH_COLDBOOT_OK;
    }
    result = h4cb_restore();
    if(result != H4V1_FLASH_COLDBOOT_OK)
    {
        /* Even if register restoration times out, never leave DMA running. */
        h4cb_dma_stop();
        h4cb_deselect();
        DMA1->INTFCR = H4CB_DMA_GL2 | H4CB_DMA_GL3;
        h4cb_fence();
    }
    return result;
}
#endif

void H4CB_ENTRY h4v1_flash_coldboot_reset(void)
{
    /*
     * The hook guarantees this happens once before the first provider call.
     * Do not touch peripherals here: DMA/SPI ownership has not been acquired,
     * and a qualified subsystem may legitimately be using those registers.
     */
    h4cb_clear_context();
}

int H4CB_ENTRY h4v1_flash_coldboot_line_provider(char *dst, uint32_t cap)
{
    uint8_t *stage = (uint8_t *)(uintptr_t)H4CB_STAGE_ADDR;
    uint32_t length = (uint32_t)sizeof(h4cb_command) - 1u;
    uint32_t index;
    int result;

    if(h4cb_context_valid() == 0)
    {
        h4v1_flash_coldboot_reset();
    }
    if(h4cb_context.public_status.state == H4V1_FLASH_COLDBOOT_STATE_REJECTED)
    {
        return 0;
    }
    if(h4cb_context.public_status.state != H4V1_FLASH_COLDBOOT_STATE_RESET)
    {
        return 0;
    }
    if((dst == RT_NULL) || (cap <= length))
    {
        (void)h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_ARGUMENT, 0u);
        return 0;
    }
    result = h4cb_acquire();
    if(result == H4V1_FLASH_COLDBOOT_OK)
    {
        result = h4cb_probe_features();
    }
    if(result == H4V1_FLASH_COLDBOOT_OK)
    {
        result = h4cb_validate_manifest(stage, 0u);
    }
    if(result != H4V1_FLASH_COLDBOOT_OK)
    {
        (void)h4cb_reject(result, 0u);
        return 0;
    }
    h4cb_context.public_status.state = H4V1_FLASH_COLDBOOT_STATE_MANIFEST;
    h4cb_log_manifest_pass();
    for(index = 0u; index < length; ++index)
    {
        dst[index] = h4cb_command[index];
    }
    dst[length] = '\0';
    h4cb_context.command_issued = 1u;
    h4cb_watchdog_feed();
    return (int)length;
}

int H4CB_ENTRY h4v1_flash_coldboot_raw_provider(void *dst, uint32_t cap)
{
    uint8_t *output = (uint8_t *)dst;
    uintptr_t output_address = (uintptr_t)output;
    uint32_t remaining;
    uint32_t take;
    uint32_t produced = 0u;
    int result;

    if((h4cb_context_valid() == 0) ||
       (h4cb_context.public_status.state == H4V1_FLASH_COLDBOOT_STATE_REJECTED))
    {
        return H4V1_FLASH_COLDBOOT_ERR_STATE;
    }
    if(h4cb_context.public_status.state == H4V1_FLASH_COLDBOOT_STATE_LOADED)
    {
        return 0;
    }
    if((h4cb_context.public_status.state != H4V1_FLASH_COLDBOOT_STATE_MANIFEST) &&
       (h4cb_context.public_status.state != H4V1_FLASH_COLDBOOT_STATE_LOAD))
    {
        return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_STATE, 1u);
    }
    if((output == RT_NULL) || (cap < H4CB_PAGE_BYTES) ||
       (cap > H4CB_STAGE_BYTES) || ((cap % H4CB_PAGE_BYTES) != 0u) ||
       ((output_address & 3u) != 0u) ||
       (output_address < H4CB_STAGE_ADDR) ||
       (output_address > (H4CB_STAGE_END - cap)))
    {
        return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_ARGUMENT, 1u);
    }

    if(h4cb_context.raw_disabled == 0u)
    {
        uint32_t descriptor = h4cb_context.public_status.descriptor_crc;
        uint32_t commit = h4cb_context.public_status.commit_crc;

#if H4V1_FLASH_COLDBOOT_PRODUCT
        if(h4v1_flash_coldboot_product_raw_disable() != 0)
#else
        if(ch32h417_usb_cdc_raw_rx_enable(0u) != 0)
#endif
        {
            return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_RAW_DISABLE, 1u);
        }
        h4cb_context.raw_disabled = 1u;
        result = h4cb_validate_manifest(output, 1u);
        if((result != H4V1_FLASH_COLDBOOT_OK) ||
           (descriptor != h4cb_context.public_status.descriptor_crc) ||
           (commit != h4cb_context.public_status.commit_crc))
        {
            if(result == H4V1_FLASH_COLDBOOT_OK)
            {
                result = H4V1_FLASH_COLDBOOT_ERR_MANIFEST_CHANGED;
            }
            return h4cb_reject(result, 1u);
        }
        h4cb_context.public_status.state = H4V1_FLASH_COLDBOOT_STATE_LOAD;
        h4cb_context.public_status.bytes_loaded = 0u;
        h4cb_context.public_status.pages_loaded = 0u;
        h4cb_context.public_status.flash_crc = 0u;
        h4cb_context.running_crc = 0u;
        h4cb_context.running_block_crc = 0u;
    }

    remaining = H4V1_FLASH_COLDBOOT_TRANSFER_BYTES -
                h4cb_context.public_status.bytes_loaded;
    take = (cap < remaining) ? cap : remaining;
    if((take == 0u) || ((take % H4CB_PAGE_BYTES) != 0u))
    {
        return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_ARGUMENT, 1u);
    }

    while(produced < take)
    {
        uint32_t logical_page =
            h4cb_context.public_status.bytes_loaded / H4CB_PAGE_BYTES;
        uint32_t logical_block = logical_page / H4CB_PAGES_PER_BLOCK;
        uint32_t page = logical_page & (H4CB_PAGES_PER_BLOCK - 1u);
        uint32_t physical;
        uint32_t row;
        uint8_t last_page;

        if(logical_block >= H4V1_FLASH_COLDBOOT_MAP_BLOCKS)
        {
            return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_MAP, 1u);
        }
        physical = h4cb_context.block_map[logical_block];
        if((physical < H4CB_PAYLOAD_FIRST_BLOCK) ||
           (physical > H4CB_PAYLOAD_LAST_BLOCK))
        {
            return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_MAP, 1u);
        }
        row = physical * H4CB_PAGES_PER_BLOCK + page;
        h4cb_context.public_status.logical_block = logical_block;
        h4cb_context.public_status.physical_block = physical;
        h4cb_context.public_status.page = page;
        result = h4cb_read_page(row, &output[produced],
                                 (uint8_t)(page == 0u));
        if(result != H4V1_FLASH_COLDBOOT_OK)
        {
            return h4cb_reject(result, 1u);
        }
        h4cb_context.running_block_crc = h4cb_crc32_update(
            h4cb_context.running_block_crc, &output[produced],
            H4CB_PAGE_BYTES);
        h4cb_context.running_crc = h4cb_crc32_update(
            h4cb_context.running_crc, &output[produced], H4CB_PAGE_BYTES);
        h4cb_context.public_status.bytes_loaded += H4CB_PAGE_BYTES;
        h4cb_context.public_status.pages_loaded++;
        h4cb_context.public_status.flash_crc = h4cb_context.running_crc;
        produced += H4CB_PAGE_BYTES;

        last_page = (uint8_t)((page + 1u == H4CB_PAGES_PER_BLOCK) ||
                    (h4cb_context.public_status.bytes_loaded ==
                     H4V1_FLASH_COLDBOOT_TRANSFER_BYTES));
        if(last_page != 0u)
        {
            if(h4cb_context.running_block_crc !=
               h4cb_context.block_crc[logical_block])
            {
                return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_CRC, 1u);
            }
            h4cb_context.running_block_crc = 0u;
        }
        h4cb_watchdog_feed();
    }

    if(h4cb_context.public_status.bytes_loaded ==
       H4V1_FLASH_COLDBOOT_TRANSFER_BYTES)
    {
        if((h4cb_context.public_status.pages_loaded != H4CB_REQUIRED_PAGES) ||
           (h4cb_context.running_crc != H4V1_FLASH_COLDBOOT_TRANSFER_CRC))
        {
            return h4cb_reject(H4V1_FLASH_COLDBOOT_ERR_CRC, 1u);
        }
        result = h4cb_restore();
        if(result != H4V1_FLASH_COLDBOOT_OK)
        {
            return h4cb_reject(result, 1u);
        }
        h4cb_context.public_status.state = H4V1_FLASH_COLDBOOT_STATE_LOADED;
        h4cb_context.public_status.error = H4V1_FLASH_COLDBOOT_OK;
        h4cb_log_load_pass();
    }
    return (int)produced;
}

void H4CB_ENTRY h4v1_flash_coldboot_play_pass(void)
{
    char line[192];
    int used;

    if((h4cb_context_valid() == 0) ||
       (h4cb_context.public_status.state != H4V1_FLASH_COLDBOOT_STATE_LOADED) ||
       (h4cb_context.public_status.bytes_loaded !=
        H4V1_FLASH_COLDBOOT_TRANSFER_BYTES) ||
       (h4cb_context.public_status.flash_crc !=
        H4V1_FLASH_COLDBOOT_TRANSFER_CRC))
    {
        if((h4cb_context_valid() != 0) && (h4cb_context.play_logged == 0u))
        {
            h4cb_context.play_logged = 1u;
            used = rt_snprintf(
                line, sizeof(line), h4cb_play_reject_format,
                (int)h4cb_context.public_status.error,
                (unsigned int)h4cb_context.public_status.state,
                (unsigned int)h4cb_context.public_status.bytes_loaded,
                (unsigned int)h4cb_context.public_status.flash_crc);
            if((used > 0) && ((uint32_t)used < sizeof(line)))
            {
                h4v1_flash_coldboot_log_line(line);
            }
        }
        return;
    }
    if(h4cb_context.play_logged != 0u)
    {
        return;
    }
    h4cb_context.play_logged = 1u;
    used = rt_snprintf(
        line, sizeof(line), h4v1_flash_coldboot_play_pass_format,
        (unsigned int)h4cb_context.public_status.descriptor_crc,
        (unsigned int)h4cb_context.public_status.commit_crc);
    if((used > 0) && ((uint32_t)used < sizeof(line)))
    {
        h4v1_flash_coldboot_log_line(line);
    }
    h4cb_context.public_status.state = H4V1_FLASH_COLDBOOT_STATE_PLAYED;
}

void H4CB_ENTRY h4v1_flash_coldboot_replay_status(void)
{
    char line[224];
    uint32_t state;
    int used;

    if(h4cb_context_valid() == 0)
    {
        return;
    }
    state = h4cb_context.public_status.state;
    if((state != H4V1_FLASH_COLDBOOT_STATE_MANIFEST) &&
       (state != H4V1_FLASH_COLDBOOT_STATE_LOAD) &&
       (state != H4V1_FLASH_COLDBOOT_STATE_LOADED) &&
       (state != H4V1_FLASH_COLDBOOT_STATE_PLAYED))
    {
        return;
    }

    h4v1_flash_coldboot_log_line(h4v1_flash_coldboot_banner);
    used = rt_snprintf(
        line, sizeof(line), h4v1_flash_coldboot_manifest_pass_format,
        (unsigned int)h4cb_context.public_status.descriptor_crc,
        (unsigned int)h4cb_context.public_status.commit_crc,
        (unsigned int)h4cb_context.public_status.first_block,
        (unsigned int)h4cb_context.public_status.last_block,
        (unsigned int)h4cb_context.public_status.factory_bad_blocks,
        (unsigned int)h4cb_context.public_status.ecc_worst_bits);
    if((used > 0) && ((uint32_t)used < sizeof(line)))
    {
        h4v1_flash_coldboot_log_line(line);
    }

    if((state == H4V1_FLASH_COLDBOOT_STATE_LOADED) ||
       (state == H4V1_FLASH_COLDBOOT_STATE_PLAYED))
    {
        used = rt_snprintf(
            line, sizeof(line), h4v1_flash_coldboot_load_pass_format,
            (unsigned int)h4cb_context.public_status.descriptor_crc,
            (unsigned int)h4cb_context.public_status.commit_crc,
            (unsigned int)h4cb_context.public_status.flash_crc,
            (unsigned int)h4cb_context.public_status.ecc_worst_bits);
        if((used > 0) && ((uint32_t)used < sizeof(line)))
        {
            h4v1_flash_coldboot_log_line(line);
        }
    }

    if(state == H4V1_FLASH_COLDBOOT_STATE_PLAYED)
    {
        used = rt_snprintf(
            line, sizeof(line), h4v1_flash_coldboot_play_pass_format,
            (unsigned int)h4cb_context.public_status.descriptor_crc,
            (unsigned int)h4cb_context.public_status.commit_crc);
        if((used > 0) && ((uint32_t)used < sizeof(line)))
        {
            h4v1_flash_coldboot_log_line(line);
        }
    }
}

void H4CB_ENTRY h4v1_flash_coldboot_ack_suppress(const char *line)
{
    (void)line;
}

const h4v1_flash_coldboot_status_t *H4CB_ENTRY
h4v1_flash_coldboot_status(void)
{
    return &h4cb_context.public_status;
}

_Static_assert(sizeof(h4v1_flash_coldboot_banner) - 1u == 48u,
               "Frozen cold-boot banner length changed");
