#ifndef H4V1_FLASH_COLDBOOT_CORE_H
#define H4V1_FLASH_COLDBOOT_CORE_H

#include <stdint.h>

#ifndef H4V1_FLASH_COLDBOOT_PRODUCT
#define H4V1_FLASH_COLDBOOT_PRODUCT 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define H4V1_FLASH_COLDBOOT_TRANSFER_BYTES 30965760u
#define H4V1_FLASH_COLDBOOT_TRANSFER_CRC   0xE32A6C99u
#define H4V1_FLASH_COLDBOOT_MAP_BLOCKS     237u
#define H4V1_FLASH_COLDBOOT_FRAMES         165u
#define H4V1_FLASH_COLDBOOT_FPS            30u

enum
{
    H4V1_FLASH_COLDBOOT_STATE_RESET = 0,
    H4V1_FLASH_COLDBOOT_STATE_MANIFEST = 1,
    H4V1_FLASH_COLDBOOT_STATE_LOAD = 2,
    H4V1_FLASH_COLDBOOT_STATE_LOADED = 3,
    H4V1_FLASH_COLDBOOT_STATE_PLAYED = 4,
    H4V1_FLASH_COLDBOOT_STATE_REJECTED = 5
};

enum
{
    H4V1_FLASH_COLDBOOT_OK = 0,
    H4V1_FLASH_COLDBOOT_ERR_ARGUMENT = -1,
    H4V1_FLASH_COLDBOOT_ERR_STATE = -2,
    H4V1_FLASH_COLDBOOT_ERR_DMA_BUSY = -3,
    H4V1_FLASH_COLDBOOT_ERR_SPI = -4,
    H4V1_FLASH_COLDBOOT_ERR_ID = -5,
    H4V1_FLASH_COLDBOOT_ERR_FEATURE = -6,
    H4V1_FLASH_COLDBOOT_ERR_COMMIT = -7,
    H4V1_FLASH_COLDBOOT_ERR_DESCRIPTOR = -8,
    H4V1_FLASH_COLDBOOT_ERR_MAP = -9,
    H4V1_FLASH_COLDBOOT_ERR_ECC = -10,
    H4V1_FLASH_COLDBOOT_ERR_MARKER = -11,
    H4V1_FLASH_COLDBOOT_ERR_DMA = -12,
    H4V1_FLASH_COLDBOOT_ERR_CRC = -13,
    H4V1_FLASH_COLDBOOT_ERR_MANIFEST_CHANGED = -14,
    H4V1_FLASH_COLDBOOT_ERR_RAW_DISABLE = -15,
    H4V1_FLASH_COLDBOOT_ERR_RESTORE = -16
};

typedef struct
{
    uint32_t state;
    int32_t error;
    uint32_t descriptor_crc;
    uint32_t commit_crc;
    uint32_t bytes_loaded;
    uint32_t pages_loaded;
    uint32_t flash_crc;
    uint32_t logical_block;
    uint32_t physical_block;
    uint32_t page;
    uint32_t spi_timeouts;
    uint32_t dma_timeouts;
    uint32_t dma_errors;
    uint16_t first_block;
    uint16_t last_block;
    uint8_t factory_bad_blocks;
    uint8_t ecc_worst_bits;
    uint8_t mid;
    uint8_t did;
    uint8_t a0;
    uint8_t b0;
    uint8_t c0;
    uint8_t f0;
} h4v1_flash_coldboot_status_t;

/*
 * The hook calls reset exactly once per boot before the first line-provider
 * call.  The cold-boot BSS is deliberately NOLOAD, so this explicit reset is
 * part of the ABI and must not be replaced by an assumed CRT zero-fill.
 */
void h4v1_flash_coldboot_reset(void);

/* Drop-in providers for the two qualified USB receive call sites. */
int h4v1_flash_coldboot_line_provider(char *dst, uint32_t cap);
int h4v1_flash_coldboot_raw_provider(void *dst, uint32_t cap);

#if H4V1_FLASH_COLDBOOT_PRODUCT
/* Release DMA1/SPI ownership on a product-player nonlocal error exit. */
int h4v1_flash_coldboot_abort(void);

/* Product shim: no USB raw ring exists, so disabling it is a no-op. */
int h4v1_flash_coldboot_product_raw_disable(void);
#endif

/* Called only after the qualified decoder has emitted RESULT PASS. */
void h4v1_flash_coldboot_play_pass(void);

/*
 * Re-emit the immutable boot banner and every completed PASS stage for a
 * late-attaching monitor.  This never changes state and never accesses NAND.
 */
void h4v1_flash_coldboot_replay_status(void);

/* Drop-in no-op for the qualified USB credit-ACK logger during Flash preload. */
void h4v1_flash_coldboot_ack_suppress(const char *line);

const h4v1_flash_coldboot_status_t *h4v1_flash_coldboot_status(void);

/* Implemented by the cold-boot hook and routed to its bounded CDC logger. */
void h4v1_flash_coldboot_log_line(const char *line);

extern const char h4v1_flash_coldboot_banner[];
extern const char h4v1_flash_coldboot_manifest_pass_format[];
extern const char h4v1_flash_coldboot_load_pass_format[];
extern const char h4v1_flash_coldboot_play_pass_format[];

#ifdef __cplusplus
}
#endif

#endif
