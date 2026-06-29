#ifndef CH32H417_CH585_SPI_LINK_H
#define CH32H417_CH585_SPI_LINK_H

#include <stdint.h>

#include "ch32h417_gpio.h"
#include "ch32h417_spi.h"

#define CH32H417_CH585_SPI_LINK_SPI_KHZ 12500U

#define CH32H417_CH585_SPI_LINK_OK            0
#define CH32H417_CH585_SPI_LINK_ERR_PARAM    -1
#define CH32H417_CH585_SPI_LINK_ERR_TXE      -2
#define CH32H417_CH585_SPI_LINK_ERR_RXNE     -3
#define CH32H417_CH585_SPI_LINK_ERR_BUSY     -4

#define CH32H417_CH585_SPI_LINK_DIAG_MISO_HIGH          0x00000001UL
#define CH32H417_CH585_SPI_LINK_DIAG_SCK_HIGH           0x00000002UL
#define CH32H417_CH585_SPI_LINK_DIAG_MOSI_HIGH          0x00000004UL
#define CH32H417_CH585_SPI_LINK_DIAG_ACTIVE_CS_OUT_HIGH 0x00000010UL
#define CH32H417_CH585_SPI_LINK_DIAG_ACTIVE_CS_IN_HIGH  0x00000020UL
#define CH32H417_CH585_SPI_LINK_DIAG_OTHER_CS_OUT_HIGH  0x00000040UL
#define CH32H417_CH585_SPI_LINK_DIAG_OTHER_CS_IN_HIGH   0x00000080UL
#define CH32H417_CH585_SPI_LINK_DIAG_SPI_RXNE           0x00000100UL
#define CH32H417_CH585_SPI_LINK_DIAG_SPI_TXE            0x00000200UL
#define CH32H417_CH585_SPI_LINK_DIAG_SPI_BSY            0x00000400UL

typedef enum
{
    CH32H417_CH585_SPI_LINK_SIDE_LEFT = 0,
    CH32H417_CH585_SPI_LINK_SIDE_RIGHT = 1,
} ch32h417_ch585_spi_link_side_t;

typedef struct
{
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *other_cs_port;
    uint16_t other_cs_pin;
    uint32_t cs_setup_cycles;
    uint32_t cs_gap_cycles;
    uint32_t timeout_polls;
} ch32h417_ch585_spi_link_config_t;

void ch32h417_ch585_spi_link_config_for_side(
    ch32h417_ch585_spi_link_side_t side,
    ch32h417_ch585_spi_link_config_t *config);
void ch32h417_ch585_spi_link_init(
    const ch32h417_ch585_spi_link_config_t *config);
int ch32h417_ch585_spi_link_transfer(const uint8_t *tx,
                                     uint8_t *rx,
                                     uint16_t len);
uint32_t ch32h417_ch585_spi_link_actual_khz(uint32_t hclk_hz);
uint32_t ch32h417_ch585_spi_link_diag_sample(void);
uint32_t ch32h417_ch585_spi_link_last_diag(void);

#endif
