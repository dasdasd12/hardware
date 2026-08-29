/*
 * H4V1 Flash Stage 1: synchronous, read-only, post-PASS probe.
 *
 * Isolation contract:
 *   - no INIT_* export, no thread, no timer and no persistent state;
 *   - no SDRAM or DMA access;
 *   - PF6..PF9 / SPI1 are first touched by the explicit entry point;
 *   - the only NAND opcodes emitted are READ ID (9f) and GET FEATURE (0f);
 *   - RESET, SET FEATURE, PAGE READ, PROGRAM and ERASE are forbidden;
 *   - every function and string owned by this translation unit lives in a
 *     dedicated .h4v1_postpass* input section for tail placement by the
 *     install-only linker script.
 */
#include "h4v1_flash_postpass_stage1.h"

#include <stdint.h>

#include <rtthread.h>

#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"

#define H4V1_POSTPASS_TEXT \
    __attribute__((section(".h4v1_postpass.text"), noinline))
#define H4V1_POSTPASS_ENTRY \
    __attribute__((section(".h4v1_postpass.text"), noinline, used))
#define H4V1_POSTPASS_RODATA \
    __attribute__((section(".h4v1_postpass.rodata"), aligned(4), used))

#define H4V1_FLASH_CS_PIN              GPIO_Pin_6
#define H4V1_FLASH_SCK_PIN             GPIO_Pin_7
#define H4V1_FLASH_MOSI_PIN            GPIO_Pin_8
#define H4V1_FLASH_MISO_PIN            GPIO_Pin_9

#define H4V1_SPI_CTLR1_SPE             0x0040u
#define H4V1_SPI_CTLR1_MASTER          0x0004u
#define H4V1_SPI_CTLR1_CPOL            0x0002u
#define H4V1_SPI_CTLR1_CPHA            0x0001u
#define H4V1_SPI_CTLR1_SSM             0x0200u
#define H4V1_SPI_CTLR1_SSI             0x0100u
#define H4V1_SPI_CTLR1_BR_MODE2        0x0010u
#define H4V1_SPI_STATR_RXNE            0x0001u
#define H4V1_SPI_STATR_TXE             0x0002u
#define H4V1_SPI_MODE_SELECT           0xF7FFu
#define H4V1_SPI_TRANSFER_TIMEOUT      1000000u

#define H4V1_FLASH_SPI_MODE0           0u
#define H4V1_FLASH_SPI_MODE3           3u
#define H4V1_FLASH_CMD_GET_FEATURE     0x0Fu
#define H4V1_FLASH_CMD_READ_ID         0x9Fu
#define H4V1_FLASH_FEATURE_PROTECTION  0xA0u
#define H4V1_FLASH_FEATURE_CONFIG      0xB0u
#define H4V1_FLASH_FEATURE_STATUS      0xC0u
#define H4V1_FLASH_FEATURE_STATUS2     0xF0u
#define H4V1_FLASH_EXPECTED_MID        0xC8u
#define H4V1_FLASH_EXPECTED_DID        0x91u

typedef struct
{
    uint32_t timeout_count;
    uint8_t active_mode;
} h4v1_postpass_spi1_context_t;

extern int ch32h417_usb_cdc_write(const void *data, rt_uint32_t len);
extern void ch32h417_dual_cdc_poll(void);

