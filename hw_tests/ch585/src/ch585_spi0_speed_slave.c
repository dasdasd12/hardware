#include "ch585_common.h"

#include "ch585_spi0_slave_link.h"
#include "ch585_h417_spi_speed_proto.h"

#define CH585_SPI0_SPEED_PIN_DESC "PA12_CS_PA13_SCK_PA14_MOSI_PA15_MISO"

#ifdef FREQ_SYS
#define CH585_SPI0_SPEED_SYSCLK_HZ ((uint32_t)FREQ_SYS)
#else
#define CH585_SPI0_SPEED_SYSCLK_HZ 62400000UL
#endif

static uint8_t g_ch585_spi0_speed_tx[CH585_H417_SPI_SPEED_TRANSFER_BYTES] __attribute__((aligned(4)));

static void ch585_spi0_speed_init_fixed_transfer(void)
{
    uint16_t i;

    for(i = 0U; i < CH585_H417_SPI_SPEED_TRANSFER_BYTES; i++)
    {
        g_ch585_spi0_speed_tx[i] = ch585_h417_spi_speed_fixed_byte(i);
    }
}

void ch585_spi0_speed_slave_run(void)
{
    ch585_spi0_slave_link_init();
    ch585_spi0_speed_init_fixed_transfer();

    ch585_log_str("DATA spi0_speed_slave pins=");
    ch585_log_str(CH585_SPI0_SPEED_PIN_DESC);
    ch585_log_str(" frame_bytes=");
    ch585_log_u32_dec(CH585_H417_SPI_SPEED_FRAME_BYTES);
    ch585_log_str(" ready_byte=0x");
    ch585_log_u32_hex((uint32_t)CH585_H417_SPI_SPEED_READY_BYTE, 2);
    ch585_log_str(" sysclk=");
    ch585_log_u32_dec(CH585_SPI0_SPEED_SYSCLK_HZ);
    ch585_log_str("\r\n");

    while(1)
    {
        (void)ch585_spi0_slave_link_serve_tx_frame(
            g_ch585_spi0_speed_tx,
            (uint16_t)CH585_H417_SPI_SPEED_TRANSFER_BYTES);
    }
}
