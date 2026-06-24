#include "ch32h417_gd5f1g_spi1.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "ch32h417_spi.h"

#define FLASH_CS_PORT      GPIOF
#define FLASH_CS_PIN       GPIO_Pin_6
#define FLASH_SCK_PIN      GPIO_Pin_7
#define FLASH_MOSI_PIN     GPIO_Pin_8
#define FLASH_MISO_PIN     GPIO_Pin_9

static void spi1_delay_us(void *context, uint32_t us);

static void gpio_delay(void)
{
    volatile uint32_t i;

    for(i = 0; i < 80u; ++i)
    {
        __asm__ volatile("nop");
    }
}

static void gpio_slow_delay(void)
{
    volatile uint32_t i;

    for(i = 0; i < 5000u; ++i)
    {
        __asm__ volatile("nop");
    }
}

static void spi1_flush_rx(void)
{
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != RESET)
    {
        (void)SPI_I2S_ReceiveData(SPI1);
    }
}

static void flash_gpio_idle(void)
{
    GPIO_SetBits(FLASH_CS_PORT, FLASH_CS_PIN);
    GPIO_ResetBits(FLASH_CS_PORT, FLASH_SCK_PIN);
    GPIO_ResetBits(FLASH_CS_PORT, FLASH_MOSI_PIN);
}

static uint8_t flash_miso_read_with_mode(GPIOMode_TypeDef mode, uint8_t cs_high)
{
    GPIO_InitTypeDef gpio = {0};

    if(cs_high != 0u)
    {
        GPIO_SetBits(FLASH_CS_PORT, FLASH_CS_PIN);
    }
    else
    {
        GPIO_ResetBits(FLASH_CS_PORT, FLASH_CS_PIN);
    }
    GPIO_ResetBits(FLASH_CS_PORT, FLASH_SCK_PIN);
    GPIO_ResetBits(FLASH_CS_PORT, FLASH_MOSI_PIN);

    gpio.GPIO_Pin = FLASH_MISO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = mode;
    GPIO_Init(FLASH_CS_PORT, &gpio);
    gpio_delay();

    return GPIO_ReadInputDataBit(FLASH_CS_PORT, FLASH_MISO_PIN);
}

uint32_t ch32h417_gd5f1g_miso_probe(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint32_t result = 0u;

    SPI_Cmd(SPI1, DISABLE);
    spi1_flush_rx();

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOF,
                          ENABLE);

    gpio.GPIO_Pin = FLASH_CS_PIN | FLASH_SCK_PIN | FLASH_MOSI_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(FLASH_CS_PORT, &gpio);
    flash_gpio_idle();

    if(flash_miso_read_with_mode(GPIO_Mode_IN_FLOATING, 1u) != Bit_RESET)
    {
        result |= CH32H417_GD5F1G_MISO_FLOAT_CS_HIGH;
    }
    if(flash_miso_read_with_mode(GPIO_Mode_IPU, 1u) != Bit_RESET)
    {
        result |= CH32H417_GD5F1G_MISO_PULLUP_CS_HIGH;
    }
    if(flash_miso_read_with_mode(GPIO_Mode_IPD, 1u) != Bit_RESET)
    {
        result |= CH32H417_GD5F1G_MISO_PULLDOWN_CS_HIGH;
    }
    if(flash_miso_read_with_mode(GPIO_Mode_IN_FLOATING, 0u) != Bit_RESET)
    {
        result |= CH32H417_GD5F1G_MISO_FLOAT_CS_LOW;
    }
    if(flash_miso_read_with_mode(GPIO_Mode_IPU, 0u) != Bit_RESET)
    {
        result |= CH32H417_GD5F1G_MISO_PULLUP_CS_LOW;
    }
    if(flash_miso_read_with_mode(GPIO_Mode_IPD, 0u) != Bit_RESET)
    {
        result |= CH32H417_GD5F1G_MISO_PULLDOWN_CS_LOW;
    }

    flash_gpio_idle();
    return result;
}