const char h4v1_postpass_stage1_start_text[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE1 START timing=after_h4v1_pass access=read_only "
    "pins=PF6-PF9 commands=9f,0f no_reset=1 no_dma=1 no_sdram=1";
const char h4v1_postpass_stage1_pass_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE1 PASS mid=%02x did=%02x mode=%u "
    "a0=%02x b0=%02x c0=%02x f0=%02x spi_timeout=%u";
const char h4v1_postpass_stage1_fail_format[] H4V1_POSTPASS_RODATA =
    "FLASH STAGE1 FAIL result=%d id_rc=%d feature_rc=%d "
    "mid=%02x did=%02x mode=%u a0=%02x b0=%02x c0=%02x f0=%02x "
    "spi_timeout=%u";

uint32_t H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_strlen(const char *text)
{
    uint32_t length = 0u;

    if(text == RT_NULL)
    {
        return 0u;
    }
    /* Volatile read keeps GCC from replacing this isolated helper with a
     * newly introduced libc strlen reference. */
    while(*(volatile const char *)&text[length] != '\0')
    {
        length++;
    }
    return length;
}

void H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_log(const char *text)
{
    uint32_t offset = 0u;
    uint32_t length;
    rt_tick_t deadline;
    char ending[2];

    if(text == RT_NULL)
    {
        return;
    }
    length = h4v1_postpass_stage1_strlen(text);
    deadline = rt_tick_get() + (2u * RT_TICK_PER_SECOND);
    while(offset < length)
    {
        int wrote = ch32h417_usb_cdc_write(&text[offset],
                                            (rt_uint32_t)(length - offset));

        if(wrote > 0)
        {
            offset += (uint32_t)wrote;
        }
        else if((rt_int32_t)(rt_tick_get() - deadline) >= 0)
        {
            return;
        }
        ch32h417_dual_cdc_poll();
    }

    ending[0] = '\r';
    ending[1] = '\n';
    offset = 0u;
    while(offset < sizeof(ending))
    {
        int wrote = ch32h417_usb_cdc_write(&ending[offset],
                                            (rt_uint32_t)(sizeof(ending) - offset));

        if(wrote > 0)
        {
            offset += (uint32_t)wrote;
        }
        else if((rt_int32_t)(rt_tick_get() - deadline) >= 0)
        {
            return;
        }
        ch32h417_dual_cdc_poll();
    }
}

void H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_spi_flush_rx(void)
{
    while((SPI1->STATR & H4V1_SPI_STATR_RXNE) != 0u)
    {
        (void)SPI1->DATAR;
    }
}

void H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_spi_select(void)
{
    GPIOF->BCR = H4V1_FLASH_CS_PIN;
}

void H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_spi_deselect(void)
{
    GPIOF->BSHR = H4V1_FLASH_CS_PIN;
}

uint8_t H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_spi_transfer(h4v1_postpass_spi1_context_t *context,
                                   uint8_t tx)
{
    uint32_t timeout = H4V1_SPI_TRANSFER_TIMEOUT;

    while(((SPI1->STATR & H4V1_SPI_STATR_TXE) == 0u) &&
          (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        context->timeout_count++;
        return 0xFFu;
    }
    SPI1->DATAR = tx;

    timeout = H4V1_SPI_TRANSFER_TIMEOUT;
    while(((SPI1->STATR & H4V1_SPI_STATR_RXNE) == 0u) &&
          (timeout != 0u))
    {
        timeout--;
    }
    if(timeout == 0u)
    {
        context->timeout_count++;
        return 0xFFu;
    }
    return (uint8_t)SPI1->DATAR;
}

void H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_spi_set_mode(h4v1_postpass_spi1_context_t *context,
                                   uint8_t mode)
{
    uint16_t ctlr1 = H4V1_SPI_CTLR1_MASTER |
                     H4V1_SPI_CTLR1_SSI |
                     H4V1_SPI_CTLR1_SSM |
                     H4V1_SPI_CTLR1_BR_MODE2;

    SPI1->CTLR1 &= (uint16_t)~H4V1_SPI_CTLR1_SPE;
    h4v1_postpass_stage1_spi_flush_rx();
    if(mode == H4V1_FLASH_SPI_MODE0)
    {
        context->active_mode = H4V1_FLASH_SPI_MODE0;
    }
    else
    {
        ctlr1 |= H4V1_SPI_CTLR1_CPOL | H4V1_SPI_CTLR1_CPHA;
        context->active_mode = H4V1_FLASH_SPI_MODE3;
    }
    SPI1->CTLR1 = ctlr1;
    SPI1->I2SCFGR &= H4V1_SPI_MODE_SELECT;
    SPI1->CRCR = 7u;
    SPI1->CTLR1 = ctlr1 | H4V1_SPI_CTLR1_SPE;
}

void H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_spi_init(h4v1_postpass_spi1_context_t *context)
{
    GPIO_InitTypeDef gpio = {0};

    context->timeout_count = 0u;
    context->active_mode = H4V1_FLASH_SPI_MODE3;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOF |
                          RCC_HB2Periph_SPI1,
                          ENABLE);
    RCC_HB2PeriphResetCmd(RCC_HB2Periph_SPI1, ENABLE);
    RCC_HB2PeriphResetCmd(RCC_HB2Periph_SPI1, DISABLE);

    gpio.GPIO_Pin = H4V1_FLASH_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOF, &gpio);
    h4v1_postpass_stage1_spi_deselect();

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource7, GPIO_AF3);
    gpio.GPIO_Pin = H4V1_FLASH_SCK_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOF, &gpio);

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource8, GPIO_AF3);
    gpio.GPIO_Pin = H4V1_FLASH_MOSI_PIN;
    GPIO_Init(GPIOF, &gpio);

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource9, GPIO_AF3);
    gpio.GPIO_Pin = H4V1_FLASH_MISO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOF, &gpio);

    h4v1_postpass_stage1_spi_set_mode(context, H4V1_FLASH_SPI_MODE3);
}

