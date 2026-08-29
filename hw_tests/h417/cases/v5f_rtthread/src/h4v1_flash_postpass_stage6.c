/*
 * H4V1 Flash Stage 6: isolated CHUNK165 installer.
 *
 * The qualified USB/SDRAM/decoder objects call this translation unit only
 * after their original RESULT PASS.  Stage 6 copies the already-qualified
 * padded upload from SDRAM to NAND.  It deliberately owns neither USB nor
 * LTDC and never enables DMA1 and DMA2 at the same time.
 *
 * NAND layout is fixed and fail closed:
 *   block 768, pages 0/1  descriptor/commit (commit is written last)
 *   blocks 769..1014     bad-block-aware payload pool
 *   block 1015           retained Stage-5 scratch, never touched
 *   blocks 1016..1023    retained L8 assets, never touched
 */
#include "h4v1_flash_installer_stage6.h"

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
#define H4V1_STAGE6_CMD_PROGRAM_LOAD       0x02u
#define H4V1_STAGE6_CMD_READ_CACHE         0x03u
#define H4V1_STAGE6_CMD_WRITE_DISABLE      0x04u
#define H4V1_STAGE6_CMD_WRITE_ENABLE       0x06u
#define H4V1_STAGE6_CMD_GET_FEATURE        0x0Fu
#define H4V1_STAGE6_CMD_PROGRAM_EXECUTE    0x10u
#define H4V1_STAGE6_CMD_PAGE_READ          0x13u
#define H4V1_STAGE6_CMD_SET_FEATURE        0x1Fu
#define H4V1_STAGE6_CMD_READ_ID            0x9Fu
#define H4V1_STAGE6_CMD_BLOCK_ERASE        0xD8u

#define H4V1_STAGE6_FEATURE_PROTECTION     0xA0u
#define H4V1_STAGE6_FEATURE_CONFIG         0xB0u
#define H4V1_STAGE6_FEATURE_STATUS         0xC0u
#define H4V1_STAGE6_FEATURE_STATUS2        0xF0u
#define H4V1_STAGE6_EXPECTED_MID           0xC8u
#define H4V1_STAGE6_EXPECTED_DID           0x91u
#define H4V1_STAGE6_A0_EXPECTED_LOCKED     0x38u
#define H4V1_STAGE6_A0_TEMP                0x2Au
#define H4V1_STAGE6_A0_BRWD                0x80u
#define H4V1_STAGE6_B0_ECC_ENABLE          0x10u
#define H4V1_STAGE6_B0_BPL                 0x08u
#define H4V1_STAGE6_B0_OTP_ENABLE          0x40u
#define H4V1_STAGE6_B0_QUAD_ENABLE         0x01u
#define H4V1_STAGE6_STATUS_OIP             0x01u
#define H4V1_STAGE6_STATUS_WEL             0x02u
#define H4V1_STAGE6_STATUS_E_FAIL          0x04u
#define H4V1_STAGE6_STATUS_P_FAIL          0x08u
#define H4V1_STAGE6_STATUS_ECC_MASK        0x30u
#define H4V1_STAGE6_STATUS_ECC_CLEAN       0x00u
#define H4V1_STAGE6_STATUS_ECC_CORRECTED   0x10u
#define H4V1_STAGE6_STATUS_ECC_FAILED      0x20u
#define H4V1_STAGE6_STATUS_ECC_8_BITS      0x30u
#define H4V1_STAGE6_STATUS2_ECCSE_MASK     0x30u

#define H4V1_STAGE6_SPI_SPE                0x0040u
#define H4V1_STAGE6_SPI_BR_MASK            0x0038u
#define H4V1_STAGE6_SPI_RXNE               0x0001u
#define H4V1_STAGE6_SPI_TXE                0x0002u
#define H4V1_STAGE6_SPI_BSY                0x0080u
#define H4V1_STAGE6_SPI_RXDMAEN            0x0001u
#define H4V1_STAGE6_SPI_TXDMAEN            0x0002u
#define H4V1_STAGE6_SPI_HSRXEN             0x0001u
#define H4V1_STAGE6_SPI_HSRXEN2            0x0004u

#define H4V1_STAGE6_DMA_EN                 0x0001u
#define H4V1_STAGE6_DMA_DIR                0x0010u
#define H4V1_STAGE6_DMA_MINC               0x0080u
#define H4V1_STAGE6_DMA_PRIORITY_VERY_HIGH 0x3000u
#define H4V1_STAGE6_DMA_GL2                0x00000010u
#define H4V1_STAGE6_DMA_TC2                0x00000020u
#define H4V1_STAGE6_DMA_TE2                0x00000080u
#define H4V1_STAGE6_DMA_GL3                0x00000100u
#define H4V1_STAGE6_DMA_TC3                0x00000200u
#define H4V1_STAGE6_DMA_TE3                0x00000800u
#define H4V1_STAGE6_DMA_DONE_MASK          \
    (H4V1_STAGE6_DMA_TC2 | H4V1_STAGE6_DMA_TC3)
#define H4V1_STAGE6_DMA_ERROR_MASK         \
    (H4V1_STAGE6_DMA_TE2 | H4V1_STAGE6_DMA_TE3)
#define H4V1_STAGE6_DMAMUX_CH2_SHIFT       8u
#define H4V1_STAGE6_DMAMUX_CH3_SHIFT       16u
#define H4V1_STAGE6_DMAMUX_FIELD_MASK      0x7Fu
#define H4V1_STAGE6_DMA_REQUEST_SPI1_TX    63u
#define H4V1_STAGE6_DMA_REQUEST_SPI1_RX    64u
#define H4V1_STAGE6_RCC_DMA1               0x00000001u
#define H4V1_STAGE6_RCC_DMA2               0x00000002u

#define H4V1_STAGE6_SPI_BYTE_TIMEOUT       1000000u
#define H4V1_STAGE6_DMA_TIMEOUT            4000000u
#define H4V1_STAGE6_READY_TIMEOUT          80000000u
#define H4V1_STAGE6_PAGE_BYTES             2048u
#define H4V1_STAGE6_PAGES_PER_BLOCK        64u
#define H4V1_STAGE6_BAD_MARK_COLUMN        2048u
#define H4V1_STAGE6_DATA_ADDR              0x20174000u
#define H4V1_STAGE6_AUX_ADDR               0x20174800u
#define H4V1_STAGE6_META_ADDR              0x20175000u
#define H4V1_STAGE6_META_END               0x20176000u
#define H4V1_STAGE6_MAP_ADDR               H4V1_STAGE6_META_ADDR
#define H4V1_STAGE6_BLOCK_CRC_ADDR          0x20175200u
#define H4V1_STAGE6_CANDIDATE_ADDR          0x20175600u
#define H4V1_STAGE6_RETAIN_ADDR             0x2017FF00u

#define H4V1_STAGE6_SOURCE_ADDR             0x60200000u
#define H4V1_STAGE6_TRANSFER_BYTES          30965760u
#define H4V1_STAGE6_TRANSFER_CRC            0xE32A6C99u
#define H4V1_STAGE6_CONTAINER_BYTES         30933600u
#define H4V1_STAGE6_CONTAINER_CRC           0x4097F39Au
#define H4V1_STAGE6_FRAME_COUNT             165u
#define H4V1_STAGE6_FPS                     30u
#define H4V1_STAGE6_WIDTH                   800u
#define H4V1_STAGE6_HEIGHT                  480u
#define H4V1_STAGE6_FRAME_BYTES             768000u
#define H4V1_STAGE6_DATA_OFFSET             4096u
#define H4V1_STAGE6_FLAGS                   0x0000000Eu

#define H4V1_STAGE6_MANIFEST_BLOCK          768u
#define H4V1_STAGE6_PAYLOAD_FIRST_BLOCK     769u
#define H4V1_STAGE6_PAYLOAD_LAST_BLOCK      1014u
#define H4V1_STAGE6_STAGE5_BLOCK            1015u
#define H4V1_STAGE6_L8_FIRST_BLOCK          1016u
#define H4V1_STAGE6_CANDIDATE_BLOCKS        246u
#define H4V1_STAGE6_REQUIRED_PAGES          15120u
#define H4V1_STAGE6_REQUIRED_BLOCKS         237u
#define H4V1_STAGE6_LAST_BLOCK_PAGES        16u

#define H4V1_STAGE6_DESCRIPTOR_MAGIC        0x36493448u /* H4I6 */
#define H4V1_STAGE6_COMMIT_MAGIC            0x36433448u /* H4C6 */
#define H4V1_STAGE6_MANIFEST_VERSION        1u
#define H4V1_STAGE6_GENERATION              1u
#define H4V1_STAGE6_DESCRIPTOR_MAP_OFFSET   128u
#define H4V1_STAGE6_DESCRIPTOR_CRC_OFFSET   88u
#define H4V1_STAGE6_DESCRIPTOR_BLOCK_CRC_OFFSET 604u
#define H4V1_STAGE6_COMMIT_CRC_OFFSET       28u

#define H4V1_STAGE6_DMA2_GL3                0x00000100u
#define H4V1_STAGE6_DMA2_TC3                0x00000200u
#define H4V1_STAGE6_DMA2_TE3                0x00000800u
#define H4V1_STAGE6_DMA2_TIMEOUT            4000000u

#define H4V1_STAGE6_IWDG_FEED_KEY           0xAAAAu
#define H4V1_STAGE6_FNV_OFFSET             2166136261u
#define H4V1_STAGE6_FNV_PRIME              16777619u

#if (H4V1_STAGE6_DATA_ADDR + H4V1_STAGE6_PAGE_BYTES) > H4V1_STAGE6_AUX_ADDR
#error "Stage6 page buffer overlaps the auxiliary DMA byte"
#endif
#if H4V1_STAGE6_CANDIDATE_ADDR + (H4V1_STAGE6_CANDIDATE_BLOCKS * 2u) > H4V1_STAGE6_META_END
#error "Stage6 metadata escapes the approved shared-SRAM window"
#endif
#if (H4V1_STAGE6_MAP_ADDR + (H4V1_STAGE6_REQUIRED_BLOCKS * 2u)) > H4V1_STAGE6_BLOCK_CRC_ADDR
#error "Stage6 selected-block map overlaps block CRCs"
#endif
#if (H4V1_STAGE6_BLOCK_CRC_ADDR + (H4V1_STAGE6_REQUIRED_BLOCKS * 4u)) > H4V1_STAGE6_CANDIDATE_ADDR
#error "Stage6 block CRCs overlap the candidate map"
#endif
#if (H4V1_STAGE6_CANDIDATE_ADDR + (H4V1_STAGE6_CANDIDATE_BLOCKS * 2u)) > 0x20175FF0u
#error "Stage6 candidate map overlaps installer control words"
#endif
#if 0x20175FF8u > H4V1_STAGE6_META_END
#error "Stage6 installer control words escape metadata"
#endif
#if (H4V1_STAGE6_TRANSFER_BYTES % H4V1_STAGE6_PAGE_BYTES) != 0u
#error "Stage6 transfer must contain whole NAND pages"
#endif
#if (H4V1_STAGE6_SOURCE_ADDR + H4V1_STAGE6_TRANSFER_BYTES) > 0x62000000u
#error "Stage6 source escapes the 32 MiB SDRAM aperture"
#endif
#if (H4V1_STAGE6_REQUIRED_BLOCKS * H4V1_STAGE6_PAGES_PER_BLOCK - H4V1_STAGE6_REQUIRED_PAGES) != 48u
#error "Stage6 payload geometry changed"
#endif
#if H4V1_STAGE6_PAYLOAD_LAST_BLOCK >= H4V1_STAGE6_STAGE5_BLOCK
#error "Stage6 payload reaches the retained Stage-5 block"
#endif
#if H4V1_STAGE6_STAGE5_BLOCK >= H4V1_STAGE6_L8_FIRST_BLOCK
#error "Stage6 retained Stage-5 block reaches L8"
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
    uint32_t source_crc;
    uint32_t flash_crc;
    uint32_t descriptor_crc;
    uint32_t commit_crc;
    uint32_t raw_stream_crc;
    uint32_t index_crc;
    uint32_t header_crc;
    uint32_t bytes_programmed;
    uint32_t bytes_verified;
    uint32_t pages_programmed;
    uint32_t pages_verified;
    uint32_t factory_bad_blocks;
    uint32_t scan_unreadable_blocks;
    uint32_t selected_blocks;
    uint32_t current_logical_block;
    uint32_t current_physical_block;
    uint32_t current_page;
    uint8_t manifest_committed;
} h4v1_stage6_result_t;

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
} h4v1_stage6_saved_t;

