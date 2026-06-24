#ifndef CH32H417_GD5F1G_SPI1_H
#define CH32H417_GD5F1G_SPI1_H

#include <stdint.h>
#include "gd5f1g_spi_nand.h"

#define CH32H417_GD5F1G_SPI_MODE0 0u
#define CH32H417_GD5F1G_SPI_MODE3 3u
#define CH32H417_GD5F1G_SPI_GPIO  100u

#define CH32H417_GD5F1G_MISO_FLOAT_CS_HIGH 0x01u
#define CH32H417_GD5F1G_MISO_PULLUP_CS_HIGH 0x02u
#define CH32H417_GD5F1G_MISO_PULLDOWN_CS_HIGH 0x04u
#define CH32H417_GD5F1G_MISO_FLOAT_CS_LOW 0x08u
#define CH32H417_GD5F1G_MISO_PULLUP_CS_LOW 0x10u
#define CH32H417_GD5F1G_MISO_PULLDOWN_CS_LOW 0x20u

typedef struct
{
    volatile uint32_t timeout_count;
    volatile uint32_t active_mode;
} ch32h417_gd5f1g_spi1_context_t;

void ch32h417_gd5f1g_spi1_init(ch32h417_gd5f1g_spi1_context_t *context,
                               gd5f1g_spi_bus_t *bus);
void ch32h417_gd5f1g_spi1_set_mode(ch32h417_gd5f1g_spi1_context_t *context,
                                   uint8_t mode);
void ch32h417_gd5f1g_gpio_init(ch32h417_gd5f1g_spi1_context_t *context,
                               gd5f1g_spi_bus_t *bus);
void ch32h417_gd5f1g_gpio_pullup_init(ch32h417_gd5f1g_spi1_context_t *context,
                                      gd5f1g_spi_bus_t *bus);
void ch32h417_gd5f1g_gpio_read_id_slow(uint8_t *manufacturer_id,
                                       uint8_t *device_id);
uint32_t ch32h417_gd5f1g_miso_probe(void);

#endif
