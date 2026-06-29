#include "ch32h417_ch585_spi_link.h"

#include <string.h>

#include "ch32h417_rcc.h"

#define CH32H417_CH585_SPI_LINK_DEFAULT_SETUP_CYCLES 256U
#define CH32H417_CH585_SPI_LINK_DEFAULT_GAP_CYCLES   1024U
#define CH32H417_CH585_SPI_LINK_DEFAULT_TIMEOUTS     500000U

static ch32h417_ch585_spi_link_config_t s_ch585_spi_link_config;
static uint32_t s_ch585_spi_link_last_diag;

static void ch32h417_ch585_spi_link_delay_cycles(uint32_t cycles)
{
    while(cycles-- != 0U)
    {
        __asm volatile("nop");
    }
}

static void ch32h417_ch585_spi_link_drain_rx(void)
{
    uint8_t guard = 16U;

    while((guard-- != 0U) &&
          (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != RESET))
    {
        (void)SPI_I2S_ReceiveData(SPI1);
    }
    (void)SPI1->STATR;
}

static int ch32h417_ch585_spi_link_wait_flag(uint16_t flag)
{
    uint32_t polls = s_ch585_spi_link_config.timeout_polls;

    while(SPI_I2S_GetFlagStatus(SPI1, flag) == RESET)
    {
        if(polls-- == 0U)
        {
            return -1;
        }
    }

    return 0;
}

uint32_t ch32h417_ch585_spi_link_diag_sample(void)
{
    uint32_t diag = 0U;

    if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4) != Bit_RESET)
    {
        diag |= CH32H417_CH585_SPI_LINK_DIAG_MISO_HIGH;
    }
    if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_3) != Bit_RESET)
    {
        diag |= CH32H417_CH585_SPI_LINK_DIAG_SCK_HIGH;
    }
    if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5) != Bit_RESET)
    {
        diag |= CH32H417_CH585_SPI_LINK_DIAG_MOSI_HIGH;
    }

    if(s_ch585_spi_link_config.cs_port != 0)
    {
        if(GPIO_ReadOutputDataBit(s_ch585_spi_link_config.cs_port,
                                  s_ch585_spi_link_config.cs_pin) != Bit_RESET)
        {
            diag |= CH32H417_CH585_SPI_LINK_DIAG_ACTIVE_CS_OUT_HIGH;
        }
        if(GPIO_ReadInputDataBit(s_ch585_spi_link_config.cs_port,
                                 s_ch585_spi_link_config.cs_pin) != Bit_RESET)
        {
            diag |= CH32H417_CH585_SPI_LINK_DIAG_ACTIVE_CS_IN_HIGH;
        }
    }
    if(s_ch585_spi_link_config.other_cs_port != 0)
    {
        if(GPIO_ReadOutputDataBit(s_ch585_spi_link_config.other_cs_port,
                                  s_ch585_spi_link_config.other_cs_pin) != Bit_RESET)
        {
            diag |= CH32H417_CH585_SPI_LINK_DIAG_OTHER_CS_OUT_HIGH;
        }
        if(GPIO_ReadInputDataBit(s_ch585_spi_link_config.other_cs_port,
                                 s_ch585_spi_link_config.other_cs_pin) != Bit_RESET)
        {
            diag |= CH32H417_CH585_SPI_LINK_DIAG_OTHER_CS_IN_HIGH;
        }
    }

    if(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != RESET)
    {
        diag |= CH32H417_CH585_SPI_LINK_DIAG_SPI_RXNE;
    }
    if(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) != RESET)
    {
        diag |= CH32H417_CH585_SPI_LINK_DIAG_SPI_TXE;
    }
    if(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET)
    {
        diag |= CH32H417_CH585_SPI_LINK_DIAG_SPI_BSY;
    }

    return diag;
}

uint32_t ch32h417_ch585_spi_link_last_diag(void)
{
    return s_ch585_spi_link_last_diag;
}

void ch32h417_ch585_spi_link_config_for_side(
    ch32h417_ch585_spi_link_side_t side,
    ch32h417_ch585_spi_link_config_t *config)
{
    if(config == 0)
    {
        return;
    }

    memset(config, 0, sizeof(*config));
    if(side == CH32H417_CH585_SPI_LINK_SIDE_RIGHT)
    {
        config->cs_port = GPIOD;
        config->cs_pin = GPIO_Pin_9;
        config->other_cs_port = GPIOF;
        config->other_cs_pin = GPIO_Pin_2;
    }
    else
    {
        config->cs_port = GPIOF;
        config->cs_pin = GPIO_Pin_2;
        config->other_cs_port = GPIOD;
        config->other_cs_pin = GPIO_Pin_9;
    }
    config->cs_setup_cycles = CH32H417_CH585_SPI_LINK_DEFAULT_SETUP_CYCLES;
    config->cs_gap_cycles = CH32H417_CH585_SPI_LINK_DEFAULT_GAP_CYCLES;
    config->timeout_polls = CH32H417_CH585_SPI_LINK_DEFAULT_TIMEOUTS;
}

