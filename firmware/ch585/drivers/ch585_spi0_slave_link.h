#ifndef CH585_SPI0_SLAVE_LINK_H
#define CH585_SPI0_SLAVE_LINK_H

#include <stdint.h>

#define CH585_SPI0_SLAVE_LINK_OK         0
#define CH585_SPI0_SLAVE_LINK_ERR_PARAM -1
#define CH585_SPI0_SLAVE_LINK_ERR_ABORT -2

typedef struct
{
    uint32_t frames;
    uint32_t aborts;
    uint16_t last_rx_count;
    uint8_t last_rx_head[4];
    uint8_t flags;
    uint32_t status;
} ch585_spi0_slave_link_stats_t;

void ch585_spi0_slave_link_init(void);
int ch585_spi0_slave_link_receive_frame(uint8_t *rx, uint16_t len);
int ch585_spi0_slave_link_serve_tx_frame(const uint8_t *tx, uint16_t len);
int ch585_spi0_slave_link_serve_frame(const uint8_t *tx, uint8_t *rx, uint16_t len);
void ch585_spi0_slave_link_get_stats(ch585_spi0_slave_link_stats_t *stats);

#endif
