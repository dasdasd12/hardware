#ifndef HOST_STUB_CH32H417_CH585_SPI_LINK_H
#define HOST_STUB_CH32H417_CH585_SPI_LINK_H

#include <stdint.h>

#define CH32H417_CH585_SPI_LINK_OK            0
#define CH32H417_CH585_SPI_LINK_ERR_PARAM    -1
#define CH32H417_CH585_SPI_LINK_ERR_TXE      -2
#define CH32H417_CH585_SPI_LINK_ERR_RXNE     -3
#define CH32H417_CH585_SPI_LINK_ERR_BUSY     -4

typedef enum
{
    CH32H417_CH585_SPI_LINK_SIDE_LEFT = 0,
    CH32H417_CH585_SPI_LINK_SIDE_RIGHT = 1,
} ch32h417_ch585_spi_link_side_t;

typedef struct
{
    ch32h417_ch585_spi_link_side_t side;
} ch32h417_ch585_spi_link_config_t;

void ch32h417_ch585_spi_link_config_for_side(
    ch32h417_ch585_spi_link_side_t side,
    ch32h417_ch585_spi_link_config_t *config);
void ch32h417_ch585_spi_link_init(
    const ch32h417_ch585_spi_link_config_t *config);
int ch32h417_ch585_spi_link_transfer(const uint8_t *tx,
                                     uint8_t *rx,
                                     uint16_t len);
uint32_t ch32h417_ch585_spi_link_last_diag(void);

#endif