int H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_read_id(h4v1_postpass_spi1_context_t *context,
                              uint8_t *manufacturer,
                              uint8_t *device)
{
    uint32_t timeout_before = context->timeout_count;

    if(timeout_before != 0u)
    {
        return -2;
    }
    h4v1_postpass_stage1_spi_select();
    (void)h4v1_postpass_stage1_spi_transfer(context,
                                             H4V1_FLASH_CMD_READ_ID);
    if(context->timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_postpass_stage1_spi_transfer(context, 0x00u);
    if(context->timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    *manufacturer = h4v1_postpass_stage1_spi_transfer(context, 0x00u);
    if(context->timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    *device = h4v1_postpass_stage1_spi_transfer(context, 0x00u);
    if(context->timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    h4v1_postpass_stage1_spi_deselect();

    return ((*manufacturer == H4V1_FLASH_EXPECTED_MID) &&
            (*device == H4V1_FLASH_EXPECTED_DID)) ? 0 : -1;

transfer_failed:
    h4v1_postpass_stage1_spi_deselect();
    return -2;
}

int H4V1_POSTPASS_TEXT
h4v1_postpass_stage1_get_feature(h4v1_postpass_spi1_context_t *context,
                                  uint8_t address,
                                  uint8_t *value)
{
    uint32_t timeout_before = context->timeout_count;

    if(timeout_before != 0u)
    {
        return -1;
    }
    h4v1_postpass_stage1_spi_select();
    (void)h4v1_postpass_stage1_spi_transfer(context,
                                             H4V1_FLASH_CMD_GET_FEATURE);
    if(context->timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    (void)h4v1_postpass_stage1_spi_transfer(context, address);
    if(context->timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    *value = h4v1_postpass_stage1_spi_transfer(context, 0x00u);
    if(context->timeout_count != timeout_before)
    {
        goto transfer_failed;
    }
    h4v1_postpass_stage1_spi_deselect();
    return 0;

transfer_failed:
    h4v1_postpass_stage1_spi_deselect();
    return -1;
}

int H4V1_POSTPASS_ENTRY
h4v1_flash_postpass_stage1_run(void)
{
    h4v1_postpass_spi1_context_t context;
    uint8_t manufacturer = 0u;
    uint8_t device = 0u;
    uint8_t protection = 0u;
    uint8_t config = 0u;
    uint8_t status = 0u;
    uint8_t status2 = 0u;
    int id_result;
    int feature_result = 0;
    int result = H4V1_FLASH_POSTPASS_STAGE1_OK;
    char line[224];

    h4v1_postpass_stage1_log(h4v1_postpass_stage1_start_text);
    h4v1_postpass_stage1_spi_init(&context);

    id_result = h4v1_postpass_stage1_read_id(&context,
                                              &manufacturer,
                                              &device);
    if(id_result != 0)
    {
        h4v1_postpass_stage1_spi_set_mode(&context,
                                           H4V1_FLASH_SPI_MODE0);
        id_result = h4v1_postpass_stage1_read_id(&context,
                                                  &manufacturer,
                                                  &device);
    }

    if(id_result == 0)
    {
        if(h4v1_postpass_stage1_get_feature(
               &context, H4V1_FLASH_FEATURE_PROTECTION, &protection) != 0)
        {
            feature_result = -1;
        }
        if(h4v1_postpass_stage1_get_feature(
               &context, H4V1_FLASH_FEATURE_CONFIG, &config) != 0)
        {
            feature_result = -1;
        }
        if(h4v1_postpass_stage1_get_feature(
               &context, H4V1_FLASH_FEATURE_STATUS, &status) != 0)
        {
            feature_result = -1;
        }
        if(h4v1_postpass_stage1_get_feature(
               &context, H4V1_FLASH_FEATURE_STATUS2, &status2) != 0)
        {
            feature_result = -1;
        }
    }

    /* Leave NAND deselected even when a transfer timed out. */
    h4v1_postpass_stage1_spi_deselect();

    if(context.timeout_count != 0u)
    {
        result = H4V1_FLASH_POSTPASS_STAGE1_ERR_SPI_TIMEOUT;
    }
    else if(id_result != 0)
    {
        result = H4V1_FLASH_POSTPASS_STAGE1_ERR_ID;
    }
    else if(feature_result != 0)
    {
        result = H4V1_FLASH_POSTPASS_STAGE1_ERR_FEATURE;
    }

    if(result == H4V1_FLASH_POSTPASS_STAGE1_OK)
    {
        (void)rt_snprintf(line, sizeof(line),
                          h4v1_postpass_stage1_pass_format,
                          (unsigned int)manufacturer,
                          (unsigned int)device,
                          (unsigned int)context.active_mode,
                          (unsigned int)protection,
                          (unsigned int)config,
                          (unsigned int)status,
                          (unsigned int)status2,
                          (unsigned int)context.timeout_count);
    }
    else
    {
        (void)rt_snprintf(line, sizeof(line),
                          h4v1_postpass_stage1_fail_format,
                          result,
                          id_result,
                          feature_result,
                          (unsigned int)manufacturer,
                          (unsigned int)device,
                          (unsigned int)context.active_mode,
                          (unsigned int)protection,
                          (unsigned int)config,
                          (unsigned int)status,
                          (unsigned int)status2,
                          (unsigned int)context.timeout_count);
    }
    h4v1_postpass_stage1_log(line);
    return result;
}
