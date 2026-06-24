#ifndef GD5F1G_SPI_NAND_H
#define GD5F1G_SPI_NAND_H

#include <stdint.h>

#define GD5F1G_MANUFACTURER_ID       0xC8u
#define GD5F1G_DEVICE_ID_3V          0x91u
#define GD5F1G_PAGE_SIZE             2048u
#define GD5F1G_SPARE_SIZE_ECC_ON     64u
#define GD5F1G_PAGES_PER_BLOCK       64u
#define GD5F1G_BLOCK_COUNT           1024u
#define GD5F1G_BLOCK_SIZE            (GD5F1G_PAGE_SIZE * GD5F1G_PAGES_PER_BLOCK)

#define GD5F1G_FEATURE_PROTECTION    0xA0u
#define GD5F1G_FEATURE_CONFIG        0xB0u
#define GD5F1G_FEATURE_STATUS        0xC0u
#define GD5F1G_FEATURE_STATUS2       0xF0u

#define GD5F1G_STATUS_OIP            0x01u
#define GD5F1G_STATUS_WEL            0x02u
#define GD5F1G_STATUS_E_FAIL         0x04u
#define GD5F1G_STATUS_P_FAIL         0x08u
#define GD5F1G_STATUS_ECC_MASK       0x30u
#define GD5F1G_STATUS_ECC_UNCORR     0x20u

typedef enum
{
    GD5F1G_OK = 0,
    GD5F1G_ERR_PARAM = -1,
    GD5F1G_ERR_TIMEOUT = -2,
    GD5F1G_ERR_ID = -3,
    GD5F1G_ERR_WRITE_ENABLE = -4,
    GD5F1G_ERR_ERASE = -5,
    GD5F1G_ERR_PROGRAM = -6,
    GD5F1G_ERR_ECC = -7,
    GD5F1G_ERR_PROTECTION = -8,
    GD5F1G_ERR_VERIFY = -9,
    GD5F1G_ERR_NO_SCRATCH_BLOCK = -10
} gd5f1g_result_t;

typedef struct
{
    void *context;
    uint8_t (*transfer)(void *context, uint8_t tx);
    void (*select)(void *context);
    void (*deselect)(void *context);
    void (*delay_us)(void *context, uint32_t us);
} gd5f1g_spi_bus_t;

typedef struct
{
    uint8_t manufacturer_id;
    uint8_t device_id;
    uint8_t protection;
    uint8_t config;
    uint8_t status;
    uint8_t status2;
} gd5f1g_info_t;

int gd5f1g_reset(const gd5f1g_spi_bus_t *bus);
int gd5f1g_read_id(const gd5f1g_spi_bus_t *bus, uint8_t *manufacturer_id, uint8_t *device_id);
int gd5f1g_read_info(const gd5f1g_spi_bus_t *bus, gd5f1g_info_t *info);
int gd5f1g_unlock_all_blocks(const gd5f1g_spi_bus_t *bus);
int gd5f1g_block_erase(const gd5f1g_spi_bus_t *bus, uint32_t block);
int gd5f1g_program_page(const gd5f1g_spi_bus_t *bus,
                        uint32_t row_address,
                        uint16_t column,
                        const uint8_t *data,
                        uint32_t length);
int gd5f1g_read_page(const gd5f1g_spi_bus_t *bus,
                     uint32_t row_address,
                     uint16_t column,
                     uint8_t *data,
                     uint32_t length,
                     uint8_t *status_out);
int gd5f1g_get_feature(const gd5f1g_spi_bus_t *bus, uint8_t address, uint8_t *value);
int gd5f1g_set_feature(const gd5f1g_spi_bus_t *bus, uint8_t address, uint8_t value);
uint32_t gd5f1g_block_to_row(uint32_t block);

#endif