void ch32h417_gd5f1g_spi1_set_mode(ch32h417_gd5f1g_spi1_context_t *context,
                                   uint8_t mode)
{
    SPI_InitTypeDef spi = {0};

    SPI_Cmd(SPI1, DISABLE);
    spi1_flush_rx();

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    if(mode == CH32H417_GD5F1G_SPI_MODE0)
    {
        spi.SPI_CPOL = SPI_CPOL_Low;
        spi.SPI_CPHA = SPI_CPHA_1Edge;
        context->active_mode = CH32H417_GD5F1G_SPI_MODE0;
    }
    else
    {
        spi.SPI_CPOL = SPI_CPOL_High;
        spi.SPI_CPHA = SPI_CPHA_2Edge;
        context->active_mode = CH32H417_GD5F1G_SPI_MODE3;
    }
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_Mode7;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7u;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);
}

static void spi1_delay_us(void *context, uint32_t us)
{
    volatile uint32_t i;
    (void)context;

    while(us != 0u)
    {
        for(i = 0; i < 25u; ++i)
        {
            __asm__ volatile("nop");
        }
        --us;
    }
}

static void spi1_select(void *context)
{
    (void)context;
    GPIO_ResetBits(FLASH_CS_PORT, FLASH_CS_PIN);
}

static void spi1_deselect(void *context)
{
    (void)context;
    GPIO_SetBits(FLASH_CS_PORT, FLASH_CS_PIN);
}

static uint8_t spi1_transfer(void *context, uint8_t tx)
{
    ch32h417_gd5f1g_spi1_context_t *spi_context;
    uint32_t timeout;

    spi_context = (ch32h417_gd5f1g_spi1_context_t *)context;

    timeout = 1000000u;
    while((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) && (timeout != 0u))
    {
        --timeout;
    }
    if(timeout == 0u)
    {
        spi_context->timeout_count++;
        return 0xFFu;
    }

    SPI_I2S_SendData(SPI1, tx);

    timeout = 1000000u;
    while((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) && (timeout != 0u))
    {
        --timeout;
    }
    if(timeout == 0u)
    {
        spi_context->timeout_count++;
        return 0xFFu;
    }

    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

static uint8_t gpio_transfer(void *context, uint8_t tx)
{
    uint8_t rx = 0u;
    uint8_t mask;

    (void)context;

    for(mask = 0x80u; mask != 0u; mask >>= 1)
    {
        if((tx & mask) != 0u)
        {
            GPIO_SetBits(FLASH_CS_PORT, FLASH_MOSI_PIN);
        }
        else
        {
            GPIO_ResetBits(FLASH_CS_PORT, FLASH_MOSI_PIN);
        }

        gpio_delay();
        GPIO_SetBits(FLASH_CS_PORT, FLASH_SCK_PIN);
        gpio_delay();
        rx <<= 1;
        if(GPIO_ReadInputDataBit(FLASH_CS_PORT, FLASH_MISO_PIN) != Bit_RESET)
        {
            rx |= 1u;
        }
        GPIO_ResetBits(FLASH_CS_PORT, FLASH_SCK_PIN);
        gpio_delay();
    }

    return rx;
}

static uint8_t gpio_slow_transfer(uint8_t tx)
{
    uint8_t rx = 0u;
    uint8_t mask;

    for(mask = 0x80u; mask != 0u; mask >>= 1)
    {
        if((tx & mask) != 0u)
        {
            GPIO_SetBits(FLASH_CS_PORT, FLASH_MOSI_PIN);
        }
        else
        {
            GPIO_ResetBits(FLASH_CS_PORT, FLASH_MOSI_PIN);
        }

        gpio_slow_delay();
        GPIO_SetBits(FLASH_CS_PORT, FLASH_SCK_PIN);
        gpio_slow_delay();
        rx <<= 1;
        if(GPIO_ReadInputDataBit(FLASH_CS_PORT, FLASH_MISO_PIN) != Bit_RESET)
        {
            rx |= 1u;
        }
        GPIO_ResetBits(FLASH_CS_PORT, FLASH_SCK_PIN);
        gpio_slow_delay();
    }

    return rx;
}

void ch32h417_gd5f1g_spi1_init(ch32h417_gd5f1g_spi1_context_t *context,
                               gd5f1g_spi_bus_t *bus)
{
    GPIO_InitTypeDef gpio = {0};

    context->timeout_count = 0u;
    context->active_mode = CH32H417_GD5F1G_SPI_MODE3;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOF |
                          RCC_HB2Periph_SPI1,
                          ENABLE);

    gpio.GPIO_Pin = FLASH_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(FLASH_CS_PORT, &gpio);
    GPIO_SetBits(FLASH_CS_PORT, FLASH_CS_PIN);

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource7, GPIO_AF3);
    gpio.GPIO_Pin = FLASH_SCK_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOF, &gpio);

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource8, GPIO_AF3);
    gpio.GPIO_Pin = FLASH_MOSI_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOF, &gpio);

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource9, GPIO_AF3);
    gpio.GPIO_Pin = FLASH_MISO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOF, &gpio);

    SPI_I2S_DeInit(SPI1);
    ch32h417_gd5f1g_spi1_set_mode(context, CH32H417_GD5F1G_SPI_MODE3);

    bus->context = context;
    bus->transfer = spi1_transfer;
    bus->select = spi1_select;
    bus->deselect = spi1_deselect;
    bus->delay_us = spi1_delay_us;
}