void ch32h417_ch585_spi_link_init(
    const ch32h417_ch585_spi_link_config_t *config)
{
    GPIO_InitTypeDef gpio = {0};
    SPI_InitTypeDef spi = {0};

    if(config != 0)
    {
        s_ch585_spi_link_config = *config;
    }

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOF |
                          RCC_HB2Periph_SPI1, ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap_VIO3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VDD3V3_IO_HSLV, ENABLE);

    GPIO_SetBits(s_ch585_spi_link_config.cs_port,
                 s_ch585_spi_link_config.cs_pin);
    GPIO_SetBits(s_ch585_spi_link_config.other_cs_port,
                 s_ch585_spi_link_config.other_cs_pin);
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = s_ch585_spi_link_config.cs_pin;
    GPIO_Init(s_ch585_spi_link_config.cs_port, &gpio);
    gpio.GPIO_Pin = s_ch585_spi_link_config.other_cs_pin;
    GPIO_Init(s_ch585_spi_link_config.other_cs_port, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_5;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    SPI_Cmd(SPI1, DISABLE);
    SPI_I2S_DeInit(SPI1);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_Mode2;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7U;
    SPI_Init(SPI1, &spi);
    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
    SPI_HighSpeedMode_Config(SPI1, SPI_HIGH_SPEED_MODE1, DISABLE);
    SPI_HighSpeedMode_Config(SPI1, SPI_HIGH_SPEED_MODE2, DISABLE);
    SPI_Cmd(SPI1, ENABLE);
}

int ch32h417_ch585_spi_link_transfer(const uint8_t *tx,
                                     uint8_t *rx,
                                     uint16_t len)
{
    uint16_t i;
    uint32_t polls;

    if((tx == 0) || (rx == 0) || (len == 0U) ||
       (s_ch585_spi_link_config.cs_port == 0))
    {
        return CH32H417_CH585_SPI_LINK_ERR_PARAM;
    }

    ch32h417_ch585_spi_link_drain_rx();
    GPIO_ResetBits(s_ch585_spi_link_config.cs_port,
                   s_ch585_spi_link_config.cs_pin);
    ch32h417_ch585_spi_link_delay_cycles(
        s_ch585_spi_link_config.cs_setup_cycles);
    s_ch585_spi_link_last_diag = ch32h417_ch585_spi_link_diag_sample();

    for(i = 0U; i < len; i++)
    {
        if(ch32h417_ch585_spi_link_wait_flag(SPI_I2S_FLAG_TXE) != 0)
        {
            s_ch585_spi_link_last_diag = ch32h417_ch585_spi_link_diag_sample();
            GPIO_SetBits(s_ch585_spi_link_config.cs_port,
                         s_ch585_spi_link_config.cs_pin);
            return CH32H417_CH585_SPI_LINK_ERR_TXE;
        }

        SPI_I2S_SendData(SPI1, tx[i]);

        if(ch32h417_ch585_spi_link_wait_flag(SPI_I2S_FLAG_RXNE) != 0)
        {
            s_ch585_spi_link_last_diag = ch32h417_ch585_spi_link_diag_sample();
            GPIO_SetBits(s_ch585_spi_link_config.cs_port,
                         s_ch585_spi_link_config.cs_pin);
            return CH32H417_CH585_SPI_LINK_ERR_RXNE;
        }

        rx[i] = (uint8_t)SPI_I2S_ReceiveData(SPI1);
    }

    polls = s_ch585_spi_link_config.timeout_polls;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET)
    {
        if(polls-- == 0U)
        {
            s_ch585_spi_link_last_diag = ch32h417_ch585_spi_link_diag_sample();
            GPIO_SetBits(s_ch585_spi_link_config.cs_port,
                         s_ch585_spi_link_config.cs_pin);
            return CH32H417_CH585_SPI_LINK_ERR_BUSY;
        }
    }

    s_ch585_spi_link_last_diag = ch32h417_ch585_spi_link_diag_sample();
    GPIO_SetBits(s_ch585_spi_link_config.cs_port,
                 s_ch585_spi_link_config.cs_pin);
    ch32h417_ch585_spi_link_delay_cycles(
        s_ch585_spi_link_config.cs_gap_cycles);
    return CH32H417_CH585_SPI_LINK_OK;
}

uint32_t ch32h417_ch585_spi_link_actual_khz(uint32_t hclk_hz)
{
    return (hclk_hz / 8U) / 1000U;
}
