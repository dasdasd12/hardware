#ifndef GD5F1G_L8_ASSET_STORE_H
#define GD5F1G_L8_ASSET_STORE_H

#include <stdint.h>
#include "gd5f1g_spi_nand.h"

/*
 * Test-only L8 image asset store on GD5F1G SPI-NAND.
 *
 * The helper owns only the package layout and byte streaming rules. It does
 * not know about LTDC, GPHA, RT-Thread, or image content. The caller provides
 * a scratch buffer for page-sized work and is responsible for choosing a safe
 * block range. Erase helpers erase whole NAND blocks in that range.
 *
 * This package format was created for the current flash/LTDC path test. Keep
 * it under hw_tests unless the product firmware later defines a stable asset
 * format.
 */

#define GD5F1G_L8_ASSET_MAGIC        0x4641384Cu
#define GD5F1G_L8_ASSET_VERSION      1u
#define GD5F1G_L8_ASSET_ENTRY_COUNT  4u
#define GD5F1G_L8_ASSET_ENTRY_BASE   32u
#define GD5F1G_L8_ASSET_ENTRY_STRIDE 16u

#define GD5F1G_L8_ASSET_TYPE_GRAY_CLUT     1u
#define GD5F1G_L8_ASSET_TYPE_GRAY_IMAGE    2u
#define GD5F1G_L8_ASSET_TYPE_PALETTE_CLUT  3u
#define GD5F1G_L8_ASSET_TYPE_PALETTE_IMAGE 4u

#define GD5F1G_L8_ASSET_FNV1A_SEED 2166136261u
#define GD5F1G_L8_ASSET_ALIGN_PAGE_CONST(value) \
    ((((value) + GD5F1G_PAGE_SIZE - 1u) / GD5F1G_PAGE_SIZE) * GD5F1G_PAGE_SIZE)

typedef struct
{
    uint32_t type;
    uint32_t offset;
    uint32_t length;
    uint32_t checksum;
} gd5f1g_l8_asset_entry_t;

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t total_bytes;
    gd5f1g_l8_asset_entry_t entries[GD5F1G_L8_ASSET_ENTRY_COUNT];
} gd5f1g_l8_asset_manifest_t;

uint32_t gd5f1g_l8_asset_align_page(uint32_t value);
uint32_t gd5f1g_l8_asset_fnv1a_update(uint32_t checksum, uint8_t value);
uint32_t gd5f1g_l8_asset_fnv1a_buffer(const uint8_t *data, uint32_t length);
void gd5f1g_l8_asset_fill_gray_clut(uint8_t *data, uint16_t entries);
int gd5f1g_l8_asset_lzss_decode(const uint8_t *source,
                                 uint32_t source_length,
                                 uint8_t *dest,
                                 uint32_t dest_length);
void gd5f1g_l8_asset_unfilter_left(uint8_t *data,
                                   uint16_t width,
                                   uint16_t height);

int gd5f1g_l8_asset_program_linear(const gd5f1g_spi_bus_t *bus,
                                   uint32_t start_block,
                                   uint32_t offset,
                                   const uint8_t *data,
                                   uint32_t length);
int gd5f1g_l8_asset_read_linear(const gd5f1g_spi_bus_t *bus,
                                uint32_t start_block,
                                uint32_t offset,
                                uint8_t *data,
                                uint32_t length,
                                uint8_t *status_out);
int gd5f1g_l8_asset_checksum_linear(const gd5f1g_spi_bus_t *bus,
                                    uint32_t start_block,
                                    uint32_t offset,
                                    uint32_t length,
                                    uint8_t *scratch,
                                    uint32_t scratch_length,
                                    uint32_t *checksum_out,
                                    uint8_t *status_out);
int gd5f1g_l8_asset_verify_linear(const gd5f1g_spi_bus_t *bus,
                                  uint32_t start_block,
                                  uint32_t offset,
                                  uint32_t length,
                                  uint32_t expected_checksum,
                                  uint8_t *scratch,
                                  uint32_t scratch_length,
                                  uint8_t *status_out);

void gd5f1g_l8_asset_manifest_init(gd5f1g_l8_asset_manifest_t *manifest,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t total_bytes);
int gd5f1g_l8_asset_manifest_set(gd5f1g_l8_asset_manifest_t *manifest,
                                 uint32_t slot,
                                 uint32_t type,
                                 uint32_t offset,
                                 uint32_t length,
                                 uint32_t checksum);
int gd5f1g_l8_asset_manifest_find(const gd5f1g_l8_asset_manifest_t *manifest,
                                  uint32_t type,
                                  gd5f1g_l8_asset_entry_t *entry_out);
int gd5f1g_l8_asset_write_manifest(const gd5f1g_spi_bus_t *bus,
                                   uint32_t start_block,
                                   const gd5f1g_l8_asset_manifest_t *manifest,
                                   uint8_t *scratch_page,
                                   uint32_t scratch_length);
int gd5f1g_l8_asset_read_manifest(const gd5f1g_spi_bus_t *bus,
                                  uint32_t start_block,
                                  uint32_t expected_width,
                                  uint32_t expected_height,
                                  gd5f1g_l8_asset_manifest_t *manifest_out,
                                  uint8_t *scratch_page,
                                  uint32_t scratch_length,
                                  uint8_t *status_out);

int gd5f1g_l8_asset_check_blocks(const gd5f1g_spi_bus_t *bus,
                                 uint32_t start_block,
                                 uint32_t block_count,
                                 uint8_t *marker_out,
                                 uint8_t *status_out);
int gd5f1g_l8_asset_erase_blocks(const gd5f1g_spi_bus_t *bus,
                                 uint32_t start_block,
                                 uint32_t block_count);

#endif