typedef struct
{
    uint32_t clock_was_enabled;
    uint32_t cfgr;
    uint32_t cntr;
    uint32_t paddr;
    uint32_t maddr;
    uint32_t m1addr;
} h4v1_stage6_dma2_saved_t;

typedef struct
{
    uint32_t magic;
    uint32_t phase;
    uint32_t logical_block;
    uint32_t physical_block;
    uint32_t page;
    uint32_t source_offset;
    uint32_t running_crc;
    uint32_t checksum;
} h4v1_stage6_retain_t;

enum
{
    H4V1_STAGE6_WRITE_NONE = 0,
    H4V1_STAGE6_WRITE_MANIFEST = 1,
    H4V1_STAGE6_WRITE_PAYLOAD = 2
};

#define h4v1_stage6_map \
    ((volatile uint16_t *)(uintptr_t)H4V1_STAGE6_MAP_ADDR)
#define h4v1_stage6_block_crc \
    ((volatile uint32_t *)(uintptr_t)H4V1_STAGE6_BLOCK_CRC_ADDR)
#define h4v1_stage6_candidates \
    ((volatile uint16_t *)(uintptr_t)H4V1_STAGE6_CANDIDATE_ADDR)
#define h4v1_stage6_retain \
    ((volatile h4v1_stage6_retain_t *)(uintptr_t)H4V1_STAGE6_RETAIN_ADDR)
#define h4v1_stage6_write_phase \
    (*(volatile uint32_t *)(uintptr_t)0x20175FF0u)
#define h4v1_stage6_selected_count \
    (*(volatile uint32_t *)(uintptr_t)0x20175FF4u)

extern void h4v1_postpass_stage1_log(const char *text);

