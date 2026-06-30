#include "ch585_spi0_slave_link.h"

#include <string.h>

#include "CH58x_common.h"

static uint32_t s_ch585_spi0_slave_link_frames;
static uint32_t s_ch585_spi0_slave_link_aborts;
static uint16_t s_ch585_spi0_slave_link_last_rx_count;
static uint8_t s_ch585_spi0_slave_link_last_rx_head[4];

static void ch585_spi0_slave_link_pins_init(void)
{
    GPIOPinRemap(DISABLE, RB_PIN_SPI0);
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_Floating);
}

static void ch585_spi0_slave_link_stream_reset(void)
{
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_MODE_SLAVE;
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;
    SPI0_DataMode(Mode0_HighBitINFront);
}

static void ch585_spi0_slave_link_wait_cs_high(void)
{
    while((R8_SPI0_RUN_FLAG & RB_SPI_SLV_SELECT) != 0U)
    {
    }
}

static void ch585_spi0_slave_link_arm_tx_dma(const uint8_t *tx, uint16_t len)
{
    R8_SPI0_CTRL_CFG &= (uint8_t)(~RB_SPI_DMA_ENABLE);
    R8_SPI0_CTRL_MOD &= (uint8_t)(~RB_SPI_FIFO_DIR);
    R32_SPI0_DMA_BEG = (uint32_t)tx;
    R32_SPI0_DMA_END = (uint32_t)(tx + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END |
                       RB_SPI_IF_DMA_END |
                       RB_SPI_IF_BYTE_END |
                       RB_SPI_IF_FIFO_OV |
                       RB_SPI_IF_FST_BYTE;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
}

static void ch585_spi0_slave_link_arm_rx_dma(uint8_t *rx, uint16_t len)
{
    R8_SPI0_CTRL_CFG &= (uint8_t)(~RB_SPI_DMA_ENABLE);
    R8_SPI0_CTRL_MOD &= (uint8_t)(~RB_SPI_MISO_OE);
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R32_SPI0_DMA_BEG = (uint32_t)rx;
    R32_SPI0_DMA_END = (uint32_t)(rx + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END |
                       RB_SPI_IF_DMA_END |
                       RB_SPI_IF_BYTE_END |
                       RB_SPI_IF_FIFO_OV |
                       RB_SPI_IF_FST_BYTE;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
}

static void ch585_spi0_slave_link_abort_dma(void)
{
    R8_SPI0_CTRL_CFG &= (uint8_t)(~RB_SPI_DMA_ENABLE);
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END |
                       RB_SPI_IF_DMA_END |
                       RB_SPI_IF_BYTE_END |
                       RB_SPI_IF_FIFO_OV |
                       RB_SPI_IF_FST_BYTE;
}

static int ch585_spi0_slave_link_wait_tx_done(uint8_t *rx, uint16_t len)
{
    uint8_t saw_select = 0U;
    uint16_t rx_index = 0U;

    while((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) == 0U)
    {
        if((R8_SPI0_RUN_FLAG & RB_SPI_SLV_SELECT) != 0U)
        {
            saw_select = 1U;
        }
        else if(saw_select != 0U)
        {
            ch585_spi0_slave_link_abort_dma();
            return CH585_SPI0_SLAVE_LINK_ERR_ABORT;
        }

        if((R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END) != 0U)
        {
            uint8_t byte = R8_SPI0_BUFFER;
            if((rx != 0) && (rx_index < len))
            {
                rx[rx_index++] = byte;
                s_ch585_spi0_slave_link_last_rx_count = rx_index;
                if(rx_index <= sizeof(s_ch585_spi0_slave_link_last_rx_head))
                {
                    s_ch585_spi0_slave_link_last_rx_head[rx_index - 1U] = byte;
                }
            }
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
        uint8_t byte = R8_SPI0_BUFFER;
        if((rx != 0) && (rx_index < len))
        {
            rx[rx_index++] = byte;
            s_ch585_spi0_slave_link_last_rx_count = rx_index;
            if(rx_index <= sizeof(s_ch585_spi0_slave_link_last_rx_head))
            {
                s_ch585_spi0_slave_link_last_rx_head[rx_index - 1U] = byte;
            }
        }
        R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
    }

    R8_SPI0_CTRL_CFG &= (uint8_t)(~RB_SPI_DMA_ENABLE);
    return CH585_SPI0_SLAVE_LINK_OK;
}

static int ch585_spi0_slave_link_wait_rx_done(uint8_t *rx, uint16_t len)
{
    uint8_t saw_select = 0U;

    while((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) == 0U)
    {
        if((R8_SPI0_RUN_FLAG & RB_SPI_SLV_SELECT) != 0U)
        {
            saw_select = 1U;
        }
        else if(saw_select != 0U)
        {
            ch585_spi0_slave_link_abort_dma();
            return CH585_SPI0_SLAVE_LINK_ERR_ABORT;
        }

        if((R8_SPI0_INT_FLAG & RB_SPI_IF_FIFO_OV) != 0U)
        {
            (void)R8_SPI0_BUFFER;
            ch585_spi0_slave_link_abort_dma();
            return CH585_SPI0_SLAVE_LINK_ERR_ABORT;
        }
    }

    ch585_spi0_slave_link_abort_dma();
    s_ch585_spi0_slave_link_last_rx_count = len;
    memcpy(s_ch585_spi0_slave_link_last_rx_head,
           rx,
           (len < sizeof(s_ch585_spi0_slave_link_last_rx_head)) ?
               len :
               sizeof(s_ch585_spi0_slave_link_last_rx_head));
    return CH585_SPI0_SLAVE_LINK_OK;
}

void ch585_spi0_slave_link_init(void)
{
    s_ch585_spi0_slave_link_frames = 0U;
    s_ch585_spi0_slave_link_aborts = 0U;
    s_ch585_spi0_slave_link_last_rx_count = 0U;
    memset(s_ch585_spi0_slave_link_last_rx_head, 0, sizeof(s_ch585_spi0_slave_link_last_rx_head));
    ch585_spi0_slave_link_pins_init();
    SPI0_SlaveInit();
    SPI0_DataMode(Mode0_HighBitINFront);
}

int ch585_spi0_slave_link_serve_frame(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    int result;

    if((tx == 0) || (len == 0U))
    {
        return CH585_SPI0_SLAVE_LINK_ERR_PARAM;
    }

    ch585_spi0_slave_link_wait_cs_high();
    ch585_spi0_slave_link_stream_reset();
    s_ch585_spi0_slave_link_last_rx_count = 0U;
    memset(s_ch585_spi0_slave_link_last_rx_head, 0, sizeof(s_ch585_spi0_slave_link_last_rx_head));
    SetFirstData(tx[0]);
    ch585_spi0_slave_link_arm_tx_dma(tx, len);
    result = ch585_spi0_slave_link_wait_tx_done(rx, len);
    ch585_spi0_slave_link_wait_cs_high();

    if(result == CH585_SPI0_SLAVE_LINK_OK)
    {
        s_ch585_spi0_slave_link_frames++;
    }
    else if(result == CH585_SPI0_SLAVE_LINK_ERR_ABORT)
    {
        s_ch585_spi0_slave_link_aborts++;
    }

    return result;
}

int ch585_spi0_slave_link_receive_frame(uint8_t *rx, uint16_t len)
{
    int result;

    if((rx == 0) || (len == 0U))
    {
        return CH585_SPI0_SLAVE_LINK_ERR_PARAM;
    }

    ch585_spi0_slave_link_wait_cs_high();
    ch585_spi0_slave_link_stream_reset();
    s_ch585_spi0_slave_link_last_rx_count = 0U;
    memset(s_ch585_spi0_slave_link_last_rx_head, 0, sizeof(s_ch585_spi0_slave_link_last_rx_head));
    memset(rx, 0, len);
    ch585_spi0_slave_link_arm_rx_dma(rx, len);
    result = ch585_spi0_slave_link_wait_rx_done(rx, len);
    ch585_spi0_slave_link_wait_cs_high();

    if(result == CH585_SPI0_SLAVE_LINK_OK)
    {
        s_ch585_spi0_slave_link_frames++;
    }
    else if(result == CH585_SPI0_SLAVE_LINK_ERR_ABORT)
    {
        s_ch585_spi0_slave_link_aborts++;
    }

    return result;
}

int ch585_spi0_slave_link_serve_tx_frame(const uint8_t *tx, uint16_t len)
{
    return ch585_spi0_slave_link_serve_frame(tx, 0, len);
}

void ch585_spi0_slave_link_get_stats(ch585_spi0_slave_link_stats_t *stats)
{
    if(stats == 0)
    {
        return;
    }

    stats->frames = s_ch585_spi0_slave_link_frames;
    stats->aborts = s_ch585_spi0_slave_link_aborts;
    stats->last_rx_count = s_ch585_spi0_slave_link_last_rx_count;
    memcpy(stats->last_rx_head, s_ch585_spi0_slave_link_last_rx_head, sizeof(stats->last_rx_head));
    stats->flags = R8_SPI0_INT_FLAG;
    stats->status = R32_SPI0_STATUS;
}
