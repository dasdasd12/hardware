#include "ch585_common.h"

#include "CH58x_spi.h"
#include "ch585_h417_spi_speed_proto.h"

#define CH585_SPI0_SPEED_PIN_DESC "PA12_CS_PA13_SCK_PA14_MOSI_PA15_MISO"
#define CH585_SPI0_SPEED_LOG_INTERVAL 1024U

#ifdef FREQ_SYS
#define CH585_SPI0_SPEED_SYSCLK_HZ ((uint32_t)FREQ_SYS)
#else
#define CH585_SPI0_SPEED_SYSCLK_HZ 62400000UL
#endif

static uint8_t g_ch585_spi0_speed_tx[CH585_H417_SPI_SPEED_TRANSFER_BYTES] __attribute__((aligned(4)));
static uint32_t g_ch585_spi0_speed_frames;

static void ch585_spi0_speed_pins_init(void)
{
    GPIOPinRemap(DISABLE, RB_PIN_SPI0);
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeOut_PP_20mA);
}

static void ch585_spi0_speed_stream_reset(void)
{
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_MODE_SLAVE;
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;
    SPI0_DataMode(Mode0_HighBitINFront);
}

static void ch585_spi0_speed_wait_cs_high(void)
{
    while((R8_SPI0_RUN_FLAG & RB_SPI_SLV_SELECT) != 0U)
    {
    }
}

static void ch585_spi0_speed_dma_trans_arm(uint8_t *pbuf, uint16_t len)
{
    R8_SPI0_CTRL_MOD &= (uint8_t)(~RB_SPI_FIFO_DIR);
    R32_SPI0_DMA_BEG = (uint32_t)pbuf;
    R32_SPI0_DMA_END = (uint32_t)(pbuf + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END |
                       RB_SPI_IF_DMA_END |
                       RB_SPI_IF_BYTE_END |
                       RB_SPI_IF_FIFO_OV |
                       RB_SPI_IF_FST_BYTE;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
}

static void ch585_spi0_speed_dma_wait_done_capture(void)
{
    while((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) == 0U)
    {
        if((R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END) != 0U)
        {
            (void)R8_SPI0_BUFFER;
            R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
        }

        if((R8_SPI0_INT_FLAG & RB_SPI_IF_FIFO_OV) != 0U)
        {
            (void)R8_SPI0_BUFFER;
            R8_SPI0_INT_FLAG = RB_SPI_IF_FIFO_OV;
        }
    }

    while((R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END) != 0U)
    {
        (void)R8_SPI0_BUFFER;
        R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
    }

    R8_SPI0_CTRL_CFG &= (uint8_t)(~RB_SPI_DMA_ENABLE);
}

static void ch585_spi0_speed_init_fixed_transfer(void)
{
    uint16_t i;

    for(i = 0U; i < CH585_H417_SPI_SPEED_TRANSFER_BYTES; i++)
    {
        g_ch585_spi0_speed_tx[i] = ch585_h417_spi_speed_fixed_byte(i);
    }
}

static void ch585_spi0_speed_log_stat(void)
{
    ch585_log_str("STAT spi0_speed_slave frames=");
    ch585_log_u32_dec(g_ch585_spi0_speed_frames);
    ch585_log_str(" flags=0x");
    ch585_log_u32_hex((uint32_t)R8_SPI0_INT_FLAG, 2);
    ch585_log_str(" status=0x");
    ch585_log_u32_hex(R32_SPI0_STATUS, 8);
    ch585_log_str("\r\n");
}

void ch585_spi0_speed_slave_run(void)
{
    ch585_spi0_speed_pins_init();
    SPI0_SlaveInit();
    SPI0_DataMode(Mode0_HighBitINFront);
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
        ch585_spi0_speed_wait_cs_high();
        ch585_spi0_speed_stream_reset();
        SetFirstData(g_ch585_spi0_speed_tx[0]);
        ch585_spi0_speed_dma_trans_arm(g_ch585_spi0_speed_tx,
                                       (uint16_t)CH585_H417_SPI_SPEED_TRANSFER_BYTES);
        ch585_spi0_speed_dma_wait_done_capture();
        ch585_spi0_speed_wait_cs_high();

        g_ch585_spi0_speed_frames++;
        if((g_ch585_spi0_speed_frames % CH585_SPI0_SPEED_LOG_INTERVAL) == 0U)
        {
            ch585_spi0_speed_log_stat();
        }
    }
}