const char h4v1_stage6_start_text[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL STAGE6 START destructive=1 source=60200000 "
    "bytes=30965760 pages=15120 payload=769..1014 manifest=768 "
    "reserved=1015 l8=1016..1023 a0=2a/38 dma=DMA2_TO_SRAM+DMA1_SPI";
const char h4v1_stage6_source_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL SOURCE PASS bytes=%u crc=%08x container=%u/%08x "
    "frames=165 fps=30 data_offset=4096";
const char h4v1_stage6_plan_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL PLAN PASS candidates=%u good=%u bad=%u unreadable=%u "
    "selected=%u first=%u last=%u";
const char h4v1_stage6_program_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL PROGRAM blocks=%u/237 pages=%u/15120 bytes=%u/30965760 "
    "block=%u crc=%08x";
const char h4v1_stage6_verify_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL VERIFY blocks=%u/237 pages=%u/15120 bytes=%u/30965760 "
    "crc=%08x ecc_worst=%u";
const char h4v1_stage6_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL STAGE6 PASS bytes=%u crc=%08x pages=%u blocks=%u "
    "bad_skipped=%u manifest=768 committed=1 a0=%02x wel=0 "
    "ecc_worst=%u timeouts=%u/%u/%u dma_error=%u illegal=%u";
const char h4v1_stage6_fail_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL STAGE6 FAIL result=%d primary=%d phase=%u logical=%u "
    "block=%u page=%u bytes=%u source_crc=%08x flash_crc=%08x "
    "status=%02x a0=%02x/%02x cleanup=%u committed=%u "
    "dirty_orphans=%u timeouts=%u/%u/%u "
    "dma_error=%u illegal=%u";
const char h4v1_stage6_manifest_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL MANIFEST PASS block=768 descriptor=valid "
    "crc=%08x map=237";
const char h4v1_stage6_invalidate_pass_text[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL INVALIDATE PASS manifest=768 committed=0 "
    "a0=38 wel=0";
const char h4v1_stage6_payload_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL PAYLOAD PASS bytes=%u pages=%u blocks=%u "
    "source_crc=%08x";
const char h4v1_stage6_verify_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL VERIFY PASS bytes=%u pages=%u crc=%08x "
    "ecc_worst=%u";
const char h4v1_stage6_commit_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH INSTALL COMMIT PASS block=768 page=1 crc=%08x "
    "descriptor=%08x committed=1 a0=%02x b0=%02x wel=0";
const uint32_t h4v1_stage6_crc_table[16] H4V1_POSTPASS_RODATA =
{
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

uint32_t H4V1_POSTPASS_TEXT h4v1_stage6_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

void H4V1_POSTPASS_TEXT h4v1_stage6_fence(void)
{
    __asm volatile ("fence iorw, iorw" ::: "memory");
}

uint16_t H4V1_POSTPASS_TEXT h4v1_stage6_get16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

uint32_t H4V1_POSTPASS_TEXT h4v1_stage6_get32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

void H4V1_POSTPASS_TEXT h4v1_stage6_put16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

void H4V1_POSTPASS_TEXT h4v1_stage6_put32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

uint32_t H4V1_POSTPASS_TEXT
h4v1_stage6_crc32_update(uint32_t previous, const uint8_t *data,
                         uint32_t length)
{
    uint32_t crc = ~previous;

    while(length-- != 0u)
    {
        crc ^= *data++;
        crc = h4v1_stage6_crc_table[crc & 0x0Fu] ^ (crc >> 4);
        crc = h4v1_stage6_crc_table[crc & 0x0Fu] ^ (crc >> 4);
    }
    return ~crc;
}

void H4V1_POSTPASS_TEXT h4v1_stage6_fill(uint8_t *data, uint8_t value,
                                         uint32_t length)
{
    while(length-- != 0u)
    {
        *data++ = value;
    }
    h4v1_stage6_fence();
}

void H4V1_POSTPASS_TEXT h4v1_stage6_iwdg_feed(void)
{
    IWDG->CTLR = H4V1_STAGE6_IWDG_FEED_KEY;
}

void H4V1_POSTPASS_TEXT
h4v1_stage6_checkpoint(uint32_t phase, uint32_t logical_block,
                        uint32_t physical_block, uint32_t page,
                        uint32_t source_offset, uint32_t crc)
{
    uint32_t checksum = 0x49365336u ^ phase ^ logical_block ^
                        physical_block ^ page ^ source_offset ^ crc;

    h4v1_stage6_retain->magic = 0u;
    h4v1_stage6_retain->phase = phase;
    h4v1_stage6_retain->logical_block = logical_block;
    h4v1_stage6_retain->physical_block = physical_block;
    h4v1_stage6_retain->page = page;
    h4v1_stage6_retain->source_offset = source_offset;
    h4v1_stage6_retain->running_crc = crc;
    h4v1_stage6_retain->checksum = checksum;
    h4v1_stage6_fence();
    h4v1_stage6_retain->magic = 0x49365336u;
    h4v1_stage6_fence();
    h4v1_stage6_iwdg_feed();
}

uint8_t H4V1_POSTPASS_TEXT h4v1_stage6_selected(uint32_t block)
{
    uint32_t index;
    uint32_t count = h4v1_stage6_selected_count;

    if(count > H4V1_STAGE6_REQUIRED_BLOCKS)
    {
        return 0u;
    }
    for(index = 0u; index < count; ++index)
    {
        if(h4v1_stage6_map[index] == block)
        {
            return 1u;
        }
    }
    return 0u;
}

uint8_t H4V1_POSTPASS_TEXT
h4v1_stage6_write_row_allowed(uint32_t row, uint8_t erase)
{
    uint32_t block = row / H4V1_STAGE6_PAGES_PER_BLOCK;
    uint32_t page = row % H4V1_STAGE6_PAGES_PER_BLOCK;

    if(h4v1_stage6_write_phase == H4V1_STAGE6_WRITE_MANIFEST)
    {
        return (uint8_t)((block == H4V1_STAGE6_MANIFEST_BLOCK) &&
                         ((erase != 0u) ? (page == 0u) : (page <= 1u)));
    }
    if(h4v1_stage6_write_phase == H4V1_STAGE6_WRITE_PAYLOAD)
    {
        return (uint8_t)((block >= H4V1_STAGE6_PAYLOAD_FIRST_BLOCK) &&
                         (block <= H4V1_STAGE6_PAYLOAD_LAST_BLOCK) &&
                         (h4v1_stage6_selected(block) != 0u) &&
                         ((erase == 0u) || (page == 0u)));
    }
    return 0u;
}

void H4V1_POSTPASS_TEXT h4v1_stage6_select(void)
{
    GPIOF->BCR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage6_deselect(void)
{
    GPIOF->BSHR = GPIO_Pin_6;
}

void H4V1_POSTPASS_TEXT h4v1_stage6_drain_rx(void)
{
    uint32_t guard = 16u;

    while((guard != 0u) && ((SPI1->STATR & H4V1_STAGE6_SPI_RXNE) != 0u))
    {
        (void)SPI1->DATAR;
        guard--;
    }
    (void)SPI1->STATR;
}

int H4V1_POSTPASS_TEXT h4v1_stage6_command_allowed(uint8_t command)
{
    return (command == H4V1_STAGE6_CMD_PROGRAM_LOAD) ||
           (command == H4V1_STAGE6_CMD_READ_CACHE) ||
           (command == H4V1_STAGE6_CMD_WRITE_DISABLE) ||
           (command == H4V1_STAGE6_CMD_WRITE_ENABLE) ||
           (command == H4V1_STAGE6_CMD_GET_FEATURE) ||
           (command == H4V1_STAGE6_CMD_PROGRAM_EXECUTE) ||
           (command == H4V1_STAGE6_CMD_PAGE_READ) ||
           (command == H4V1_STAGE6_CMD_SET_FEATURE) ||
           (command == H4V1_STAGE6_CMD_READ_ID) ||
           (command == H4V1_STAGE6_CMD_BLOCK_ERASE);
}

uint8_t H4V1_POSTPASS_TEXT
h4v1_stage6_transfer(h4v1_stage6_result_t *result, uint8_t tx)
{
    uint32_t timeout = H4V1_STAGE6_SPI_BYTE_TIMEOUT;

    while(((SPI1->STATR & H4V1_STAGE6_SPI_TXE) == 0u) && (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        result->spi_timeout_count++;
        return 0xFFu;
    }
    SPI1->DATAR = tx;
    timeout = H4V1_STAGE6_SPI_BYTE_TIMEOUT;
    while(((SPI1->STATR & H4V1_STAGE6_SPI_RXNE) == 0u) && (timeout != 0u))
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
h4v1_stage6_send(h4v1_stage6_result_t *result, uint8_t value,
                 uint32_t timeout_before)
{
    (void)h4v1_stage6_transfer(result, value);
    return (result->spi_timeout_count == timeout_before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_begin(h4v1_stage6_result_t *result, uint8_t command,
                  uint32_t timeout_before)
{
    if(h4v1_stage6_command_allowed(command) == 0)
    {
        result->illegal_command_count++;
        return -1;
    }
    /* A timed-out CPU/DMA transaction may leave RXNE/OVR pending.  Every
     * command starts from a clean receive state before CS is asserted. */
    h4v1_stage6_drain_rx();
    h4v1_stage6_select();
    if(h4v1_stage6_send(result, command, timeout_before) != 0)
    {
        h4v1_stage6_deselect();
        return -1;
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_command_only(h4v1_stage6_result_t *result, uint8_t command)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage6_begin(result, command, before) != 0)
    {
        return -1;
    }
    h4v1_stage6_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_get_feature(h4v1_stage6_result_t *result,
                        uint8_t address, uint8_t *value)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage6_begin(result, H4V1_STAGE6_CMD_GET_FEATURE, before) != 0 ||
       h4v1_stage6_send(result, address, before) != 0)
    {
        h4v1_stage6_deselect();
        return -1;
    }
    *value = h4v1_stage6_transfer(result, 0x00u);
    h4v1_stage6_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_set_a0(h4v1_stage6_result_t *result, uint8_t value)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage6_begin(result, H4V1_STAGE6_CMD_SET_FEATURE, before) != 0 ||
       h4v1_stage6_send(result, H4V1_STAGE6_FEATURE_PROTECTION, before) != 0 ||
       h4v1_stage6_send(result, value, before) != 0)
    {
        h4v1_stage6_deselect();
        return -1;
    }
    h4v1_stage6_deselect();
    return (result->spi_timeout_count == before) ? 0 : -1;
}

int H4V1_POSTPASS_TEXT h4v1_stage6_read_id(h4v1_stage6_result_t *result)
{
    uint32_t before = result->spi_timeout_count;

    if(h4v1_stage6_begin(result, H4V1_STAGE6_CMD_READ_ID, before) != 0 ||
       h4v1_stage6_send(result, 0x00u, before) != 0)
    {
        h4v1_stage6_deselect();
        return -1;
    }
    result->mid = h4v1_stage6_transfer(result, 0x00u);
    result->did = h4v1_stage6_transfer(result, 0x00u);
    h4v1_stage6_deselect();
    if(result->spi_timeout_count != before)
    {
        return -1;
    }
    return ((result->mid == H4V1_STAGE6_EXPECTED_MID) &&
            (result->did == H4V1_STAGE6_EXPECTED_DID)) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_wait_ready(h4v1_stage6_result_t *result, uint8_t *status_out)
{
    uint32_t start = h4v1_stage6_cycle_now();
    uint8_t status = 0xFFu;

    do
    {
        if(h4v1_stage6_get_feature(result,
                                   H4V1_STAGE6_FEATURE_STATUS,
                                   &status) != 0)
        {
            *status_out = status;
            return -1;
        }
        result->ready_polls++;
        if((status & H4V1_STAGE6_STATUS_OIP) == 0u)
        {
            result->status_last = status;
            *status_out = status;
            return 0;
        }
    } while((uint32_t)(h4v1_stage6_cycle_now() - start) <
            H4V1_STAGE6_READY_TIMEOUT);

    result->ready_timeout_count++;
    result->status_last = status;
    *status_out = status;
    return -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_classify_ecc(h4v1_stage6_result_t *result, uint8_t status)
{
    uint8_t ecc = status & H4V1_STAGE6_STATUS_ECC_MASK;
    uint8_t corrected_bits = 0u;

    result->status_last = status;
    if(ecc == H4V1_STAGE6_STATUS_ECC_CLEAN)
    {
        return 0;
    }
    if(ecc == H4V1_STAGE6_STATUS_ECC_FAILED)
    {
        result->ecc_worst_bits = 9u;
        return -2;
    }
    if(ecc == H4V1_STAGE6_STATUS_ECC_8_BITS)
    {
        corrected_bits = 8u;
    }
    else
    {
        uint8_t status2 = 0xFFu;

        if(h4v1_stage6_get_feature(result,
                                   H4V1_STAGE6_FEATURE_STATUS2,
                                   &status2) != 0)
        {
            return -1;
        }
        result->status2_last = status2;
        if((status2 & H4V1_STAGE6_STATUS2_ECCSE_MASK) == 0x00u)
        {
            corrected_bits = 4u;
        }
        else if((status2 & H4V1_STAGE6_STATUS2_ECCSE_MASK) == 0x10u)
        {
            corrected_bits = 5u;
        }
        else if((status2 & H4V1_STAGE6_STATUS2_ECCSE_MASK) == 0x20u)
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
h4v1_stage6_load_page(h4v1_stage6_result_t *result, uint32_t row,
                       uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t start = h4v1_stage6_cycle_now();
    uint8_t status = 0xFFu;
    int ready;

    if(h4v1_stage6_begin(result, H4V1_STAGE6_CMD_PAGE_READ, before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)(row >> 16), before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)(row >> 8), before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)row, before) != 0)
    {
        h4v1_stage6_deselect();
        *cycles_out = h4v1_stage6_cycle_now() - start;
        return -1;
    }
    h4v1_stage6_deselect();
    ready = h4v1_stage6_wait_ready(result, &status);
    *cycles_out = h4v1_stage6_cycle_now() - start;
    if(ready != 0)
    {
        return (ready == -2) ? -2 : -1;
    }
    ready = h4v1_stage6_classify_ecc(result, status);
    return (ready == -2) ? -3 : ready;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_read_cache_cpu(h4v1_stage6_result_t *result,
                           uint16_t column, uint8_t *data,
                           uint32_t length)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t index;

    if(h4v1_stage6_begin(result, H4V1_STAGE6_CMD_READ_CACHE, before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)(column >> 8), before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)column, before) != 0 ||
       h4v1_stage6_send(result, 0x00u, before) != 0)
    {
        h4v1_stage6_deselect();
        return -1;
    }
    for(index = 0u; index < length; ++index)
    {
        data[index] = h4v1_stage6_transfer(result, 0x00u);
        if(result->spi_timeout_count != before)
        {
            h4v1_stage6_deselect();
            return -1;
        }
    }
    h4v1_stage6_deselect();
    return 0;
}

void H4V1_POSTPASS_TEXT h4v1_stage6_dma_stop(void)
{
    DMA1_Channel3->CFGR &= ~H4V1_STAGE6_DMA_EN;
    DMA1_Channel2->CFGR &= ~H4V1_STAGE6_DMA_EN;
    SPI1->CTLR2 &= (uint16_t)~(H4V1_STAGE6_SPI_RXDMAEN |
                                H4V1_STAGE6_SPI_TXDMAEN);
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_dma_stop_wait(h4v1_stage6_result_t *result)
{
    uint32_t start = h4v1_stage6_cycle_now();

    h4v1_stage6_dma_stop();
    while(((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) &
           H4V1_STAGE6_DMA_EN) != 0u)
    {
        if((uint32_t)(h4v1_stage6_cycle_now() - start) >=
           H4V1_STAGE6_DMA_TIMEOUT)
        {
            result->dma_timeout_count++;
            return -1;
        }
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_spi_wait_idle(h4v1_stage6_result_t *result)
{
    uint32_t start = h4v1_stage6_cycle_now();

    while((SPI1->STATR & H4V1_STAGE6_SPI_BSY) != 0u)
    {
        if((uint32_t)(h4v1_stage6_cycle_now() - start) >=
           H4V1_STAGE6_DMA_TIMEOUT)
        {
            result->spi_timeout_count++;
            return -1;
        }
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_dma2_prepare(h4v1_stage6_result_t *result,
                         h4v1_stage6_dma2_saved_t *saved)
{
    uint32_t config = DMA_PeripheralInc_Enable |
                      DMA_MemoryInc_Enable |
                      DMA_Priority_VeryHigh |
                      DMA_M2M_Enable |
                      DMA_PeripheralDataSize_256 |
                      DMA_MemoryDataSize_256;

    saved->clock_was_enabled = RCC->HBPCENR & H4V1_STAGE6_RCC_DMA2;
    RCC->HBPCENR |= H4V1_STAGE6_RCC_DMA2;
    h4v1_stage6_fence();
    saved->cfgr = DMA2_Channel3->CFGR;
    saved->cntr = DMA2_Channel3->CNTR;
    saved->paddr = DMA2_Channel3->PADDR;
    saved->maddr = DMA2_Channel3->MADDR;
    saved->m1addr = DMA2_Channel3->M1ADDR;
    if((saved->cfgr & H4V1_STAGE6_DMA_EN) != 0u ||
       ((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) &
        H4V1_STAGE6_DMA_EN) != 0u)
    {
        result->dma_error_count++;
        if(saved->clock_was_enabled == 0u)
        {
            RCC->HBPCENR &= ~H4V1_STAGE6_RCC_DMA2;
        }
        return -1;
    }
    DMA2_Channel3->CFGR = config;
    DMA2_Channel3->CNTR = 0u;
    DMA2->INTFCR = H4V1_STAGE6_DMA2_GL3;
    h4v1_stage6_fence();
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_dma2_copy_page(h4v1_stage6_result_t *result,
                            uint32_t source_address)
{
    uint32_t start;
    uint32_t flags = 0u;
    uint32_t stop_start;

    if(((source_address & 31u) != 0u) ||
       ((H4V1_STAGE6_DATA_ADDR & 31u) != 0u) ||
       ((DMA1_Channel2->CFGR | DMA1_Channel3->CFGR) &
        H4V1_STAGE6_DMA_EN) != 0u ||
       (SPI1->CTLR2 & (H4V1_STAGE6_SPI_RXDMAEN |
                       H4V1_STAGE6_SPI_TXDMAEN)) != 0u ||
       (DMA2_Channel3->CFGR & H4V1_STAGE6_DMA_EN) != 0u)
    {
        result->dma_error_count++;
        return -1;
    }

    DMA2_Channel3->PADDR = source_address;
    DMA2_Channel3->MADDR = H4V1_STAGE6_DATA_ADDR;
    DMA2_Channel3->CNTR = H4V1_STAGE6_PAGE_BYTES / 32u;
    DMA2->INTFCR = H4V1_STAGE6_DMA2_GL3;
    h4v1_stage6_fence();
    start = h4v1_stage6_cycle_now();
    DMA2_Channel3->CFGR |= H4V1_STAGE6_DMA_EN;
    do
    {
        flags = DMA2->INTFR;
        if((flags & (H4V1_STAGE6_DMA2_TC3 |
                     H4V1_STAGE6_DMA2_TE3)) != 0u)
        {
            break;
        }
    } while((uint32_t)(h4v1_stage6_cycle_now() - start) <
            H4V1_STAGE6_DMA2_TIMEOUT);

    DMA2_Channel3->CFGR &= ~H4V1_STAGE6_DMA_EN;
    stop_start = h4v1_stage6_cycle_now();
    while((DMA2_Channel3->CFGR & H4V1_STAGE6_DMA_EN) != 0u)
    {
        if((uint32_t)(h4v1_stage6_cycle_now() - stop_start) >=
           H4V1_STAGE6_DMA2_TIMEOUT)
        {
            result->dma_timeout_count++;
            DMA2->INTFCR = H4V1_STAGE6_DMA2_GL3;
            return -1;
        }
    }
    flags |= DMA2->INTFR;
    DMA2->INTFCR = H4V1_STAGE6_DMA2_GL3;
    h4v1_stage6_fence();
    if(((flags & H4V1_STAGE6_DMA2_TC3) == 0u) ||
       ((flags & H4V1_STAGE6_DMA2_TE3) != 0u))
    {
        if((flags & H4V1_STAGE6_DMA2_TE3) != 0u)
        {
            result->dma_error_count++;
        }
        else
        {
            result->dma_timeout_count++;
        }
        return -1;
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_dma2_restore(const h4v1_stage6_dma2_saved_t *saved)
{
    uint32_t start = h4v1_stage6_cycle_now();

    DMA2_Channel3->CFGR &= ~H4V1_STAGE6_DMA_EN;
    while((DMA2_Channel3->CFGR & H4V1_STAGE6_DMA_EN) != 0u)
    {
        if((uint32_t)(h4v1_stage6_cycle_now() - start) >=
           H4V1_STAGE6_DMA2_TIMEOUT)
        {
            return -1;
        }
    }
    DMA2->INTFCR = H4V1_STAGE6_DMA2_GL3;
    DMA2_Channel3->CFGR = saved->cfgr & ~H4V1_STAGE6_DMA_EN;
    DMA2_Channel3->CNTR = saved->cntr;
    DMA2_Channel3->PADDR = saved->paddr;
    DMA2_Channel3->MADDR = saved->maddr;
    DMA2_Channel3->M1ADDR = saved->m1addr;
    if(saved->clock_was_enabled == 0u)
    {
        RCC->HBPCENR &= ~H4V1_STAGE6_RCC_DMA2;
    }
    h4v1_stage6_fence();
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_dma_payload(h4v1_stage6_result_t *result,
                        uint8_t command, uint8_t address_bytes,
                        uint8_t address0, uint8_t address1, uint8_t address2,
                        uint32_t rx_address, uint8_t rx_increment,
                        uint32_t tx_address, uint8_t tx_increment,
                        uint32_t length, uint32_t *cycles_out)
{
    uint32_t before = result->spi_timeout_count;
    uint32_t start;

    if(!(((command == H4V1_STAGE6_CMD_PROGRAM_LOAD) &&
          (address_bytes == 2u) &&
          (address0 == 0u) && (address1 == 0u)) ||
         ((command == H4V1_STAGE6_CMD_READ_CACHE) &&
          (address_bytes == 3u) &&
          (address0 == 0u) && (address1 == 0u) && (address2 == 0u))))
    {
        result->illegal_command_count++;
        *cycles_out = 0u;
        return -1;
    }

    if(h4v1_stage6_dma_stop_wait(result) != 0)
    {
        *cycles_out = 0u;
        return -2;
    }
    if((DMA2_Channel3->CFGR & H4V1_STAGE6_DMA_EN) != 0u)
    {
        result->dma_error_count++;
        *cycles_out = 0u;
        return -2;
    }
    h4v1_stage6_drain_rx();
    DMA1_Channel2->CFGR = H4V1_STAGE6_DMA_PRIORITY_VERY_HIGH |
        ((rx_increment != 0u) ? H4V1_STAGE6_DMA_MINC : 0u);
    DMA1_Channel2->MADDR = rx_address;
    DMA1_Channel2->CNTR = length;
    DMA1_Channel3->CFGR = H4V1_STAGE6_DMA_PRIORITY_VERY_HIGH |
        H4V1_STAGE6_DMA_DIR |
        ((tx_increment != 0u) ? H4V1_STAGE6_DMA_MINC : 0u);
    DMA1_Channel3->MADDR = tx_address;
    DMA1_Channel3->CNTR = length;
    DMA1->INTFCR = H4V1_STAGE6_DMA_GL2 | H4V1_STAGE6_DMA_GL3;
    start = h4v1_stage6_cycle_now();

    if(h4v1_stage6_begin(result, command, before) != 0 ||
       ((address_bytes > 0u) &&
        (h4v1_stage6_send(result, address0, before) != 0)) ||
       ((address_bytes > 1u) &&
        (h4v1_stage6_send(result, address1, before) != 0)) ||
       ((address_bytes > 2u) &&
        (h4v1_stage6_send(result, address2, before) != 0)))
    {
        h4v1_stage6_deselect();
        *cycles_out = h4v1_stage6_cycle_now() - start;
        return -1;
    }
    SPI1->CTLR2 |= H4V1_STAGE6_SPI_RXDMAEN | H4V1_STAGE6_SPI_TXDMAEN;
    h4v1_stage6_fence();
    DMA1_Channel2->CFGR |= H4V1_STAGE6_DMA_EN;
    DMA1_Channel3->CFGR |= H4V1_STAGE6_DMA_EN;

    while((DMA1->INTFR & H4V1_STAGE6_DMA_DONE_MASK) !=
          H4V1_STAGE6_DMA_DONE_MASK)
    {
        if((DMA1->INTFR & H4V1_STAGE6_DMA_ERROR_MASK) != 0u)
        {
            result->dma_error_count++;
            goto dma_failed;
        }
        if((uint32_t)(h4v1_stage6_cycle_now() - start) >=
           H4V1_STAGE6_DMA_TIMEOUT)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    while((SPI1->STATR & H4V1_STAGE6_SPI_BSY) != 0u)
    {
        if((uint32_t)(h4v1_stage6_cycle_now() - start) >=
           H4V1_STAGE6_DMA_TIMEOUT)
        {
            result->dma_timeout_count++;
            goto dma_failed;
        }
    }
    if((DMA1->INTFR & H4V1_STAGE6_DMA_ERROR_MASK) != 0u)
    {
        result->dma_error_count++;
        goto dma_failed;
    }
    if(h4v1_stage6_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
    if((DMA1->INTFR & H4V1_STAGE6_DMA_ERROR_MASK) != 0u)
    {
        result->dma_error_count++;
        h4v1_stage6_deselect();
        DMA1->INTFCR = H4V1_STAGE6_DMA_GL2 | H4V1_STAGE6_DMA_GL3;
        *cycles_out = h4v1_stage6_cycle_now() - start;
        return -1;
    }
    h4v1_stage6_deselect();
    DMA1->INTFCR = H4V1_STAGE6_DMA_GL2 | H4V1_STAGE6_DMA_GL3;
    h4v1_stage6_fence();
    *cycles_out = h4v1_stage6_cycle_now() - start;
    return 0;

dma_failed:
    if(h4v1_stage6_dma_stop_wait(result) != 0)
    {
        goto dma_stuck;
    }
    h4v1_stage6_deselect();
    DMA1->INTFCR = H4V1_STAGE6_DMA_GL2 | H4V1_STAGE6_DMA_GL3;
    *cycles_out = h4v1_stage6_cycle_now() - start;
    return -1;

dma_stuck:
    h4v1_stage6_deselect();
    DMA1->INTFCR = H4V1_STAGE6_DMA_GL2 | H4V1_STAGE6_DMA_GL3;
    *cycles_out = h4v1_stage6_cycle_now() - start;
    return -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_read_cache_dma(h4v1_stage6_result_t *result,
                           uint8_t *data, uint32_t *cycles_out)
{
    *(volatile uint8_t *)(uintptr_t)H4V1_STAGE6_AUX_ADDR = 0u;
    return h4v1_stage6_dma_payload(
        result, H4V1_STAGE6_CMD_READ_CACHE, 3u, 0u, 0u, 0u,
        (uint32_t)(uintptr_t)data, 1u,
        H4V1_STAGE6_AUX_ADDR, 0u,
        H4V1_STAGE6_PAGE_BYTES, cycles_out);
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_program_load_dma(h4v1_stage6_result_t *result,
                             const uint8_t *data, uint32_t *cycles_out)
{
    return h4v1_stage6_dma_payload(
        result, H4V1_STAGE6_CMD_PROGRAM_LOAD, 2u, 0u, 0u, 0u,
        H4V1_STAGE6_AUX_ADDR, 0u,
        (uint32_t)(uintptr_t)data, 1u,
        H4V1_STAGE6_PAGE_BYTES, cycles_out);
}

int H4V1_POSTPASS_TEXT h4v1_stage6_write_enable(h4v1_stage6_result_t *result)
{
    uint8_t status = 0xFFu;

    if(h4v1_stage6_command_only(result, H4V1_STAGE6_CMD_WRITE_ENABLE) != 0 ||
       h4v1_stage6_get_feature(result,
                               H4V1_STAGE6_FEATURE_STATUS,
                               &status) != 0)
    {
        return -1;
    }
    result->status_last = status;
    return ((status & H4V1_STAGE6_STATUS_WEL) != 0u) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT h4v1_stage6_write_disable(h4v1_stage6_result_t *result)
{
    uint8_t status = 0xFFu;

    if(h4v1_stage6_command_only(result, H4V1_STAGE6_CMD_WRITE_DISABLE) != 0 ||
       h4v1_stage6_get_feature(result,
                               H4V1_STAGE6_FEATURE_STATUS,
                               &status) != 0)
    {
        return -1;
    }
    result->status_last = status;
    return ((status & H4V1_STAGE6_STATUS_WEL) == 0u) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_erase(h4v1_stage6_result_t *result, uint32_t row,
                  uint32_t *cycles_out)
{
    uint32_t before;
    uint32_t start = h4v1_stage6_cycle_now();
    uint8_t status = 0xFFu;
    int ready;

    if(h4v1_stage6_write_row_allowed(row, 1u) == 0u)
    {
        result->illegal_command_count++;
        *cycles_out = 0u;
        return -4;
    }

    if(h4v1_stage6_write_enable(result) != 0)
    {
        *cycles_out = h4v1_stage6_cycle_now() - start;
        return -1;
    }
    before = result->spi_timeout_count;
    if(h4v1_stage6_begin(result, H4V1_STAGE6_CMD_BLOCK_ERASE, before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)(row >> 16), before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)(row >> 8), before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)row, before) != 0)
    {
        h4v1_stage6_deselect();
        *cycles_out = h4v1_stage6_cycle_now() - start;
        return -1;
    }
    h4v1_stage6_deselect();
    ready = h4v1_stage6_wait_ready(result, &status);
    *cycles_out = h4v1_stage6_cycle_now() - start;
    if(ready != 0)
    {
        return (ready == -2) ? -2 : -1;
    }
    return ((status & H4V1_STAGE6_STATUS_E_FAIL) == 0u) ? 0 : -3;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_program_execute(h4v1_stage6_result_t *result, uint32_t row,
                            uint32_t load_cycles, uint32_t *cycles_out)
{
    uint32_t before;
    uint32_t start = h4v1_stage6_cycle_now();
    uint8_t status = 0xFFu;
    int ready;

    if(h4v1_stage6_write_row_allowed(row, 0u) == 0u)
    {
        result->illegal_command_count++;
        *cycles_out = load_cycles;
        return -4;
    }

    /* Required ordering: 02 PROGRAM LOAD completed before this 06 WREN. */
    if(h4v1_stage6_write_enable(result) != 0)
    {
        *cycles_out = load_cycles + (h4v1_stage6_cycle_now() - start);
        return -1;
    }
    before = result->spi_timeout_count;
    if(h4v1_stage6_begin(result,
                         H4V1_STAGE6_CMD_PROGRAM_EXECUTE,
                         before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)(row >> 16), before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)(row >> 8), before) != 0 ||
       h4v1_stage6_send(result, (uint8_t)row, before) != 0)
    {
        h4v1_stage6_deselect();
        *cycles_out = load_cycles + (h4v1_stage6_cycle_now() - start);
        return -1;
    }
    h4v1_stage6_deselect();
    ready = h4v1_stage6_wait_ready(result, &status);
    *cycles_out = load_cycles + (h4v1_stage6_cycle_now() - start);
    if(ready != 0)
    {
        return (ready == -2) ? -2 : -1;
    }
    return ((status & H4V1_STAGE6_STATUS_P_FAIL) == 0u) ? 0 : -3;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_read_marker_at(h4v1_stage6_result_t *result, uint32_t block,
                            uint8_t *marker)
{
    uint32_t cycles = 0u;
    uint32_t row;

    if((block < H4V1_STAGE6_MANIFEST_BLOCK) ||
       (block > H4V1_STAGE6_PAYLOAD_LAST_BLOCK))
    {
        result->illegal_command_count++;
        return -1;
    }
    row = block * H4V1_STAGE6_PAGES_PER_BLOCK;
    {
        int status = h4v1_stage6_load_page(result, row, &cycles);

        if(status != 0)
        {
            return status;
        }
    }
    return h4v1_stage6_read_cache_cpu(result,
                                       H4V1_STAGE6_BAD_MARK_COLUMN,
                                       marker, 1u);
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_read_page(h4v1_stage6_result_t *result, uint32_t row,
                       uint8_t *data)
{
    uint32_t cycles = 0u;
    int status;

    status = h4v1_stage6_load_page(result, row, &cycles);
    if(status != 0)
    {
        return status;
    }
    return h4v1_stage6_read_cache_dma(result, data, &cycles);
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_program_page(h4v1_stage6_result_t *result, uint32_t row,
                          uint8_t *data, uint32_t expected_crc)
{
    uint32_t load_cycles = 0u;
    uint32_t program_cycles = 0u;
    uint32_t actual_crc;
    int status;

    if(h4v1_stage6_write_row_allowed(row, 0u) == 0u)
    {
        result->illegal_command_count++;
        return -1;
    }
    status = h4v1_stage6_program_load_dma(result, data, &load_cycles);
    if(status != 0)
    {
        return -1;
    }
    status = h4v1_stage6_program_execute(result, row, load_cycles,
                                          &program_cycles);
    if(status != 0)
    {
        return -2;
    }
    status = h4v1_stage6_read_page(result, row, data);
    if(status != 0)
    {
        return -3;
    }
    actual_crc = h4v1_stage6_crc32_update(0u, data,
                                          H4V1_STAGE6_PAGE_BYTES);
    return (actual_crc == expected_crc) ? 0 : -4;
}

uint32_t H4V1_POSTPASS_TEXT
h4v1_stage6_header_crc(const uint8_t *header)
{
    uint32_t crc = 0u;
    uint32_t index;

    for(index = 0u; index < 64u; ++index)
    {
        uint8_t value = ((index >= 56u) && (index < 60u)) ?
                        0u : header[index];
        crc = h4v1_stage6_crc32_update(crc, &value, 1u);
    }
    return crc;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_header_contract(const uint8_t *header)
{
    if((header[0] != 'H') || (header[1] != '4') ||
       (header[2] != 'V') || (header[3] != '1') ||
       (h4v1_stage6_get16(&header[4]) != 1u) ||
       (h4v1_stage6_get16(&header[6]) != 64u) ||
       (h4v1_stage6_get16(&header[8]) != H4V1_STAGE6_WIDTH) ||
       (h4v1_stage6_get16(&header[10]) != H4V1_STAGE6_HEIGHT) ||
       (h4v1_stage6_get16(&header[12]) != H4V1_STAGE6_FPS) ||
       (h4v1_stage6_get16(&header[14]) != 1u) ||
       (h4v1_stage6_get32(&header[16]) != H4V1_STAGE6_FRAME_COUNT) ||
       (h4v1_stage6_get32(&header[20]) != H4V1_STAGE6_FRAME_BYTES) ||
       (h4v1_stage6_get32(&header[24]) != 1u) ||
       (h4v1_stage6_get32(&header[28]) != H4V1_STAGE6_FLAGS) ||
       (h4v1_stage6_get32(&header[32]) != 64u) ||
       (h4v1_stage6_get32(&header[36]) != 24u) ||
       (h4v1_stage6_get32(&header[40]) != H4V1_STAGE6_DATA_OFFSET) ||
       (h4v1_stage6_get32(&header[44]) != H4V1_STAGE6_CONTAINER_BYTES) ||
       (h4v1_stage6_get32(&header[60]) != 0u) ||
       (h4v1_stage6_header_crc(header) !=
        h4v1_stage6_get32(&header[56])))
    {
        return -1;
    }
    return 0;
}

void H4V1_POSTPASS_TEXT h4v1_stage6_apply_50mhz(void)
{
    uint16_t ctlr1 = SPI1->CTLR1;
    uint16_t hscr = SPI1->HSCR;

    SPI1->CTLR1 = ctlr1 & (uint16_t)~H4V1_STAGE6_SPI_SPE;
    h4v1_stage6_drain_rx();
    hscr &= (uint16_t)~(H4V1_STAGE6_SPI_HSRXEN |
                        H4V1_STAGE6_SPI_HSRXEN2);
    SPI1->HSCR = hscr;
    ctlr1 &= (uint16_t)~H4V1_STAGE6_SPI_BR_MASK;
    SPI1->CTLR1 = ctlr1 | H4V1_STAGE6_SPI_SPE;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_save_and_configure(h4v1_stage6_saved_t *saved)
{
    saved->dma_clock_was_enabled = RCC->HBPCENR & H4V1_STAGE6_RCC_DMA1;
    RCC->HBPCENR |= H4V1_STAGE6_RCC_DMA1;
    h4v1_stage6_fence();
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
    if(((saved->dma2_cfgr | saved->dma3_cfgr) & H4V1_STAGE6_DMA_EN) != 0u)
    {
        if(saved->dma_clock_was_enabled == 0u)
        {
            RCC->HBPCENR &= ~H4V1_STAGE6_RCC_DMA1;
        }
        return -1;
    }
    h4v1_stage6_dma_stop();
    DMA1_Channel2->CNTR = 0u;
    DMA1_Channel2->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel2->MADDR = H4V1_STAGE6_DATA_ADDR;
    DMA1_Channel2->M1ADDR = 0u;
    DMA1_Channel3->CNTR = 0u;
    DMA1_Channel3->PADDR = (uint32_t)(uintptr_t)&SPI1->DATAR;
    DMA1_Channel3->MADDR = H4V1_STAGE6_AUX_ADDR;
    DMA1_Channel3->M1ADDR = 0u;
    DMAMUX->CFGR0_3 =
        (saved->dmamux_cfgr0_3 &
         ~((H4V1_STAGE6_DMAMUX_FIELD_MASK << H4V1_STAGE6_DMAMUX_CH2_SHIFT) |
           (H4V1_STAGE6_DMAMUX_FIELD_MASK << H4V1_STAGE6_DMAMUX_CH3_SHIFT))) |
        ((H4V1_STAGE6_DMA_REQUEST_SPI1_RX - 1u) <<
         H4V1_STAGE6_DMAMUX_CH2_SHIFT) |
        ((H4V1_STAGE6_DMA_REQUEST_SPI1_TX - 1u) <<
         H4V1_STAGE6_DMAMUX_CH3_SHIFT);
    *(volatile uint8_t *)(uintptr_t)H4V1_STAGE6_AUX_ADDR = 0u;
    DMA1->INTFCR = H4V1_STAGE6_DMA_GL2 | H4V1_STAGE6_DMA_GL3;
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_restore(const h4v1_stage6_saved_t *saved)
{
    h4v1_stage6_result_t stop_result = {0};

    if(((saved->dma2_cfgr | saved->dma3_cfgr) & H4V1_STAGE6_DMA_EN) != 0u)
    {
        if(saved->dma_clock_was_enabled == 0u)
        {
            RCC->HBPCENR &= ~H4V1_STAGE6_RCC_DMA1;
        }
        return 0;
    }
    if(h4v1_stage6_dma_stop_wait(&stop_result) != 0)
    {
        h4v1_stage6_deselect();
        return -1;
    }
    h4v1_stage6_deselect();
    if(h4v1_stage6_spi_wait_idle(&stop_result) != 0)
    {
        h4v1_stage6_drain_rx();
        return -1;
    }
    h4v1_stage6_drain_rx();
    DMA1->INTFCR = H4V1_STAGE6_DMA_GL2 | H4V1_STAGE6_DMA_GL3;
    DMA1_Channel2->CFGR = saved->dma2_cfgr & ~H4V1_STAGE6_DMA_EN;
    DMA1_Channel2->CNTR = saved->dma2_cntr;
    DMA1_Channel2->PADDR = saved->dma2_paddr;
    DMA1_Channel2->MADDR = saved->dma2_maddr;
    DMA1_Channel2->M1ADDR = saved->dma2_m1addr;
    DMA1_Channel3->CFGR = saved->dma3_cfgr & ~H4V1_STAGE6_DMA_EN;
    DMA1_Channel3->CNTR = saved->dma3_cntr;
    DMA1_Channel3->PADDR = saved->dma3_paddr;
    DMA1_Channel3->MADDR = saved->dma3_maddr;
    DMA1_Channel3->M1ADDR = saved->dma3_m1addr;
    DMAMUX->CFGR0_3 = saved->dmamux_cfgr0_3;
    SPI1->CTLR1 &= (uint16_t)~H4V1_STAGE6_SPI_SPE;
    h4v1_stage6_drain_rx();
    SPI1->HSCR = saved->spi_hscr;
    SPI1->CTLR2 = saved->spi_ctlr2;
    SPI1->CTLR1 = saved->spi_ctlr1;
    if(saved->dma_clock_was_enabled == 0u)
    {
        RCC->HBPCENR &= ~H4V1_STAGE6_RCC_DMA1;
    }
    h4v1_stage6_fence();
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_source_preflight(h4v1_stage6_result_t *result)
{
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE6_DATA_ADDR;
    uint32_t transfer_crc = 0u;
    uint32_t container_crc = 0u;
    uint32_t index_crc = 0u;
    uint32_t expected_index_crc = 0u;
    uint32_t page;

    for(page = 0u; page < H4V1_STAGE6_REQUIRED_PAGES; ++page)
    {
        uint32_t offset = page * H4V1_STAGE6_PAGE_BYTES;
        uint32_t container_bytes = 0u;
        uint32_t overlap_start;
        uint32_t overlap_end;
        const uint32_t index_start = 64u;
        const uint32_t index_end =
            64u + H4V1_STAGE6_FRAME_COUNT * 24u;

        if(h4v1_stage6_dma2_copy_page(
               result, H4V1_STAGE6_SOURCE_ADDR + offset) != 0)
        {
            return -1;
        }
        if(page == 0u)
        {
            if(h4v1_stage6_header_contract(data) != 0)
            {
                return -2;
            }
            result->raw_stream_crc = h4v1_stage6_get32(&data[48]);
            expected_index_crc = h4v1_stage6_get32(&data[52]);
            result->header_crc = h4v1_stage6_get32(&data[56]);
        }

        transfer_crc = h4v1_stage6_crc32_update(
            transfer_crc, data, H4V1_STAGE6_PAGE_BYTES);
        if(offset < H4V1_STAGE6_CONTAINER_BYTES)
        {
            container_bytes = H4V1_STAGE6_CONTAINER_BYTES - offset;
            if(container_bytes > H4V1_STAGE6_PAGE_BYTES)
            {
                container_bytes = H4V1_STAGE6_PAGE_BYTES;
            }
            container_crc = h4v1_stage6_crc32_update(
                container_crc, data, container_bytes);
        }

        overlap_start = (offset > index_start) ? offset : index_start;
        overlap_end = ((offset + H4V1_STAGE6_PAGE_BYTES) < index_end) ?
                      (offset + H4V1_STAGE6_PAGE_BYTES) : index_end;
        if(overlap_start < overlap_end)
        {
            index_crc = h4v1_stage6_crc32_update(
                index_crc,
                &data[overlap_start - offset],
                overlap_end - overlap_start);
        }
        if((page & 0xFFu) == 0xFFu)
        {
            h4v1_stage6_checkpoint(1u, 0u, 0u, page,
                                    offset + H4V1_STAGE6_PAGE_BYTES,
                                    transfer_crc);
        }
    }

    result->source_crc = transfer_crc;
    result->index_crc = index_crc;
    if((transfer_crc != H4V1_STAGE6_TRANSFER_CRC) ||
       (container_crc != H4V1_STAGE6_CONTAINER_CRC) ||
       (index_crc != expected_index_crc))
    {
        return -3;
    }
    h4v1_stage6_checkpoint(1u, 0u, 0u, H4V1_STAGE6_REQUIRED_PAGES,
                            H4V1_STAGE6_TRANSFER_BYTES, transfer_crc);
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_build_plan(h4v1_stage6_result_t *result)
{
    uint32_t block;
    uint32_t good = 0u;
    uint8_t marker = 0u;

    h4v1_stage6_selected_count = 0u;
    if(h4v1_stage6_read_marker_at(result,
                                   H4V1_STAGE6_MANIFEST_BLOCK,
                                   &marker) != 0 ||
       marker != 0xFFu)
    {
        return -1;
    }

    for(block = H4V1_STAGE6_PAYLOAD_FIRST_BLOCK;
        block <= H4V1_STAGE6_PAYLOAD_LAST_BLOCK;
        ++block)
    {
        int marker_status;

        marker = 0u;
        marker_status = h4v1_stage6_read_marker_at(
            result, block, &marker);
        if(marker_status == -3)
        {
            result->scan_unreadable_blocks++;
            h4v1_stage6_iwdg_feed();
            continue;
        }
        if(marker_status != 0)
        {
            return -2;
        }
        if(marker == 0xFFu)
        {
            if(good < H4V1_STAGE6_CANDIDATE_BLOCKS)
            {
                h4v1_stage6_candidates[good] = (uint16_t)block;
            }
            good++;
        }
        h4v1_stage6_iwdg_feed();
    }
    if(good < H4V1_STAGE6_REQUIRED_BLOCKS)
    {
        result->factory_bad_blocks =
            H4V1_STAGE6_CANDIDATE_BLOCKS - good;
        return -3;
    }
    for(block = 0u; block < H4V1_STAGE6_REQUIRED_BLOCKS; ++block)
    {
        h4v1_stage6_map[block] = h4v1_stage6_candidates[block];
        h4v1_stage6_block_crc[block] = 0u;
    }
    h4v1_stage6_fence();
    h4v1_stage6_selected_count = H4V1_STAGE6_REQUIRED_BLOCKS;
    h4v1_stage6_fence();
    result->factory_bad_blocks = H4V1_STAGE6_CANDIDATE_BLOCKS - good;
    result->selected_blocks = H4V1_STAGE6_REQUIRED_BLOCKS;
    result->ecc_worst_bits = 0u;
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_unlock(h4v1_stage6_result_t *result)
{
    uint8_t value = 0xFFu;

    if(h4v1_stage6_set_a0(result, H4V1_STAGE6_A0_TEMP) != 0 ||
       h4v1_stage6_get_feature(result,
                                H4V1_STAGE6_FEATURE_PROTECTION,
                                &value) != 0 ||
       value != H4V1_STAGE6_A0_TEMP)
    {
        return -1;
    }
    result->a0_temp_readback = value;
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_lock(h4v1_stage6_result_t *result)
{
    uint8_t a0 = 0xFFu;
    uint8_t b0 = 0xFFu;
    uint8_t status = 0xFFu;

    if(h4v1_stage6_write_disable(result) != 0 ||
       h4v1_stage6_set_a0(result, result->a0_saved) != 0 ||
       h4v1_stage6_get_feature(result,
                                H4V1_STAGE6_FEATURE_PROTECTION,
                                &a0) != 0 ||
       h4v1_stage6_get_feature(result,
                                H4V1_STAGE6_FEATURE_CONFIG,
                                &b0) != 0 ||
       h4v1_stage6_get_feature(result,
                                H4V1_STAGE6_FEATURE_STATUS,
                                &status) != 0)
    {
        return -1;
    }
    result->a0_restored = a0;
    result->b0_readback = b0;
    result->status_last = status;
    return ((a0 == H4V1_STAGE6_A0_EXPECTED_LOCKED) &&
            (b0 == result->b0_saved) &&
            ((status & H4V1_STAGE6_STATUS_WEL) == 0u)) ? 0 : -2;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_page_is_ff(const uint8_t *data)
{
    uint32_t index;

    for(index = 0u; index < H4V1_STAGE6_PAGE_BYTES; ++index)
    {
        if(data[index] != 0xFFu)
        {
            return -1;
        }
    }
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_invalidate_manifest(h4v1_stage6_result_t *result)
{
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE6_DATA_ADDR;
    uint32_t cycles = 0u;
    uint32_t row =
        H4V1_STAGE6_MANIFEST_BLOCK * H4V1_STAGE6_PAGES_PER_BLOCK;

    h4v1_stage6_write_phase = H4V1_STAGE6_WRITE_MANIFEST;
    h4v1_stage6_fence();
    if(h4v1_stage6_unlock(result) != 0 ||
       h4v1_stage6_erase(result, row, &cycles) != 0 ||
       h4v1_stage6_read_page(result, row, data) != 0 ||
       h4v1_stage6_page_is_ff(data) != 0 ||
       h4v1_stage6_read_page(result, row + 1u, data) != 0 ||
       h4v1_stage6_page_is_ff(data) != 0 ||
       h4v1_stage6_lock(result) != 0)
    {
        return -1;
    }
    h4v1_stage6_checkpoint(3u, 0u, H4V1_STAGE6_MANIFEST_BLOCK,
                            0u, 0u, 0u);
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_program_payload(h4v1_stage6_result_t *result)
{
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE6_DATA_ADDR;
    uint32_t logical;

    h4v1_stage6_write_phase = H4V1_STAGE6_WRITE_PAYLOAD;
    h4v1_stage6_fence();
    result->bytes_programmed = 0u;
    result->pages_programmed = 0u;
    for(logical = 0u; logical < H4V1_STAGE6_REQUIRED_BLOCKS; ++logical)
    {
        uint32_t physical = h4v1_stage6_map[logical];
        uint32_t pages = (logical + 1u == H4V1_STAGE6_REQUIRED_BLOCKS) ?
                         H4V1_STAGE6_LAST_BLOCK_PAGES :
                         H4V1_STAGE6_PAGES_PER_BLOCK;
        uint32_t block_crc = 0u;
        uint32_t page;
        uint32_t cycles = 0u;

        result->current_logical_block = logical;
        result->current_physical_block = physical;
        result->current_page = 0u;
        if(h4v1_stage6_unlock(result) != 0 ||
           h4v1_stage6_erase(
               result, physical * H4V1_STAGE6_PAGES_PER_BLOCK,
               &cycles) != 0)
        {
            return -1;
        }
        for(page = 0u; page < pages; ++page)
        {
            uint32_t source_offset =
                logical * H4V1_STAGE6_PAGES_PER_BLOCK *
                    H4V1_STAGE6_PAGE_BYTES +
                page * H4V1_STAGE6_PAGE_BYTES;
            uint32_t page_crc;

            result->current_page = page;
            if(h4v1_stage6_dma2_copy_page(
                   result, H4V1_STAGE6_SOURCE_ADDR + source_offset) != 0)
            {
                return -2;
            }
            page_crc = h4v1_stage6_crc32_update(
                0u, data, H4V1_STAGE6_PAGE_BYTES);
            block_crc = h4v1_stage6_crc32_update(
                block_crc, data, H4V1_STAGE6_PAGE_BYTES);
            if(h4v1_stage6_program_page(
                   result,
                   physical * H4V1_STAGE6_PAGES_PER_BLOCK + page,
                   data, page_crc) != 0)
            {
                return -3;
            }
            result->pages_programmed++;
            result->bytes_programmed += H4V1_STAGE6_PAGE_BYTES;
            h4v1_stage6_checkpoint(4u, logical, physical, page,
                                    source_offset +
                                        H4V1_STAGE6_PAGE_BYTES,
                                    block_crc);
        }
        h4v1_stage6_block_crc[logical] = block_crc;
        h4v1_stage6_fence();
        if(h4v1_stage6_lock(result) != 0)
        {
            return -4;
        }
        if(((logical & 7u) == 7u) ||
           (logical + 1u == H4V1_STAGE6_REQUIRED_BLOCKS))
        {
            char line[192];
            (void)rt_snprintf(
                line, sizeof(line), h4v1_stage6_program_format,
                (unsigned int)(logical + 1u),
                (unsigned int)result->pages_programmed,
                (unsigned int)result->bytes_programmed,
                (unsigned int)physical,
                (unsigned int)block_crc);
            h4v1_postpass_stage1_log(line);
        }
    }
    return (result->bytes_programmed == H4V1_STAGE6_TRANSFER_BYTES) ?
           0 : -5;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_verify_payload(h4v1_stage6_result_t *result)
{
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE6_DATA_ADDR;
    uint32_t whole_crc = 0u;
    uint32_t logical;

    result->bytes_verified = 0u;
    result->pages_verified = 0u;
    for(logical = 0u; logical < H4V1_STAGE6_REQUIRED_BLOCKS; ++logical)
    {
        uint32_t physical = h4v1_stage6_map[logical];
        uint32_t pages = (logical + 1u == H4V1_STAGE6_REQUIRED_BLOCKS) ?
                         H4V1_STAGE6_LAST_BLOCK_PAGES :
                         H4V1_STAGE6_PAGES_PER_BLOCK;
        uint32_t block_crc = 0u;
        uint32_t page;

        result->current_logical_block = logical;
        result->current_physical_block = physical;
        for(page = 0u; page < pages; ++page)
        {
            uint32_t row =
                physical * H4V1_STAGE6_PAGES_PER_BLOCK + page;

            result->current_page = page;
            if(h4v1_stage6_read_page(result, row, data) != 0)
            {
                return -1;
            }
            block_crc = h4v1_stage6_crc32_update(
                block_crc, data, H4V1_STAGE6_PAGE_BYTES);
            whole_crc = h4v1_stage6_crc32_update(
                whole_crc, data, H4V1_STAGE6_PAGE_BYTES);
            result->pages_verified++;
            result->bytes_verified += H4V1_STAGE6_PAGE_BYTES;
            h4v1_stage6_checkpoint(5u, logical, physical, page,
                                    result->bytes_verified, whole_crc);
        }
        if(block_crc != h4v1_stage6_block_crc[logical])
        {
            return -2;
        }
        if(((logical & 7u) == 7u) ||
           (logical + 1u == H4V1_STAGE6_REQUIRED_BLOCKS))
        {
            char line[192];
            (void)rt_snprintf(
                line, sizeof(line), h4v1_stage6_verify_format,
                (unsigned int)(logical + 1u),
                (unsigned int)result->pages_verified,
                (unsigned int)result->bytes_verified,
                (unsigned int)whole_crc,
                (unsigned int)result->ecc_worst_bits);
            h4v1_postpass_stage1_log(line);
        }
    }
    result->flash_crc = whole_crc;
    return ((result->bytes_verified == H4V1_STAGE6_TRANSFER_BYTES) &&
            (whole_crc == H4V1_STAGE6_TRANSFER_CRC)) ? 0 : -3;
}

uint32_t H4V1_POSTPASS_TEXT
h4v1_stage6_build_descriptor(uint8_t *data,
                              const h4v1_stage6_result_t *result)
{
    uint32_t index;
    uint32_t crc;

    h4v1_stage6_fill(data, 0xFFu, H4V1_STAGE6_PAGE_BYTES);
    h4v1_stage6_put32(&data[0], H4V1_STAGE6_DESCRIPTOR_MAGIC);
    h4v1_stage6_put32(&data[4], H4V1_STAGE6_MANIFEST_VERSION);
    h4v1_stage6_put32(&data[8], H4V1_STAGE6_GENERATION);
    h4v1_stage6_put32(&data[12], H4V1_STAGE6_PAGE_BYTES);
    h4v1_stage6_put32(&data[16], H4V1_STAGE6_TRANSFER_BYTES);
    h4v1_stage6_put32(&data[20], H4V1_STAGE6_TRANSFER_CRC);
    h4v1_stage6_put32(&data[24], H4V1_STAGE6_CONTAINER_BYTES);
    h4v1_stage6_put32(&data[28], H4V1_STAGE6_CONTAINER_CRC);
    h4v1_stage6_put32(&data[32], H4V1_STAGE6_FRAME_COUNT);
    h4v1_stage6_put32(&data[36], H4V1_STAGE6_FPS);
    h4v1_stage6_put32(&data[40], H4V1_STAGE6_WIDTH);
    h4v1_stage6_put32(&data[44], H4V1_STAGE6_HEIGHT);
    h4v1_stage6_put32(&data[48], H4V1_STAGE6_FRAME_BYTES);
    h4v1_stage6_put32(&data[52], 1u);
    h4v1_stage6_put32(&data[56], H4V1_STAGE6_FLAGS);
    h4v1_stage6_put32(&data[60], H4V1_STAGE6_DATA_OFFSET);
    h4v1_stage6_put32(&data[64], H4V1_STAGE6_REQUIRED_BLOCKS);
    h4v1_stage6_put32(&data[68], H4V1_STAGE6_PAYLOAD_FIRST_BLOCK);
    h4v1_stage6_put32(&data[72], H4V1_STAGE6_PAYLOAD_LAST_BLOCK);
    h4v1_stage6_put32(&data[76], H4V1_STAGE6_DESCRIPTOR_MAP_OFFSET);
    h4v1_stage6_put32(
        &data[80], H4V1_STAGE6_DESCRIPTOR_BLOCK_CRC_OFFSET);
    h4v1_stage6_put32(&data[84], result->factory_bad_blocks);
    h4v1_stage6_put32(&data[H4V1_STAGE6_DESCRIPTOR_CRC_OFFSET], 0u);
    h4v1_stage6_put32(&data[92], result->raw_stream_crc);
    h4v1_stage6_put32(&data[96], result->index_crc);
    h4v1_stage6_put32(&data[100], result->header_crc);
    for(index = 0u; index < H4V1_STAGE6_REQUIRED_BLOCKS; ++index)
    {
        h4v1_stage6_put16(
            &data[H4V1_STAGE6_DESCRIPTOR_MAP_OFFSET + index * 2u],
            h4v1_stage6_map[index]);
        h4v1_stage6_put32(
            &data[H4V1_STAGE6_DESCRIPTOR_BLOCK_CRC_OFFSET +
                  index * 4u],
            h4v1_stage6_block_crc[index]);
    }
    crc = h4v1_stage6_crc32_update(
        0u, data, H4V1_STAGE6_PAGE_BYTES);
    h4v1_stage6_put32(
        &data[H4V1_STAGE6_DESCRIPTOR_CRC_OFFSET], crc);
    return crc;
}

uint32_t H4V1_POSTPASS_TEXT
h4v1_stage6_build_commit(uint8_t *data, uint32_t descriptor_crc)
{
    uint32_t crc;

    h4v1_stage6_fill(data, 0xFFu, H4V1_STAGE6_PAGE_BYTES);
    h4v1_stage6_put32(&data[0], H4V1_STAGE6_COMMIT_MAGIC);
    h4v1_stage6_put32(&data[4], H4V1_STAGE6_MANIFEST_VERSION);
    h4v1_stage6_put32(&data[8], H4V1_STAGE6_GENERATION);
    h4v1_stage6_put32(&data[12], descriptor_crc);
    h4v1_stage6_put32(&data[16], H4V1_STAGE6_TRANSFER_CRC);
    h4v1_stage6_put32(&data[20], H4V1_STAGE6_TRANSFER_BYTES);
    h4v1_stage6_put32(&data[24], H4V1_STAGE6_REQUIRED_BLOCKS);
    h4v1_stage6_put32(&data[H4V1_STAGE6_COMMIT_CRC_OFFSET], 0u);
    crc = h4v1_stage6_crc32_update(
        0u, data, H4V1_STAGE6_PAGE_BYTES);
    h4v1_stage6_put32(&data[H4V1_STAGE6_COMMIT_CRC_OFFSET], crc);
    return crc;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_write_manifest(h4v1_stage6_result_t *result)
{
    uint8_t *data = (uint8_t *)(uintptr_t)H4V1_STAGE6_DATA_ADDR;
    uint32_t base_row =
        H4V1_STAGE6_MANIFEST_BLOCK * H4V1_STAGE6_PAGES_PER_BLOCK;
    uint32_t page_crc;
    uint32_t descriptor_page_crc;
    uint32_t descriptor_crc_readback;
    uint32_t descriptor_crc_rebuilt;
    uint32_t descriptor_crc_zero_field;
    char line[192];

    h4v1_stage6_write_phase = H4V1_STAGE6_WRITE_MANIFEST;
    h4v1_stage6_fence();
    if(h4v1_stage6_unlock(result) != 0)
    {
        return -1;
    }

    result->descriptor_crc = h4v1_stage6_build_descriptor(data, result);
    page_crc = h4v1_stage6_crc32_update(
        0u, data, H4V1_STAGE6_PAGE_BYTES);
    if(h4v1_stage6_program_page(
           result, base_row, data, page_crc) != 0)
    {
        return -2;
    }

    result->commit_crc = h4v1_stage6_build_commit(
        data, result->descriptor_crc);
    page_crc = h4v1_stage6_crc32_update(
        0u, data, H4V1_STAGE6_PAGE_BYTES);
    if(h4v1_stage6_program_page(
           result, base_row + 1u, data, page_crc) != 0)
    {
        return -3;
    }
    result->manifest_committed = 1u;

    /*
     * PROGRAM EXECUTE for commit page 1 is the final write in the block.
     * Rebuild the descriptor expectation after that operation, then read
     * page 0 again.  This detects program disturb that could otherwise
     * leave a valid commit pointing at a descriptor changed by page 1.
     */
    descriptor_crc_rebuilt =
        h4v1_stage6_build_descriptor(data, result);
    if(descriptor_crc_rebuilt != result->descriptor_crc)
    {
        return -4;
    }
    descriptor_page_crc = h4v1_stage6_crc32_update(
        0u, data, H4V1_STAGE6_PAGE_BYTES);
    if(h4v1_stage6_read_page(result, base_row, data) != 0)
    {
        return -5;
    }
    page_crc = h4v1_stage6_crc32_update(
        0u, data, H4V1_STAGE6_PAGE_BYTES);
    descriptor_crc_readback = h4v1_stage6_get32(
        &data[H4V1_STAGE6_DESCRIPTOR_CRC_OFFSET]);
    h4v1_stage6_put32(
        &data[H4V1_STAGE6_DESCRIPTOR_CRC_OFFSET], 0u);
    descriptor_crc_zero_field = h4v1_stage6_crc32_update(
        0u, data, H4V1_STAGE6_PAGE_BYTES);
    h4v1_stage6_put32(
        &data[H4V1_STAGE6_DESCRIPTOR_CRC_OFFSET],
        descriptor_crc_readback);
    if((page_crc != descriptor_page_crc) ||
       (descriptor_crc_readback != result->descriptor_crc) ||
       (descriptor_crc_zero_field != result->descriptor_crc))
    {
        return -6;
    }
    (void)rt_snprintf(
        line, sizeof(line), h4v1_stage6_manifest_pass_format,
        (unsigned int)result->descriptor_crc);
    h4v1_postpass_stage1_log(line);

    if(h4v1_stage6_lock(result) != 0)
    {
        return -7;
    }
    h4v1_stage6_checkpoint(7u, 0u, H4V1_STAGE6_MANIFEST_BLOCK,
                            1u, H4V1_STAGE6_TRANSFER_BYTES,
                            H4V1_STAGE6_TRANSFER_CRC);
    return 0;
}

int H4V1_POSTPASS_TEXT
h4v1_stage6_cleanup(h4v1_stage6_result_t *result)
{
    uint8_t status = 0xFFu;
    uint8_t b0 = 0xFFu;
    uint32_t start;
    int failed = 0;

    h4v1_stage6_write_phase = H4V1_STAGE6_WRITE_NONE;
    if(h4v1_stage6_dma_stop_wait(result) != 0)
    {
        failed = 1;
    }
    DMA2_Channel3->CFGR &= ~H4V1_STAGE6_DMA_EN;
    start = h4v1_stage6_cycle_now();
    while((DMA2_Channel3->CFGR & H4V1_STAGE6_DMA_EN) != 0u)
    {
        if((uint32_t)(h4v1_stage6_cycle_now() - start) >=
           H4V1_STAGE6_DMA2_TIMEOUT)
        {
            result->dma_timeout_count++;
            failed = 1;
            break;
        }
    }
    DMA2->INTFCR = H4V1_STAGE6_DMA2_GL3;
    h4v1_stage6_deselect();
    if(h4v1_stage6_spi_wait_idle(result) != 0)
    {
        failed = 1;
    }
    h4v1_stage6_drain_rx();
    if(result->flash_owned != 0u)
    {
        if(h4v1_stage6_write_disable(result) != 0 ||
           h4v1_stage6_set_a0(result, result->a0_saved) != 0 ||
           h4v1_stage6_get_feature(
               result, H4V1_STAGE6_FEATURE_PROTECTION,
               &result->a0_restored) != 0 ||
           h4v1_stage6_get_feature(
               result, H4V1_STAGE6_FEATURE_CONFIG, &b0) != 0 ||
           h4v1_stage6_get_feature(
               result, H4V1_STAGE6_FEATURE_STATUS, &status) != 0 ||
           result->a0_restored != H4V1_STAGE6_A0_EXPECTED_LOCKED ||
           b0 != result->b0_saved ||
           (status & H4V1_STAGE6_STATUS_WEL) != 0u)
        {
            failed = 1;
        }
        result->b0_readback = b0;
        result->status_last = status;
    }
    h4v1_stage6_iwdg_feed();
    return (failed == 0) ? 0 : -1;
}

int H4V1_POSTPASS_ENTRY h4v1_flash_installer_stage6_run(void)
{
    h4v1_stage6_result_t run = {0};
    h4v1_stage6_saved_t dma1_spi_saved = {0};
    h4v1_stage6_dma2_saved_t dma2_saved = {0};
    uint8_t a0 = 0xFFu;
    uint8_t b0 = 0xFFu;
    uint8_t dma2_owned = 0u;
    uint8_t dma1_spi_owned = 0u;
    uint8_t cleanup_failed = 0u;
    uint32_t failed_phase = 1u;
    int primary = H4V1_FLASH_INSTALLER_STAGE6_OK;
    int result = H4V1_FLASH_INSTALLER_STAGE6_OK;
    char line[384];

    h4v1_stage6_write_phase = H4V1_STAGE6_WRITE_NONE;
    h4v1_stage6_selected_count = 0u;
    h4v1_postpass_stage1_log(h4v1_stage6_start_text);
    h4v1_stage6_checkpoint(1u, 0u, 0u, 0u, 0u, 0u);

    if(h4v1_stage6_dma2_prepare(&run, &dma2_saved) != 0)
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_DMA_BUSY;
    }
    else
    {
        dma2_owned = 1u;
    }
    failed_phase = 1u;
    if((primary == H4V1_FLASH_INSTALLER_STAGE6_OK) &&
       (h4v1_stage6_source_preflight(&run) != 0))
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_CONTRACT;
    }
    if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
    {
        (void)rt_snprintf(
            line, sizeof(line), h4v1_stage6_source_pass_format,
            (unsigned int)H4V1_STAGE6_TRANSFER_BYTES,
            (unsigned int)run.source_crc,
            (unsigned int)H4V1_STAGE6_CONTAINER_BYTES,
            (unsigned int)H4V1_STAGE6_CONTAINER_CRC);
        h4v1_postpass_stage1_log(line);
    }

    if((primary == H4V1_FLASH_INSTALLER_STAGE6_OK) &&
       (h4v1_stage6_save_and_configure(&dma1_spi_saved) != 0))
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_DMA_BUSY;
    }
    else if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
    {
        dma1_spi_owned = 1u;
        h4v1_stage6_apply_50mhz();
        if(h4v1_stage6_read_id(&run) != 0 ||
           h4v1_stage6_get_feature(
               &run, H4V1_STAGE6_FEATURE_PROTECTION, &a0) != 0 ||
           h4v1_stage6_get_feature(
               &run, H4V1_STAGE6_FEATURE_CONFIG, &b0) != 0)
        {
            primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_PROBE;
        }
        else
        {
            run.a0_saved = a0;
            run.b0_saved = b0;
            run.features_saved = 1u;
            run.flash_owned = 1u;
            if((a0 != H4V1_STAGE6_A0_EXPECTED_LOCKED) ||
               ((a0 & H4V1_STAGE6_A0_BRWD) != 0u) ||
               ((b0 & H4V1_STAGE6_B0_ECC_ENABLE) == 0u) ||
               ((b0 & (H4V1_STAGE6_B0_BPL |
                       H4V1_STAGE6_B0_OTP_ENABLE |
                       H4V1_STAGE6_B0_QUAD_ENABLE)) != 0u))
            {
                primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_FEATURE;
            }
        }
    }

    failed_phase = 2u;
    if((primary == H4V1_FLASH_INSTALLER_STAGE6_OK) &&
       (h4v1_stage6_build_plan(&run) != 0))
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_CAPACITY;
    }
    if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
    {
        (void)rt_snprintf(
            line, sizeof(line), h4v1_stage6_plan_pass_format,
            (unsigned int)H4V1_STAGE6_CANDIDATE_BLOCKS,
            (unsigned int)(H4V1_STAGE6_CANDIDATE_BLOCKS -
                           run.factory_bad_blocks),
            (unsigned int)run.factory_bad_blocks,
            (unsigned int)run.scan_unreadable_blocks,
            (unsigned int)run.selected_blocks,
            (unsigned int)h4v1_stage6_map[0],
            (unsigned int)h4v1_stage6_map[
                H4V1_STAGE6_REQUIRED_BLOCKS - 1u]);
        h4v1_postpass_stage1_log(line);
    }

    failed_phase = 3u;
    if((primary == H4V1_FLASH_INSTALLER_STAGE6_OK) &&
       (h4v1_stage6_invalidate_manifest(&run) != 0))
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_MANIFEST;
    }
    if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
    {
        h4v1_postpass_stage1_log(h4v1_stage6_invalidate_pass_text);
    }

    failed_phase = 4u;
    if((primary == H4V1_FLASH_INSTALLER_STAGE6_OK) &&
       (h4v1_stage6_program_payload(&run) != 0))
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_PROGRAM;
    }
    if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
    {
        (void)rt_snprintf(
            line, sizeof(line), h4v1_stage6_payload_pass_format,
            (unsigned int)run.bytes_programmed,
            (unsigned int)run.pages_programmed,
            (unsigned int)run.selected_blocks,
            (unsigned int)run.source_crc);
        h4v1_postpass_stage1_log(line);
    }

    failed_phase = 5u;
    if((primary == H4V1_FLASH_INSTALLER_STAGE6_OK) &&
       (h4v1_stage6_verify_payload(&run) != 0))
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_VERIFY;
    }
    if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
    {
        (void)rt_snprintf(
            line, sizeof(line), h4v1_stage6_verify_pass_format,
            (unsigned int)run.bytes_verified,
            (unsigned int)run.pages_verified,
            (unsigned int)run.flash_crc,
            (unsigned int)run.ecc_worst_bits);
        h4v1_postpass_stage1_log(line);
    }

    failed_phase = 6u;
    if((primary == H4V1_FLASH_INSTALLER_STAGE6_OK) &&
       (h4v1_stage6_write_manifest(&run) != 0))
    {
        primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_MANIFEST;
    }

    if((dma1_spi_owned != 0u) &&
       (h4v1_stage6_cleanup(&run) != 0))
    {
        cleanup_failed = 1u;
        if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
        {
            primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_CLEANUP;
        }
    }
    if(dma1_spi_owned != 0u &&
       h4v1_stage6_restore(&dma1_spi_saved) != 0)
    {
        cleanup_failed = 1u;
        if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
        {
            primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_CLEANUP;
        }
    }
    if(dma2_owned != 0u &&
       h4v1_stage6_dma2_restore(&dma2_saved) != 0)
    {
        cleanup_failed = 1u;
        if(primary == H4V1_FLASH_INSTALLER_STAGE6_OK)
        {
            primary = H4V1_FLASH_INSTALLER_STAGE6_ERR_CLEANUP;
        }
    }
    result = primary;

    if(result == H4V1_FLASH_INSTALLER_STAGE6_OK)
    {
        (void)rt_snprintf(
            line, sizeof(line), h4v1_stage6_commit_pass_format,
            (unsigned int)run.commit_crc,
            (unsigned int)run.descriptor_crc,
            (unsigned int)run.a0_restored,
            (unsigned int)run.b0_readback);
        h4v1_postpass_stage1_log(line);
        (void)rt_snprintf(
            line, sizeof(line), h4v1_stage6_pass_format,
            (unsigned int)run.bytes_verified,
            (unsigned int)run.flash_crc,
            (unsigned int)run.pages_verified,
            (unsigned int)run.selected_blocks,
            (unsigned int)run.factory_bad_blocks,
            (unsigned int)run.a0_restored,
            (unsigned int)run.ecc_worst_bits,
            (unsigned int)run.spi_timeout_count,
            (unsigned int)run.ready_timeout_count,
            (unsigned int)run.dma_timeout_count,
            (unsigned int)run.dma_error_count,
            (unsigned int)run.illegal_command_count);
        h4v1_postpass_stage1_log(line);
        h4v1_stage6_retain->magic = 0u;
        h4v1_stage6_fence();
    }
    else
    {
        (void)rt_snprintf(
            line, sizeof(line), h4v1_stage6_fail_format,
            result, primary,
            (unsigned int)failed_phase,
            (unsigned int)run.current_logical_block,
            (unsigned int)run.current_physical_block,
            (unsigned int)run.current_page,
            (unsigned int)run.bytes_programmed,
            (unsigned int)run.source_crc,
            (unsigned int)run.flash_crc,
            (unsigned int)run.status_last,
            (unsigned int)run.a0_saved,
            (unsigned int)run.a0_restored,
            (unsigned int)cleanup_failed,
            (unsigned int)run.manifest_committed,
            (unsigned int)((failed_phase >= 3u) ? 1u : 0u),
            (unsigned int)run.spi_timeout_count,
            (unsigned int)run.ready_timeout_count,
            (unsigned int)run.dma_timeout_count,
            (unsigned int)run.dma_error_count,
            (unsigned int)run.illegal_command_count);
        h4v1_postpass_stage1_log(line);
    }
    h4v1_stage6_iwdg_feed();
    return result;
}