static void gpio_bus_init_with_miso(ch32h417_gd5f1g_spi1_context_t *context,
                                    gd5f1g_spi_bus_t *bus,
                                    GPIOMode_TypeDef miso_mode)
{
    GPIO_InitTypeDef gpio = {0};

    SPI_Cmd(SPI1, DISABLE);
    spi1_flush_rx();

    context->active_mode = CH32H417_GD5F1G_SPI_GPIO;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOF,
                          ENABLE);

    gpio.GPIO_Pin = FLASH_CS_PIN | FLASH_SCK_PIN | FLASH_MOSI_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(FLASH_CS_PORT, &gpio);

    gpio.GPIO_Pin = FLASH_MISO_PIN;
    gpio.GPIO_Mode = miso_mode;
    GPIO_Init(FLASH_CS_PORT, &gpio);
    flash_gpio_idle();

    bus->context = context;
    bus->transfer = gpio_transfer;
    bus->select = spi1_select;
    bus->deselect = spi1_deselect;
    bus->delay_us = spi1_delay_us;
}

void ch32h417_gd5f1g_gpio_init(ch32h417_gd5f1g_spi1_context_t *context,
                               gd5f1g_spi_bus_t *bus)
{
    gpio_bus_init_with_miso(context, bus, GPIO_Mode_IN_FLOATING);
}

void ch32h417_gd5f1g_gpio_pullup_init(ch32h417_gd5f1g_spi1_context_t *context,
                                      gd5f1g_spi_bus_t *bus)
{
    gpio_bus_init_with_miso(context, bus, GPIO_Mode_IPU);
}

void ch32h417_gd5f1g_gpio_read_id_slow(uint8_t *manufacturer_id,
                                       uint8_t *device_id)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t mid;
    uint8_t did;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOF,
                          ENABLE);

    gpio.GPIO_Pin = FLASH_CS_PIN | FLASH_SCK_PIN | FLASH_MOSI_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(FLASH_CS_PORT, &gpio);

    gpio.GPIO_Pin = FLASH_MISO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FLASH_CS_PORT, &gpio);

    flash_gpio_idle();
    gpio_slow_delay();
    GPIO_ResetBits(FLASH_CS_PORT, FLASH_CS_PIN);
    gpio_slow_delay();
    (void)gpio_slow_transfer(0x9Fu);
    (void)gpio_slow_transfer(0x00u);
    mid = gpio_slow_transfer(0xFFu);
    did = gpio_slow_transfer(0xFFu);
    gpio_slow_delay();
    GPIO_SetBits(FLASH_CS_PORT, FLASH_CS_PIN);
    gpio_slow_delay();

    if(manufacturer_id != 0)
    {
        *manufacturer_id = mid;
    }
    if(device_id != 0)
    {
        *device_id = did;
    }
}
