/*
 * CH585M scan-frame ingest prototype for CH32H417.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <rtthread.h>
#include <rtdevice.h>

#include "ch32h417_dma.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "ch32h417_spi.h"
#include "ch32h417_tim.h"

#include "ch585_spi_scan.h"

#if defined(__GNUC__)
#define CH585_MAYBE_UNUSED __attribute__((unused))
#else
#define CH585_MAYBE_UNUSED
#endif

extern uint32_t SystemCoreClock;
extern uint32_t HCLKClock;

#ifndef APP_CH585_SPI_REAL_SOURCE0
#define APP_CH585_SPI_REAL_SOURCE0 1
#endif

#ifndef APP_CH585_SPI_FAKE_SOURCE1
#define APP_CH585_SPI_FAKE_SOURCE1 1
#endif

#ifndef APP_CH585_SPI_SOFT_DELAY_CYCLES
#define APP_CH585_SPI_SOFT_DELAY_CYCLES 300U
#endif

#ifndef APP_CH585_SPI_SAMPLE_TRAILING
#define APP_CH585_SPI_SAMPLE_TRAILING 0
#endif

#ifndef APP_CH585_SPI_CS_SETUP_MS
#define APP_CH585_SPI_CS_SETUP_MS 1U
#endif

#ifndef APP_CH585_SPI_CMD_TO_DATA_MS
#define APP_CH585_SPI_CMD_TO_DATA_MS 0U
#endif

#ifndef APP_CH585_SPI_CMD_TO_DATA_US
#define APP_CH585_SPI_CMD_TO_DATA_US 100U
#endif

#ifndef APP_CH585_SPI_RESYNC_TO_CMD_US
#define APP_CH585_SPI_RESYNC_TO_CMD_US 1000U
#endif

#ifndef APP_CH585_SPI_RESYNC_EVERY_POLL
#define APP_CH585_SPI_RESYNC_EVERY_POLL 0
#endif

#ifndef APP_CH585_SPI_PIPELINE_SHORT
#define APP_CH585_SPI_PIPELINE_SHORT 0
#endif

#ifndef APP_CH585_SPI_REQUEST_ONLY_SHORT
#define APP_CH585_SPI_REQUEST_ONLY_SHORT APP_CH585_SPI_WIRE_SHORT
#endif

#ifndef APP_CH585_SPI_DMA_BACKEND
#define APP_CH585_SPI_DMA_BACKEND 1
#endif

#ifndef APP_CH585_SPI_HW_SPI2_BACKEND
#define APP_CH585_SPI_HW_SPI2_BACKEND 1
#endif

#ifndef APP_CH585_SPI_HW_SPI2_PRESCALER
#define APP_CH585_SPI_HW_SPI2_PRESCALER SPI_BaudRatePrescaler_Mode4
#endif

#ifndef APP_CH585_SPI_HW_SPI2_HIGHSPEED
#define APP_CH585_SPI_HW_SPI2_HIGHSPEED 2
#endif

#ifndef APP_CH585_SPI_AUTO_TRAIN
#define APP_CH585_SPI_AUTO_TRAIN 1
#endif

#ifndef APP_CH585_SPI_AUTO_TRAIN_ON_INIT
#define APP_CH585_SPI_AUTO_TRAIN_ON_INIT 0
#endif

#ifndef APP_CH585_SPI_AUTO_TRAIN_AFTER_POLLS
#define APP_CH585_SPI_AUTO_TRAIN_AFTER_POLLS 8U
#endif

#ifndef APP_CH585_SPI_TRAIN_FRAMES
#define APP_CH585_SPI_TRAIN_FRAMES 64U
#endif

#ifndef APP_CH585_SPI_TRAIN_WARMUP_FRAMES
#define APP_CH585_SPI_TRAIN_WARMUP_FRAMES 8U
#endif

#ifndef APP_CH585_SPI_TRAIN_INTERFRAME_US
#define APP_CH585_SPI_TRAIN_INTERFRAME_US 125U
#endif

#ifndef APP_CH585_SPI_TRAIN_MAX_CANDIDATES
#define APP_CH585_SPI_TRAIN_MAX_CANDIDATES 16U
#endif

#ifndef APP_CH585_SPI_HW_SPI2_ENABLE_HSLV
#define APP_CH585_SPI_HW_SPI2_ENABLE_HSLV 1
#endif

#ifndef APP_CH585_SPI_HW_SPI2_GPIO_CS
#define APP_CH585_SPI_HW_SPI2_GPIO_CS 1
#endif

#ifndef APP_CH585_SPI_DMA_TARGET_KHZ
#define APP_CH585_SPI_DMA_TARGET_KHZ 70000U
#endif

#ifndef APP_CH585_SPI_SOURCE0_CAPTURE_BYTES
#define APP_CH585_SPI_SOURCE0_CAPTURE_BYTES sizeof(ch585_scan_frame_v1_t)
#endif

#ifndef APP_CH585_SPI_PIN_SCK
#define APP_CH585_SPI_PIN_SCK  "PD.2"
#endif

#ifndef APP_CH585_SPI_PIN_MOSI
#define APP_CH585_SPI_PIN_MOSI "PD.3"
#endif

#ifndef APP_CH585_SPI_PIN_MISO0
#define APP_CH585_SPI_PIN_MISO0 "PD.4"
#endif

#ifndef APP_CH585_SPI_PIN_CS0
#define APP_CH585_SPI_PIN_CS0 "PD.6"
#endif

#define CH585_SPI_DMA_PHASES_PER_BIT 2U
#define CH585_SPI_DMA_SAMPLE_PHASE   0U
#define CH585_SPI_DMA_PHASE_COUNT \
    ((uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U * CH585_SPI_DMA_PHASES_PER_BIT)
#define CH585_SPI_DMA_TIM8_UP_REQ    36U

#define CH585_SPI_BACKEND_CPU_GPIO       0U
#define CH585_SPI_BACKEND_DMA_GPIO       1U
#define CH585_SPI_BACKEND_HW_SPI2_HARDNSS 2U

#define CH585_SPI2_TX_DMA_REQ 65U
#define CH585_SPI2_RX_DMA_REQ 66U

#ifndef APP_CH585_SPI_DMA_USE_OUTDR
#define APP_CH585_SPI_DMA_USE_OUTDR 1
#endif

typedef struct
{
    ch585_scan_source_stats_t source[CH585_SCAN_SOURCE_COUNT];
    uint32_t poll_count;
    uint16_t raw[CH585_SCAN_TOTAL_KEYS];
    uint8_t source0_head[8];
    uint8_t source0_tail[8];
    uint8_t source0_req_rx[4];
    uint8_t source0_miso_idle[3];
    uint8_t source0_sync_found;
    uint8_t source0_sync_bit;
    uint16_t source0_sync_byte;
    uint32_t source0_first_bit_repairs;
    uint32_t source0_first_bit_repair_crc_misses;
    uint16_t source0_first_bit_repair_rx_crc;
    uint16_t source0_first_bit_repair_expected_crc;
    uint8_t source0_bad_head[8];
    uint32_t source0_xfer_cycles;
    uint32_t source0_xfer_us;
    uint32_t source0_sck_khz_x10;
    uint32_t source0_frame_fps_x10;
    uint8_t source0_backend;
    uint8_t source0_dma_ready;
    uint16_t source0_dma_period;
    uint32_t source0_dma_runs;
    uint32_t source0_dma_timeouts;
    uint32_t source0_dma_te_flags;
    uint16_t source0_dma_sample_or;
    uint16_t source0_dma_sample_and;
    uint8_t source0_spi2_ready;
    uint8_t source0_need_resync;
    uint16_t source0_spi2_prescaler;
    uint8_t source0_spi2_hsrx;
    uint16_t source0_spi2_cpha;
    uint16_t source0_train_errors;
    uint16_t source0_train_frames;
    uint8_t source0_train_done;
    uint8_t source0_train_candidate_count;
    uint16_t source0_train_candidate_prescaler[APP_CH585_SPI_TRAIN_MAX_CANDIDATES];
    uint16_t source0_train_candidate_bad[APP_CH585_SPI_TRAIN_MAX_CANDIDATES];
    uint16_t source0_train_candidate_seq[APP_CH585_SPI_TRAIN_MAX_CANDIDATES];
    uint8_t source0_train_candidate_hsrx[APP_CH585_SPI_TRAIN_MAX_CANDIDATES];
    uint8_t source0_train_candidate_cpha[APP_CH585_SPI_TRAIN_MAX_CANDIDATES];
    uint16_t source0_host_seq;
    uint16_t source0_accept_ack_seq;
    uint16_t source0_pipeline_ack_seq;
    uint8_t source0_pipeline_primed;
    uint32_t source0_resync_runs;
    uint32_t source0_cmd_runs;
    uint32_t source0_cmd_timeouts;
    uint32_t source0_ack_errors;
    uint32_t source0_spi2_runs;
    uint32_t source0_spi2_timeouts;
    uint32_t source0_spi2_te_flags;
    uint16_t source0_spi2_sr_last;
    rt_base_t source1_gpio_cs;
} ch585_scan_runtime_t;

typedef struct
{
    rt_base_t sck;
    rt_base_t mosi;
    rt_base_t miso0;
    rt_base_t cs0;
    uint8_t ready;
} ch585_soft_spi_t;

static ch585_scan_runtime_t g_scan;
static ch585_soft_spi_t g_soft_spi;
static uint8_t g_source0_capture[APP_CH585_SPI_SOURCE0_CAPTURE_BYTES];

#if APP_CH585_SPI_HW_SPI2_BACKEND
static uint8_t g_source0_spi2_tx[APP_CH585_SPI_SOURCE0_CAPTURE_BYTES] __attribute__((aligned(4)));
static uint8_t g_source0_spi2_rx[APP_CH585_SPI_SOURCE0_CAPTURE_BYTES] __attribute__((aligned(4)));
static ch585_scan_cmd_v1_t g_source0_spi2_cmd_tx __attribute__((aligned(4)));
static ch585_scan_cmd_v1_t g_source0_spi2_cmd_rx __attribute__((aligned(4)));
#endif

#if APP_CH585_SPI_HW_SPI2_BACKEND
static int ch585_hw_spi2_train(void);
static int ch585_scan_decode_source0_capture(ch585_scan_frame_v1_t *frame);
static int ch585_scan_frame_is_valid(const ch585_scan_frame_v1_t *frame);

static uint32_t ch585_hw_spi2_prescaler_divisor(void)
{
    uint16_t prescaler = (g_scan.source0_spi2_ready != 0U) ?
                         g_scan.source0_spi2_prescaler :
                         APP_CH585_SPI_HW_SPI2_PRESCALER;

    if (g_scan.source0_spi2_hsrx != 0U)
    {
        switch (prescaler)
        {
        case SPI_BaudRatePrescaler_Mode0: return 2U;
        case SPI_BaudRatePrescaler_Mode1: return 3U;
        case SPI_BaudRatePrescaler_Mode2: return 4U;
        case SPI_BaudRatePrescaler_Mode3: return 5U;
        case SPI_BaudRatePrescaler_Mode4: return 6U;
        case SPI_BaudRatePrescaler_Mode5: return 7U;
        case SPI_BaudRatePrescaler_Mode6: return 8U;
        case SPI_BaudRatePrescaler_Mode7: return 9U;
        default: return 4U;
        }
    }

    switch (prescaler)
    {
    case SPI_BaudRatePrescaler_Mode0: return 2U;
    case SPI_BaudRatePrescaler_Mode1: return 4U;
    case SPI_BaudRatePrescaler_Mode2: return 8U;
    case SPI_BaudRatePrescaler_Mode3: return 16U;
    case SPI_BaudRatePrescaler_Mode4: return 32U;
    case SPI_BaudRatePrescaler_Mode5: return 64U;
    case SPI_BaudRatePrescaler_Mode6: return 128U;
    case SPI_BaudRatePrescaler_Mode7: return 256U;
    default: return 8U;
    }
}

static uint32_t ch585_hw_spi2_expected_sck_khz_x10(void)
{
    uint32_t clk = (HCLKClock != 0U) ? HCLKClock : SystemCoreClock;
    uint32_t div = ch585_hw_spi2_prescaler_divisor();

    if ((clk == 0U) || (div == 0U))
    {
        return 0U;
    }

    return (uint32_t)((((uint64_t)clk * 10ULL) + ((uint64_t)div * 1000ULL / 2ULL)) /
                      ((uint64_t)div * 1000ULL));
}

static const char *ch585_scan_source0_backend_name(void)
{
    switch (g_scan.source0_backend)
    {
    case CH585_SPI_BACKEND_HW_SPI2_HARDNSS:
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
        return "hw-spi2-gpiocs";
#else
        return "hw-spi2-hardnss";
#endif
    case CH585_SPI_BACKEND_DMA_GPIO:
        return "dma-gpio";
    case CH585_SPI_BACKEND_CPU_GPIO:
    default:
        return "cpu-gpio";
    }
}

static void ch585_hw_spi2_apply_config(uint16_t prescaler, uint8_t hsrx, uint16_t cpha)
{
    SPI_InitTypeDef spi = {0};

    SPI_Cmd(SPI2, DISABLE);
    SPI_I2S_DeInit(SPI2);

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    SPI_SSOutputCmd(SPI2, DISABLE);
#else
    SPI_SSOutputCmd(SPI2, ENABLE);
#endif

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = cpha;
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    spi.SPI_NSS = SPI_NSS_Soft;
#else
    spi.SPI_NSS = SPI_NSS_Hard;
#endif
    spi.SPI_BaudRatePrescaler = prescaler;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7U;
    SPI_Init(SPI2, &spi);

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    SPI_NSSInternalSoftwareConfig(SPI2, SPI_NSSInternalSoft_Set);
#endif

    if (hsrx == 1U)
    {
        SPI_HighSpeedMode_Config(SPI2, SPI_HIGH_SPEED_MODE1, ENABLE);
    }
    else if (hsrx == 2U)
    {
        SPI_HighSpeedMode_Config(SPI2, SPI_HIGH_SPEED_MODE2, ENABLE);
    }
    else
    {
        SPI_HighSpeedMode_Config(SPI2, SPI_HIGH_SPEED_MODE1, DISABLE);
    }

    SPI_Cmd(SPI2, DISABLE);
    g_scan.source0_spi2_prescaler = prescaler;
    g_scan.source0_spi2_hsrx = hsrx;
    g_scan.source0_spi2_cpha = cpha;
}

static int ch585_hw_spi2_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOC |
                          RCC_HB2Periph_GPIOD, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);

#if APP_CH585_SPI_HW_SPI2_ENABLE_HSLV
    GPIO_PinRemapConfig(GPIO_Remap_VIO3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VDD3V3_IO_HSLV, ENABLE);
#endif

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    /* SPI2 uses hardware SCK/MOSI/MISO; PB12 is a manual GPIO CS for CH585 #0. */
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &gpio);
#else
    /* Method 2: SPI2 hardware NSS selects CH585 #0, PD7 is reserved for CH585 #1. */
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);
#endif

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_13;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_1;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &gpio);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource2, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_2;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_7;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOD, &gpio);
    GPIOD->BSHR = GPIO_Pin_7;
    g_scan.source1_gpio_cs = rt_pin_get("PD.7");

    ch585_hw_spi2_apply_config(APP_CH585_SPI_HW_SPI2_PRESCALER,
                               APP_CH585_SPI_HW_SPI2_HIGHSPEED,
                               SPI_CPHA_1Edge);
    g_scan.source0_spi2_ready = 1U;

#if APP_CH585_SPI_AUTO_TRAIN && APP_CH585_SPI_AUTO_TRAIN_ON_INIT
    (void)ch585_hw_spi2_train();
#endif

    rt_kprintf("CH585 source0 HW SPI2 method2: PB12=%s PB13=SCK PC1=MOSI PC2=MISO, CS1=PD7, prescaler=0x%04x hsrx=%u cpha=%u expect=%u.%u kHz hclk=%u core=%u hslv=%u\r\n",
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
               "GPIO-CS",
#else
               "NSS",
#endif
               (unsigned int)g_scan.source0_spi2_prescaler,
               (unsigned int)g_scan.source0_spi2_hsrx,
               (unsigned int)((g_scan.source0_spi2_cpha == SPI_CPHA_2Edge) ? 2U : 1U),
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() / 10U),
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() % 10U),
               (unsigned int)HCLKClock,
               (unsigned int)SystemCoreClock,
               (unsigned int)APP_CH585_SPI_HW_SPI2_ENABLE_HSLV);
    return 0;
}
#else
static const char *ch585_scan_source0_backend_name(void)
{
    return (g_scan.source0_backend != 0U) ? "dma-gpio" : "cpu-gpio";
}
#endif

#if APP_CH585_SPI_DMA_BACKEND
#if APP_CH585_SPI_DMA_USE_OUTDR
static uint16_t g_source0_dma_sck_wave[CH585_SPI_DMA_PHASE_COUNT] __attribute__((aligned(4)));
#else
static uint32_t g_source0_dma_sck_wave[CH585_SPI_DMA_PHASE_COUNT] __attribute__((aligned(4)));
#endif
static uint16_t g_source0_dma_miso_sample[CH585_SPI_DMA_PHASE_COUNT] __attribute__((aligned(4)));
#endif

static uint16_t g_fake_seq[CH585_SCAN_SOURCE_COUNT];

static uint32_t ch585_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static void ch585_cycle_delay_us(uint32_t us)
{
    uint32_t cycles;
    uint32_t start;

    if ((us == 0U) || (SystemCoreClock == 0U))
    {
        return;
    }

    cycles = (uint32_t)((((uint64_t)SystemCoreClock * (uint64_t)us) +
                         999999ULL) / 1000000ULL);
    start = ch585_cycle_now();
    while ((uint32_t)(ch585_cycle_now() - start) < cycles)
    {
    }
}

uint16_t ch585_spi_scan_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;

    for (i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

static uint16_t ch585_scan_expected_base(uint8_t source_id)
{
    return (uint16_t)source_id * CH585_SCAN_KEYS_PER_SOURCE;
}

static void ch585_scan_frame_set_down_bit(ch585_scan_frame_v1_t *frame, uint8_t key_id)
{
    frame->down_bits[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
}

#if APP_CH585_SPI_WIRE_SHORT
static uint8_t ch585_scan_flags_to_short(uint16_t flags)
{
    uint8_t out = 0U;

    if ((flags & CH585_SCAN_FLAG_OVERRUN) != 0U)
    {
        out |= CH585_SCAN_SHORT_FLAG_OVERRUN;
    }
    if ((flags & CH585_SCAN_FLAG_ADC_ERROR) != 0U)
    {
        out |= CH585_SCAN_SHORT_FLAG_ADC_ERROR;
    }
    if ((flags & CH585_SCAN_FLAG_STALE) != 0U)
    {
        out |= CH585_SCAN_SHORT_FLAG_STALE;
    }
    if ((flags & CH585_SCAN_FLAG_SYNC_LOST) != 0U)
    {
        out |= CH585_SCAN_SHORT_FLAG_SYNC_LOST;
    }
    if ((flags & CH585_SCAN_FLAG_CMD_ERROR) != 0U)
    {
        out |= CH585_SCAN_SHORT_FLAG_CMD_ERROR;
    }
    if ((flags & CH585_SCAN_FLAG_READY) != 0U)
    {
        out |= CH585_SCAN_SHORT_FLAG_READY;
    }

    return out;
}

static uint16_t ch585_scan_flags_from_short(uint8_t flags)
{
    uint16_t out = 0U;

    if ((flags & CH585_SCAN_SHORT_FLAG_OVERRUN) != 0U)
    {
        out |= CH585_SCAN_FLAG_OVERRUN;
    }
    if ((flags & CH585_SCAN_SHORT_FLAG_ADC_ERROR) != 0U)
    {
        out |= CH585_SCAN_FLAG_ADC_ERROR;
    }
    if ((flags & CH585_SCAN_SHORT_FLAG_STALE) != 0U)
    {
        out |= CH585_SCAN_FLAG_STALE;
    }
    if ((flags & CH585_SCAN_SHORT_FLAG_SYNC_LOST) != 0U)
    {
        out |= CH585_SCAN_FLAG_SYNC_LOST;
    }
    if ((flags & CH585_SCAN_SHORT_FLAG_CMD_ERROR) != 0U)
    {
        out |= CH585_SCAN_FLAG_CMD_ERROR;
    }
    if ((flags & CH585_SCAN_SHORT_FLAG_READY) != 0U)
    {
        out |= CH585_SCAN_FLAG_READY;
    }

    return out;
}
#endif

static int ch585_scan_fetch_source(uint8_t source_id, ch585_scan_frame_v1_t *frame)
{
    uint8_t i;
    uint16_t seq;
    uint16_t phase;

    if ((source_id >= CH585_SCAN_SOURCE_COUNT) || (frame == RT_NULL))
    {
        return -1;
    }

    seq = g_fake_seq[source_id]++;
    memset(frame, 0, sizeof(*frame));

#if APP_CH585_SPI_WIRE_SHORT
    frame->magic = CH585_SCAN_SHORT_FRAME_MAGIC;
    frame->type = CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE;
    frame->source_id = source_id;
    frame->seq = (uint8_t)seq;
    frame->ack_seq = 0xFFU;
    frame->flags = ch585_scan_flags_to_short(CH585_SCAN_FLAG_READY);
#else
    frame->magic = CH585_SCAN_FRAME_MAGIC;
    frame->version = CH585_SCAN_FRAME_VERSION;
    frame->type = CH585_SCAN_FRAME_TYPE_KEY_STATE;
    frame->source_id = source_id;
    frame->key_count = CH585_SCAN_KEYS_PER_SOURCE;
    frame->seq = seq;
    frame->flags = CH585_SCAN_FLAG_READY;
    frame->ack_seq = 0xFFFFU;
    frame->diag = 0U;
#endif

    for (i = 0; i < CH585_SCAN_KEYS_PER_SOURCE; i++)
    {
        phase = (uint16_t)((seq + ((uint16_t)i * 4U) + ((uint16_t)source_id * 8U)) & 31U);
        if ((i < 4U) && (phase >= 8U) && (phase < 24U))
        {
            ch585_scan_frame_set_down_bit(frame, i);
        }
    }

    frame->crc16 = ch585_spi_scan_crc16((const uint8_t *)frame,
                                        (uint16_t)offsetof(ch585_scan_frame_v1_t, crc16));
    return 0;
}

static void ch585_soft_spi_delay(void)
{
    volatile uint32_t i;

    for (i = 0; i < APP_CH585_SPI_SOFT_DELAY_CYCLES; i++)
    {
        __asm volatile ("nop");
    }
}

static int ch585_soft_spi_init(void)
{
    g_soft_spi.sck = rt_pin_get(APP_CH585_SPI_PIN_SCK);
    g_soft_spi.mosi = rt_pin_get(APP_CH585_SPI_PIN_MOSI);
    g_soft_spi.miso0 = rt_pin_get(APP_CH585_SPI_PIN_MISO0);
    g_soft_spi.cs0 = rt_pin_get(APP_CH585_SPI_PIN_CS0);

    if ((g_soft_spi.sck < 0) || (g_soft_spi.mosi < 0) ||
        (g_soft_spi.miso0 < 0) || (g_soft_spi.cs0 < 0))
    {
        rt_kprintf("CH585 soft SPI pin lookup failed: sck=%d mosi=%d miso0=%d cs0=%d\r\n",
                   (int)g_soft_spi.sck, (int)g_soft_spi.mosi,
                   (int)g_soft_spi.miso0, (int)g_soft_spi.cs0);
        return -1;
    }

    rt_pin_mode(g_soft_spi.sck, PIN_MODE_OUTPUT);
    rt_pin_mode(g_soft_spi.mosi, PIN_MODE_OUTPUT);
    rt_pin_mode(g_soft_spi.cs0, PIN_MODE_OUTPUT);
    rt_pin_mode(g_soft_spi.miso0, PIN_MODE_INPUT_PULLUP);

    rt_pin_write(g_soft_spi.cs0, PIN_HIGH);
    rt_pin_write(g_soft_spi.sck, PIN_LOW);
    rt_pin_write(g_soft_spi.mosi, PIN_HIGH);

    g_soft_spi.ready = 1U;
    rt_kprintf("CH585 soft SPI source0 pins: SCK=%s MOSI=%s MISO=%s CS=%s, idle-low sample=%s MSB-first\r\n",
               APP_CH585_SPI_PIN_SCK, APP_CH585_SPI_PIN_MOSI,
               APP_CH585_SPI_PIN_MISO0, APP_CH585_SPI_PIN_CS0,
#if APP_CH585_SPI_SAMPLE_TRAILING
               "trailing"
#else
               "leading"
#endif
    );
    return 0;
}

#if APP_CH585_SPI_DMA_BACKEND
static uint16_t ch585_dma_period_from_target(void)
{
    uint32_t denom;
    uint32_t ticks;
    uint32_t tim_clk;

    denom = APP_CH585_SPI_DMA_TARGET_KHZ * 1000U * CH585_SPI_DMA_PHASES_PER_BIT;
    tim_clk = (HCLKClock != 0U) ? HCLKClock : SystemCoreClock;
    if ((tim_clk == 0U) || (denom == 0U))
    {
        return 356U;
    }

    ticks = (uint32_t)((((uint64_t)tim_clk) + ((uint64_t)denom / 2ULL)) /
                       (uint64_t)denom);
    if (ticks < 2U)
    {
        ticks = 2U;
    }
    if (ticks > 65535U)
    {
        ticks = 65535U;
    }

    return (uint16_t)(ticks - 1U);
}

static void ch585_dma_wave_prepare(void)
{
    uint32_t bit;
    uint32_t phase;

#if APP_CH585_SPI_DMA_USE_OUTDR
    uint16_t base;
    uint16_t sck_high;

    base = (uint16_t)GPIOD->OUTDR;
    base |= GPIO_Pin_3;
    base &= (uint16_t)~(GPIO_Pin_2 | GPIO_Pin_6);
    sck_high = (uint16_t)(base | GPIO_Pin_2);

    for (bit = 0U; bit < ((uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U); bit++)
    {
        phase = bit * CH585_SPI_DMA_PHASES_PER_BIT;
#if CH585_SPI_DMA_PHASES_PER_BIT == 4U
        g_source0_dma_sck_wave[phase + 0U] = base;
        g_source0_dma_sck_wave[phase + 1U] = sck_high;
        g_source0_dma_sck_wave[phase + 2U] = sck_high;
        g_source0_dma_sck_wave[phase + 3U] = base;
#elif CH585_SPI_DMA_PHASES_PER_BIT == 3U
        g_source0_dma_sck_wave[phase + 0U] = sck_high;
        g_source0_dma_sck_wave[phase + 1U] = sck_high;
        g_source0_dma_sck_wave[phase + 2U] = base;
#elif CH585_SPI_DMA_PHASES_PER_BIT == 2U
        g_source0_dma_sck_wave[phase + 0U] = sck_high;
        g_source0_dma_sck_wave[phase + 1U] = base;
#else
#error "Unsupported CH585_SPI_DMA_PHASES_PER_BIT"
#endif
    }
#else
    for (bit = 0U; bit < ((uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U); bit++)
    {
        phase = bit * CH585_SPI_DMA_PHASES_PER_BIT;
#if CH585_SPI_DMA_PHASES_PER_BIT == 4U
        g_source0_dma_sck_wave[phase + 0U] = GPIO_BSHR_BR2;
        g_source0_dma_sck_wave[phase + 1U] = GPIO_BSHR_BS2;
        g_source0_dma_sck_wave[phase + 2U] = 0U;
        g_source0_dma_sck_wave[phase + 3U] = GPIO_BSHR_BR2;
#elif CH585_SPI_DMA_PHASES_PER_BIT == 3U
        g_source0_dma_sck_wave[phase + 0U] = GPIO_BSHR_BS2;
        g_source0_dma_sck_wave[phase + 1U] = 0U;
        g_source0_dma_sck_wave[phase + 2U] = GPIO_BSHR_BR2;
#elif CH585_SPI_DMA_PHASES_PER_BIT == 2U
        g_source0_dma_sck_wave[phase + 0U] = GPIO_BSHR_BS2;
        g_source0_dma_sck_wave[phase + 1U] = GPIO_BSHR_BR2;
#else
#error "Unsupported CH585_SPI_DMA_PHASES_PER_BIT"
#endif
    }
#endif
}

static int ch585_dma_soft_spi_init(void)
{
    TIM_TimeBaseInitTypeDef timer = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOD | RCC_HB2Periph_TIM8 | RCC_HB2Periph_AFIO, ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);

    ch585_dma_wave_prepare();
    g_scan.source0_dma_period = ch585_dma_period_from_target();

    TIM_Cmd(TIM8, DISABLE);
    TIM_DeInit(TIM8);
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_Period = g_scan.source0_dma_period;
    timer.TIM_Prescaler = 0U;
    timer.TIM_RepetitionCounter = 0U;
    TIM_TimeBaseInit(TIM8, &timer);
    TIM_DMACmd(TIM8, TIM_DMA_Update, ENABLE);

    DMA_MuxChannelConfig(DMA_MuxChannel1, CH585_SPI_DMA_TIM8_UP_REQ);
    DMA_MuxChannelConfig(DMA_MuxChannel2, CH585_SPI_DMA_TIM8_UP_REQ);

    GPIOD->BSHR = GPIO_BSHR_BR2;
    g_scan.source0_dma_ready = 1U;

    rt_kprintf("CH585 source0 DMA soft SPI: TIM8_UP req=%u phase_count=%u target=%u kHz period=%u sample_phase=%u hclk=%u core=%u\r\n",
               (unsigned int)CH585_SPI_DMA_TIM8_UP_REQ,
               (unsigned int)CH585_SPI_DMA_PHASE_COUNT,
               (unsigned int)APP_CH585_SPI_DMA_TARGET_KHZ,
               (unsigned int)g_scan.source0_dma_period,
               (unsigned int)CH585_SPI_DMA_SAMPLE_PHASE,
               (unsigned int)HCLKClock,
               (unsigned int)SystemCoreClock);
    return 0;
}
#endif

static uint8_t ch585_soft_spi_transfer_byte(uint8_t tx)
{
    uint8_t bit;
    uint8_t rx = 0U;

    for (bit = 0x80U; bit != 0U; bit >>= 1)
    {
        rt_pin_write(g_soft_spi.mosi, ((tx & bit) != 0U) ? PIN_HIGH : PIN_LOW);
        ch585_soft_spi_delay();

        rt_pin_write(g_soft_spi.sck, PIN_HIGH);
        ch585_soft_spi_delay();

#if APP_CH585_SPI_SAMPLE_TRAILING
        rt_pin_write(g_soft_spi.sck, PIN_LOW);
        ch585_soft_spi_delay();

        if (rt_pin_read(g_soft_spi.miso0) == PIN_HIGH)
        {
            rx |= bit;
        }
#else
        if (rt_pin_read(g_soft_spi.miso0) == PIN_HIGH)
        {
            rx |= bit;
        }

        rt_pin_write(g_soft_spi.sck, PIN_LOW);
        ch585_soft_spi_delay();
#endif
    }

    return rx;
}

static uint8_t ch585_bitstream_read_byte(const uint8_t *stream, uint32_t bit_start)
{
    uint8_t value = 0U;
    uint8_t i;

    for (i = 0; i < 8U; i++)
    {
        uint32_t bit_index = bit_start + i;
        uint8_t bit = (uint8_t)((stream[bit_index >> 3] >> (7U - (bit_index & 7U))) & 1U);
        value = (uint8_t)((value << 1) | bit);
    }

    return value;
}

static int ch585_scan_frame_is_valid(const ch585_scan_frame_v1_t *frame)
{
    uint16_t expected_crc;

    if (frame == RT_NULL)
    {
        return 0;
    }

#if APP_CH585_SPI_WIRE_SHORT
    if ((frame->magic != CH585_SCAN_SHORT_FRAME_MAGIC) ||
        (frame->type != CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE) ||
        (frame->source_id != 0U))
    {
        return 0;
    }
#else
    if ((frame->magic != CH585_SCAN_FRAME_MAGIC) ||
        (frame->version != CH585_SCAN_FRAME_VERSION) ||
        (frame->type != CH585_SCAN_FRAME_TYPE_KEY_STATE) ||
        (frame->source_id != 0U) ||
        (frame->key_count != CH585_SCAN_KEYS_PER_SOURCE))
    {
        return 0;
    }
#endif

    expected_crc = ch585_spi_scan_crc16((const uint8_t *)frame,
                                        (uint16_t)offsetof(ch585_scan_frame_v1_t, crc16));
    return (frame->crc16 == expected_crc) ? 1 : 0;
}

static int ch585_scan_find_frame_in_stream(const uint8_t *stream,
                                           uint16_t stream_len,
                                           ch585_scan_frame_v1_t *frame,
                                           uint16_t *byte_offset,
                                           uint8_t *bit_offset)
{
    ch585_scan_frame_v1_t candidate;
    uint8_t *candidate_bytes = (uint8_t *)&candidate;
    uint32_t total_bits;
    uint32_t frame_bits;
    uint32_t bit_start;
    uint16_t i;

    if ((stream == RT_NULL) || (frame == RT_NULL))
    {
        return -1;
    }

    total_bits = (uint32_t)stream_len * 8U;
    frame_bits = (uint32_t)sizeof(candidate) * 8U;
    if (total_bits < frame_bits)
    {
        return -1;
    }

    for (bit_start = 0U; bit_start + frame_bits <= total_bits; bit_start++)
    {
#if APP_CH585_SPI_WIRE_SHORT
        if ((ch585_bitstream_read_byte(stream, bit_start) !=
             (uint8_t)CH585_SCAN_SHORT_FRAME_MAGIC) ||
            (ch585_bitstream_read_byte(stream, bit_start + 8U) !=
             (uint8_t)CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE))
        {
            continue;
        }
#else
        if ((ch585_bitstream_read_byte(stream, bit_start) !=
             (uint8_t)(CH585_SCAN_FRAME_MAGIC & 0xFFU)) ||
            (ch585_bitstream_read_byte(stream, bit_start + 8U) !=
             (uint8_t)((CH585_SCAN_FRAME_MAGIC >> 8) & 0xFFU)))
        {
            continue;
        }
#endif

        for (i = 0U; i < sizeof(candidate); i++)
        {
            candidate_bytes[i] = ch585_bitstream_read_byte(stream, bit_start + ((uint32_t)i * 8U));
        }

        if (ch585_scan_frame_is_valid(&candidate) != 0)
        {
            memcpy(frame, &candidate, sizeof(*frame));
            if (byte_offset != RT_NULL)
            {
                *byte_offset = (uint16_t)(bit_start >> 3);
            }
            if (bit_offset != RT_NULL)
            {
                *bit_offset = (uint8_t)(bit_start & 7U);
            }
            return 0;
        }
    }

    return -1;
}

static int ch585_scan_try_first_bit_repair(const uint8_t *stream, ch585_scan_frame_v1_t *frame)
{
    uint8_t *frame_bytes = (uint8_t *)frame;
    uint16_t expected_crc;

    if ((stream == RT_NULL) || (frame == RT_NULL))
    {
        return -1;
    }

    memcpy(frame, stream, sizeof(*frame));

    if (ch585_scan_frame_is_valid(frame) != 0)
    {
        return 0;
    }

    /*
     * CH585 SPI0 can leave MISO high until the first clock after CS, so the
     * first sampled bit may be 1 while the rest of the frame is byte-aligned.
     * Keep this path for older low-MSB magic bytes; the current magic starts
     * with 0xd3 so a healthy frame should validate before reaching here.
     */
#if APP_CH585_SPI_WIRE_SHORT
    if (((frame_bytes[0] & 0x7FU) ==
         ((uint8_t)CH585_SCAN_SHORT_FRAME_MAGIC & 0x7FU)) &&
        (frame_bytes[1] == (uint8_t)CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE))
    {
        frame_bytes[0] = (uint8_t)CH585_SCAN_SHORT_FRAME_MAGIC;
        g_scan.source0_first_bit_repair_rx_crc = frame->crc16;
        expected_crc = ch585_spi_scan_crc16((const uint8_t *)frame,
                                            (uint16_t)offsetof(ch585_scan_frame_v1_t, crc16));
        g_scan.source0_first_bit_repair_expected_crc = expected_crc;
        if ((frame->type == CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE) &&
            (frame->source_id == 0U) &&
            (frame->crc16 == expected_crc))
        {
            g_scan.source0_first_bit_repairs++;
            g_scan.source0_sync_found = 2U;
            return 0;
        }
        g_scan.source0_first_bit_repair_crc_misses++;
    }
#else
    if (((frame_bytes[0] & 0x7FU) ==
         ((uint8_t)(CH585_SCAN_FRAME_MAGIC & 0xFFU) & 0x7FU)) &&
        (frame_bytes[1] == (uint8_t)((CH585_SCAN_FRAME_MAGIC >> 8) & 0xFFU)))
    {
        frame_bytes[0] = (uint8_t)(CH585_SCAN_FRAME_MAGIC & 0xFFU);
        g_scan.source0_first_bit_repair_rx_crc = frame->crc16;
        expected_crc = ch585_spi_scan_crc16((const uint8_t *)frame,
                                            (uint16_t)offsetof(ch585_scan_frame_v1_t, crc16));
        g_scan.source0_first_bit_repair_expected_crc = expected_crc;
        if ((frame->version == CH585_SCAN_FRAME_VERSION) &&
            (frame->type == CH585_SCAN_FRAME_TYPE_KEY_STATE) &&
            (frame->source_id == 0U) &&
            (frame->key_count == CH585_SCAN_KEYS_PER_SOURCE) &&
            (frame->crc16 == expected_crc))
        {
            g_scan.source0_first_bit_repairs++;
            g_scan.source0_sync_found = 2U;
            return 0;
        }
        g_scan.source0_first_bit_repair_crc_misses++;
    }
#endif

    return -1;
}

static void ch585_scan_snapshot_source0_capture(void)
{
    uint16_t i;

    for (i = 0U; i < sizeof(g_scan.source0_head); i++)
    {
        g_scan.source0_head[i] = g_source0_capture[i];
    }
    for (i = 0U; i < sizeof(g_scan.source0_req_rx); i++)
    {
        g_scan.source0_req_rx[i] = g_source0_capture[i];
    }
    for (i = 0U; i < sizeof(g_scan.source0_tail); i++)
    {
        g_scan.source0_tail[i] =
            g_source0_capture[sizeof(ch585_scan_frame_v1_t) - sizeof(g_scan.source0_tail) + i];
    }
}

static int ch585_scan_decode_source0_capture(ch585_scan_frame_v1_t *frame)
{
    uint8_t *rx = (uint8_t *)frame;
    uint16_t i;

    g_scan.source0_sync_found = 0U;
    g_scan.source0_sync_byte = 0U;
    g_scan.source0_sync_bit = 0U;

    ch585_scan_snapshot_source0_capture();

    if (ch585_scan_try_first_bit_repair(g_source0_capture, frame) == 0)
    {
        return 0;
    }

    if (ch585_scan_find_frame_in_stream(g_source0_capture,
                                        APP_CH585_SPI_SOURCE0_CAPTURE_BYTES,
                                        frame,
                                        &g_scan.source0_sync_byte,
                                        &g_scan.source0_sync_bit) == 0)
    {
        g_scan.source0_sync_found = 1U;
        return 0;
    }

    for (i = 0U; i < sizeof(*frame); i++)
    {
        rx[i] = g_source0_capture[i];
    }

    return 0;
}

static int ch585_scan_fetch_source0_soft_spi(ch585_scan_frame_v1_t *frame)
{
    uint32_t cycle_begin;
    uint32_t cycle_delta;
    uint32_t xfer_us;
    uint32_t bit_count;
    uint16_t i;

    if ((frame == RT_NULL) || (g_soft_spi.ready == 0U))
    {
        return -1;
    }

    g_scan.source0_miso_idle[0] = (uint8_t)rt_pin_read(g_soft_spi.miso0);

    /* One host request clocks exactly one CH585 frame. */
    rt_pin_write(g_soft_spi.cs0, PIN_LOW);
    if (APP_CH585_SPI_CS_SETUP_MS != 0U)
    {
        rt_thread_mdelay(APP_CH585_SPI_CS_SETUP_MS);
    }
    else
    {
        ch585_soft_spi_delay();
        ch585_soft_spi_delay();
    }

    cycle_begin = ch585_cycle_now();
    for (i = 0U; i < APP_CH585_SPI_SOURCE0_CAPTURE_BYTES; i++)
    {
        g_source0_capture[i] = ch585_soft_spi_transfer_byte(0xFFU);
    }
    cycle_delta = ch585_cycle_now() - cycle_begin;
    bit_count = (uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U;
    g_scan.source0_xfer_cycles = cycle_delta;
    if (SystemCoreClock != 0U)
    {
        xfer_us = (uint32_t)((((uint64_t)cycle_delta * 1000000ULL) +
                              ((uint64_t)SystemCoreClock / 2ULL)) /
                             (uint64_t)SystemCoreClock);
        if (xfer_us == 0U)
        {
            xfer_us = 1U;
        }
        g_scan.source0_xfer_us = xfer_us;
        g_scan.source0_sck_khz_x10 = (uint32_t)(((uint64_t)bit_count * 10000ULL) /
                                                (uint64_t)xfer_us);
        g_scan.source0_frame_fps_x10 = (uint32_t)(10000000ULL / (uint64_t)xfer_us);
    }
    else
    {
        g_scan.source0_xfer_us = 0U;
        g_scan.source0_sck_khz_x10 = 0U;
        g_scan.source0_frame_fps_x10 = 0U;
    }

    g_scan.source0_miso_idle[1] = (uint8_t)rt_pin_read(g_soft_spi.miso0);
    ch585_soft_spi_delay();
    rt_pin_write(g_soft_spi.cs0, PIN_HIGH);
    ch585_soft_spi_delay();
    g_scan.source0_miso_idle[2] = (uint8_t)rt_pin_read(g_soft_spi.miso0);

    g_scan.source0_backend = CH585_SPI_BACKEND_CPU_GPIO;
    return ch585_scan_decode_source0_capture(frame);
}

#if APP_CH585_SPI_HW_SPI2_BACKEND
static void ch585_hw_spi2_flush_rx(void)
{
    volatile uint16_t dummy;

    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) != RESET)
    {
        dummy = SPI_I2S_ReceiveData(SPI2);
        (void)dummy;
    }

    dummy = SPI2->STATR;
    dummy = SPI2->DATAR;
    (void)dummy;
}

static int ch585_hw_spi2_cmd_xfer(const ch585_scan_cmd_v1_t *cmd)
{
    const uint8_t *tx = (const uint8_t *)cmd;
    uint8_t *rx = (uint8_t *)&g_source0_spi2_cmd_rx;
    uint32_t timeout_cycles;
    uint32_t start;
    uint16_t i;

    if (cmd == RT_NULL)
    {
        return -1;
    }

    memset(&g_source0_spi2_cmd_rx, 0, sizeof(g_source0_spi2_cmd_rx));

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_Cmd(SPI2, DISABLE);
    ch585_hw_spi2_flush_rx();

    timeout_cycles = (SystemCoreClock != 0U) ? (SystemCoreClock / 100U) : 4000000U;
    SPI_Cmd(SPI2, ENABLE);
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
#endif

    for (i = 0U; i < (uint16_t)sizeof(ch585_scan_cmd_v1_t); i++)
    {
        start = ch585_cycle_now();
        while ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) &&
               ((uint32_t)(ch585_cycle_now() - start) < timeout_cycles))
        {
        }
        if (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
        {
            goto timeout;
        }

        SPI_I2S_SendData(SPI2, tx[i]);

        start = ch585_cycle_now();
        while ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) &&
               ((uint32_t)(ch585_cycle_now() - start) < timeout_cycles))
        {
        }
        if (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
        {
            goto timeout;
        }

        rx[i] = (uint8_t)SPI_I2S_ReceiveData(SPI2);
    }

    start = ch585_cycle_now();
    while ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET) &&
           ((uint32_t)(ch585_cycle_now() - start) < timeout_cycles))
    {
    }
    if (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET)
    {
        goto timeout;
    }

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    SPI_Cmd(SPI2, DISABLE);
    g_scan.source0_cmd_runs++;
    return 0;

timeout:
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    SPI_Cmd(SPI2, DISABLE);
    g_scan.source0_cmd_runs++;
    g_scan.source0_cmd_timeouts++;
    return -1;
}

static int ch585_hw_spi2_drain_xfer(uint16_t len)
{
    uint32_t timeout_cycles;
    uint32_t start;
    uint16_t i;

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_Cmd(SPI2, DISABLE);
    ch585_hw_spi2_flush_rx();

    timeout_cycles = (SystemCoreClock != 0U) ? (SystemCoreClock / 100U) : 4000000U;
    SPI_Cmd(SPI2, ENABLE);
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
#endif

    for (i = 0U; i < len; i++)
    {
        start = ch585_cycle_now();
        while ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) &&
               ((uint32_t)(ch585_cycle_now() - start) < timeout_cycles))
        {
        }
        if (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
        {
            goto timeout;
        }

        SPI_I2S_SendData(SPI2, 0xFFU);

        start = ch585_cycle_now();
        while ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) &&
               ((uint32_t)(ch585_cycle_now() - start) < timeout_cycles))
        {
        }
        if (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
        {
            goto timeout;
        }

        (void)SPI_I2S_ReceiveData(SPI2);
    }

    start = ch585_cycle_now();
    while ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET) &&
           ((uint32_t)(ch585_cycle_now() - start) < timeout_cycles))
    {
    }
    if (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET)
    {
        goto timeout;
    }

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    SPI_Cmd(SPI2, DISABLE);
    return 0;

timeout:
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    SPI_Cmd(SPI2, DISABLE);
    return -1;
}

static void ch585_hw_spi2_prepare_cmd(void)
{
    memset(&g_source0_spi2_cmd_tx, 0, sizeof(g_source0_spi2_cmd_tx));

    g_scan.source0_host_seq = (uint16_t)(g_scan.source0_host_seq + 1U);
#if APP_CH585_SPI_WIRE_SHORT
    g_source0_spi2_cmd_tx.magic = CH585_SCAN_SHORT_CMD_MAGIC;
    g_source0_spi2_cmd_tx.cmd = CH585_SCAN_CMD_GET_STATE;
    g_source0_spi2_cmd_tx.host_seq = (uint8_t)g_scan.source0_host_seq;
    g_source0_spi2_cmd_tx.ack_seq =
        (g_scan.source[0].have_seq != 0U) ? (uint8_t)g_scan.source[0].last_seq : 0xFFU;
    g_source0_spi2_cmd_tx.flags = 0U;
    g_source0_spi2_cmd_tx.reserved = 0U;
#else
    g_source0_spi2_cmd_tx.magic = CH585_SCAN_CMD_MAGIC;
    g_source0_spi2_cmd_tx.version = CH585_SCAN_FRAME_VERSION;
    g_source0_spi2_cmd_tx.cmd = CH585_SCAN_CMD_GET_STATE;
    g_source0_spi2_cmd_tx.host_seq = g_scan.source0_host_seq;
    g_source0_spi2_cmd_tx.ack_seq =
        (g_scan.source[0].have_seq != 0U) ? g_scan.source[0].last_seq : 0xFFFFU;
    g_source0_spi2_cmd_tx.flags = 0U;
#endif
    g_source0_spi2_cmd_tx.crc16 =
        ch585_spi_scan_crc16((const uint8_t *)&g_source0_spi2_cmd_tx,
                             (uint16_t)offsetof(ch585_scan_cmd_v1_t, crc16));
}

static void CH585_MAYBE_UNUSED ch585_hw_spi2_prepare_pipeline_tx(void)
{
    memset(g_source0_spi2_tx, 0xFF, sizeof(g_source0_spi2_tx));
    ch585_hw_spi2_prepare_cmd();
    memcpy(g_source0_spi2_tx, &g_source0_spi2_cmd_tx, sizeof(g_source0_spi2_cmd_tx));
}

static int ch585_hw_spi2_dma_frame_xfer(void)
{
    DMA_InitTypeDef dma_tx = {0};
    DMA_InitTypeDef dma_rx = {0};
    uint32_t cycle_begin;
    uint32_t cycle_delta;
    uint32_t timeout_cycles;
    uint32_t xfer_us;
    uint32_t bit_count;
    uint32_t flags;

    memset(g_source0_spi2_rx, 0x00, sizeof(g_source0_spi2_rx));

    g_scan.source0_miso_idle[0] =
        ((GPIOC->INDR & GPIO_Pin_2) != 0U) ? 1U : 0U;

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif

    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_Cmd(SPI2, DISABLE);
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL2 | DMA1_FLAG_GL3);
    ch585_hw_spi2_flush_rx();

    dma_tx.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    dma_tx.DMA_Memory0BaseAddr = (uint32_t)g_source0_spi2_tx;
    dma_tx.DMA_DIR = DMA_DIR_PeripheralDST;
    dma_tx.DMA_BufferSize = APP_CH585_SPI_SOURCE0_CAPTURE_BYTES;
    dma_tx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_tx.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_tx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_tx.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma_tx.DMA_Mode = DMA_Mode_Normal;
    dma_tx.DMA_Priority = DMA_Priority_VeryHigh;
    dma_tx.DMA_M2M = DMA_M2M_Disable;

    dma_rx.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    dma_rx.DMA_Memory0BaseAddr = (uint32_t)g_source0_spi2_rx;
    dma_rx.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma_rx.DMA_BufferSize = APP_CH585_SPI_SOURCE0_CAPTURE_BYTES;
    dma_rx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_rx.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_rx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_rx.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma_rx.DMA_Mode = DMA_Mode_Normal;
    dma_rx.DMA_Priority = DMA_Priority_VeryHigh;
    dma_rx.DMA_M2M = DMA_M2M_Disable;

    DMA_DeInit(DMA1_Channel2);
    DMA_DeInit(DMA1_Channel3);
    DMA_Init(DMA1_Channel2, &dma_rx);
    DMA_Init(DMA1_Channel3, &dma_tx);
    DMA_MuxChannelConfig(DMA_MuxChannel2, CH585_SPI2_RX_DMA_REQ);
    DMA_MuxChannelConfig(DMA_MuxChannel3, CH585_SPI2_TX_DMA_REQ);
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL2 | DMA1_FLAG_GL3);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
#endif
    SPI_Cmd(SPI2, ENABLE);

    if (APP_CH585_SPI_CS_SETUP_MS != 0U)
    {
        rt_thread_mdelay(APP_CH585_SPI_CS_SETUP_MS);
    }
    else
    {
        ch585_soft_spi_delay();
        ch585_soft_spi_delay();
    }

    cycle_begin = ch585_cycle_now();
    DMA_Cmd(DMA1_Channel2, ENABLE);
    DMA_Cmd(DMA1_Channel3, ENABLE);

    timeout_cycles = (SystemCoreClock != 0U) ? (SystemCoreClock / 5U) : 80000000U;
    while ((((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC2) == RESET) ||
             (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) == RESET)) ||
            (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET)) &&
           ((uint32_t)(ch585_cycle_now() - cycle_begin) < timeout_cycles))
    {
    }

    cycle_delta = ch585_cycle_now() - cycle_begin;
    flags = DMA1->INTFR;

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_Cmd(DMA1_Channel2, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    g_scan.source0_spi2_sr_last = SPI2->STATR;
    g_scan.source0_miso_idle[1] =
        ((GPIOC->INDR & GPIO_Pin_2) != 0U) ? 1U : 0U;
    SPI_Cmd(SPI2, DISABLE);
    ch585_soft_spi_delay();
    g_scan.source0_miso_idle[2] =
        ((GPIOC->INDR & GPIO_Pin_2) != 0U) ? 1U : 0U;

    g_scan.source0_spi2_runs++;
    if (((flags & DMA1_FLAG_TC2) == 0U) || ((flags & DMA1_FLAG_TC3) == 0U))
    {
        g_scan.source0_spi2_timeouts++;
    }
    if ((flags & (DMA1_FLAG_TE2 | DMA1_FLAG_TE3)) != 0U)
    {
        g_scan.source0_spi2_te_flags++;
    }

    memcpy(g_source0_capture, g_source0_spi2_rx, sizeof(g_source0_capture));

    bit_count = (uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U;
    g_scan.source0_xfer_cycles = cycle_delta;
    if (SystemCoreClock != 0U)
    {
        xfer_us = (uint32_t)((((uint64_t)cycle_delta * 1000000ULL) +
                              ((uint64_t)SystemCoreClock / 2ULL)) /
                             (uint64_t)SystemCoreClock);
        if (xfer_us == 0U)
        {
            xfer_us = 1U;
        }
        g_scan.source0_xfer_us = xfer_us;
        g_scan.source0_sck_khz_x10 = (uint32_t)(((uint64_t)bit_count * 10000ULL) /
                                                (uint64_t)xfer_us);
        g_scan.source0_frame_fps_x10 = (uint32_t)(10000000ULL / (uint64_t)xfer_us);
    }
    else
    {
        g_scan.source0_xfer_us = 0U;
        g_scan.source0_sck_khz_x10 = ch585_hw_spi2_expected_sck_khz_x10();
        g_scan.source0_frame_fps_x10 = 0U;
    }

    g_scan.source0_backend = CH585_SPI_BACKEND_HW_SPI2_HARDNSS;

    return (((flags & DMA1_FLAG_TC2) != 0U) && ((flags & DMA1_FLAG_TC3) != 0U)) ? 0 : -1;
}

#if APP_CH585_SPI_AUTO_TRAIN && APP_CH585_SPI_WIRE_SHORT && APP_CH585_SPI_REQUEST_ONLY_SHORT
typedef struct
{
    uint16_t prescaler;
    uint8_t hsrx;
    uint16_t cpha;
    const char *name;
} ch585_spi_train_candidate_t;

static int ch585_hw_spi2_train_try(const ch585_spi_train_candidate_t *candidate,
                                   uint16_t *bad_out,
                                   uint16_t *seq_out)
{
    ch585_scan_frame_v1_t frame;
    uint16_t bad_errors = 0U;
    uint16_t seq_errors = 0U;
    uint8_t have_seq = 0U;
    uint8_t last_seq = 0U;
    uint8_t i;
    uint8_t warmup;

    if ((candidate == RT_NULL) || (bad_out == RT_NULL) || (seq_out == RT_NULL))
    {
        return -1;
    }

    ch585_hw_spi2_apply_config(candidate->prescaler, candidate->hsrx, candidate->cpha);

    ch585_cycle_delay_us(APP_CH585_SPI_RESYNC_TO_CMD_US);

    for (warmup = 0U; warmup < (uint8_t)APP_CH585_SPI_TRAIN_WARMUP_FRAMES; warmup++)
    {
        if (ch585_hw_spi2_drain_xfer((uint16_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES) != 0)
        {
            ch585_cycle_delay_us(APP_CH585_SPI_RESYNC_TO_CMD_US);
        }
        ch585_cycle_delay_us(APP_CH585_SPI_TRAIN_INTERFRAME_US);
    }

    for (i = 0U; i < APP_CH585_SPI_TRAIN_FRAMES; i++)
    {
        memset(g_source0_spi2_tx, 0xFF, sizeof(g_source0_spi2_tx));
        if (ch585_hw_spi2_dma_frame_xfer() != 0)
        {
            bad_errors++;
            ch585_cycle_delay_us(APP_CH585_SPI_TRAIN_INTERFRAME_US);
            continue;
        }

        if (ch585_scan_decode_source0_capture(&frame) != 0)
        {
            bad_errors++;
            ch585_cycle_delay_us(APP_CH585_SPI_TRAIN_INTERFRAME_US);
            continue;
        }

        if (ch585_scan_frame_is_valid(&frame) == 0)
        {
            bad_errors++;
            ch585_cycle_delay_us(APP_CH585_SPI_TRAIN_INTERFRAME_US);
            continue;
        }

        if ((have_seq != 0U) && (frame.seq != (uint8_t)(last_seq + 1U)))
        {
            seq_errors++;
        }
        last_seq = frame.seq;
        have_seq = 1U;
        ch585_cycle_delay_us(APP_CH585_SPI_TRAIN_INTERFRAME_US);
    }

    *bad_out = bad_errors;
    *seq_out = seq_errors;
    rt_kprintf("CH585 SPI train: %s prescaler=0x%04x hsrx=%u cpha=%u expect=%u.%u kHz bad=%u/%u seq=%u warmup=%u gap=%u us\r\n",
               candidate->name,
               (unsigned int)candidate->prescaler,
               (unsigned int)candidate->hsrx,
               (unsigned int)((candidate->cpha == SPI_CPHA_2Edge) ? 2U : 1U),
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() / 10U),
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() % 10U),
               (unsigned int)bad_errors,
               (unsigned int)APP_CH585_SPI_TRAIN_FRAMES,
               (unsigned int)seq_errors,
               (unsigned int)APP_CH585_SPI_TRAIN_WARMUP_FRAMES,
               (unsigned int)APP_CH585_SPI_TRAIN_INTERFRAME_US);

    return 0;
}

static int ch585_hw_spi2_train(void)
{
    static const ch585_spi_train_candidate_t candidates[] = {
        { SPI_BaudRatePrescaler_Mode0, 2U, SPI_CPHA_1Edge, "mode0-hsrx2-cpha1" },
        { SPI_BaudRatePrescaler_Mode0, 1U, SPI_CPHA_1Edge, "mode0-hsrx1-cpha1" },
        { SPI_BaudRatePrescaler_Mode1, 2U, SPI_CPHA_1Edge, "mode1-hsrx2-cpha1" },
        { SPI_BaudRatePrescaler_Mode1, 1U, SPI_CPHA_1Edge, "mode1-hsrx1-cpha1" },
        { SPI_BaudRatePrescaler_Mode2, 2U, SPI_CPHA_1Edge, "mode2-hsrx2-cpha1" },
        { SPI_BaudRatePrescaler_Mode2, 1U, SPI_CPHA_1Edge, "mode2-hsrx1-cpha1" },
        { SPI_BaudRatePrescaler_Mode2, 1U, SPI_CPHA_2Edge, "mode2-hsrx1-cpha2" },
        { SPI_BaudRatePrescaler_Mode3, 2U, SPI_CPHA_1Edge, "mode3-hsrx2-cpha1" },
        { SPI_BaudRatePrescaler_Mode3, 1U, SPI_CPHA_2Edge, "mode3-hsrx1-cpha2" },
        { SPI_BaudRatePrescaler_Mode3, 1U, SPI_CPHA_1Edge, "mode3-hsrx1-cpha1" },
        { SPI_BaudRatePrescaler_Mode4, 2U, SPI_CPHA_1Edge, "mode4-hsrx2-cpha1" },
        { SPI_BaudRatePrescaler_Mode4, 1U, SPI_CPHA_1Edge, "mode4-hsrx1-cpha1" },
        { SPI_BaudRatePrescaler_Mode5, 1U, SPI_CPHA_1Edge, "mode5-hsrx1-cpha1" },
    };
    const ch585_spi_train_candidate_t *selected = RT_NULL;
    const ch585_spi_train_candidate_t *best_seen = &candidates[sizeof(candidates) / sizeof(candidates[0]) - 1U];
    uint16_t best_errors = 0xFFFFU;
    uint16_t best_bad_errors = 0xFFFFU;
    uint16_t bad_errors;
    uint16_t seq_errors;
    uint16_t total_errors;
    uint8_t i;

    rt_kprintf("CH585 SPI train: start candidates=%u frames=%u\r\n",
               (unsigned int)(sizeof(candidates) / sizeof(candidates[0])),
               (unsigned int)APP_CH585_SPI_TRAIN_FRAMES);
    g_scan.source0_train_candidate_count = 0U;

    for (i = 0U; i < (uint8_t)(sizeof(candidates) / sizeof(candidates[0])); i++)
    {
        if (ch585_hw_spi2_train_try(&candidates[i], &bad_errors, &seq_errors) != 0)
        {
            bad_errors = 0xFFFFU;
            seq_errors = 0xFFFFU;
        }

        if (i < APP_CH585_SPI_TRAIN_MAX_CANDIDATES)
        {
            g_scan.source0_train_candidate_prescaler[i] = candidates[i].prescaler;
            g_scan.source0_train_candidate_hsrx[i] = candidates[i].hsrx;
            g_scan.source0_train_candidate_cpha[i] =
                (uint8_t)((candidates[i].cpha == SPI_CPHA_2Edge) ? 2U : 1U);
            g_scan.source0_train_candidate_bad[i] = bad_errors;
            g_scan.source0_train_candidate_seq[i] = seq_errors;
            g_scan.source0_train_candidate_count = (uint8_t)(i + 1U);
        }

        if ((bad_errors == 0xFFFFU) || (seq_errors == 0xFFFFU))
        {
            total_errors = 0xFFFFU;
        }
        else
        {
            total_errors = (uint16_t)(bad_errors + seq_errors);
        }

        if ((total_errors < best_errors) ||
            ((total_errors == best_errors) && (bad_errors < best_bad_errors)))
        {
            best_errors = total_errors;
            best_bad_errors = bad_errors;
            best_seen = &candidates[i];
        }

        if ((bad_errors == 0U) && (seq_errors == 0U))
        {
            selected = &candidates[i];
            break;
        }
    }

    if (selected == RT_NULL)
    {
        selected = best_seen;
    }

    ch585_hw_spi2_apply_config(selected->prescaler, selected->hsrx, selected->cpha);
    g_scan.source0_train_errors = best_errors;
    g_scan.source0_train_frames = APP_CH585_SPI_TRAIN_FRAMES;
    g_scan.source0_need_resync = 1U;
    g_scan.source0_pipeline_primed = 0U;
    g_scan.source0_spi2_runs = 0U;
    g_scan.source0_spi2_timeouts = 0U;
    g_scan.source0_spi2_te_flags = 0U;
    g_scan.source[0].have_seq = 0U;
    g_scan.source0_train_done = 1U;

    rt_kprintf("CH585 SPI train: selected %s prescaler=0x%04x hsrx=%u cpha=%u expect=%u.%u kHz errors=%u/%u\r\n",
               selected->name,
               (unsigned int)selected->prescaler,
               (unsigned int)selected->hsrx,
               (unsigned int)((selected->cpha == SPI_CPHA_2Edge) ? 2U : 1U),
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() / 10U),
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() % 10U),
               (unsigned int)best_errors,
               (unsigned int)APP_CH585_SPI_TRAIN_FRAMES);

    return (best_errors == 0U) ? 0 : -1;
}
#else
static int ch585_hw_spi2_train(void)
{
    return 0;
}
#endif

#if APP_CH585_SPI_PIPELINE_SHORT && APP_CH585_SPI_WIRE_SHORT
static int ch585_scan_fetch_source0_hw_spi2_pipeline(ch585_scan_frame_v1_t *frame)
{
    uint16_t expected_ack;

    if ((frame == RT_NULL) || (g_scan.source0_spi2_ready == 0U))
    {
        return -1;
    }

    if ((g_scan.source0_need_resync != 0U) || (g_scan.source0_pipeline_primed == 0U))
    {
        g_scan.source0_resync_runs++;
        if (ch585_hw_spi2_drain_xfer((uint16_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES) != 0)
        {
            return -1;
        }
        ch585_cycle_delay_us(APP_CH585_SPI_RESYNC_TO_CMD_US);

        ch585_hw_spi2_prepare_pipeline_tx();
        if (ch585_hw_spi2_dma_frame_xfer() != 0)
        {
            g_scan.source0_need_resync = 1U;
            g_scan.source0_pipeline_primed = 0U;
            return -1;
        }

        g_scan.source0_pipeline_ack_seq = g_scan.source0_host_seq;
        g_scan.source0_pipeline_primed = 1U;
        g_scan.source0_need_resync = 0U;
    }

    ch585_cycle_delay_us(APP_CH585_SPI_CMD_TO_DATA_US);
    expected_ack = g_scan.source0_pipeline_ack_seq;
    ch585_hw_spi2_prepare_pipeline_tx();
    if (ch585_hw_spi2_dma_frame_xfer() != 0)
    {
        g_scan.source0_need_resync = 1U;
        g_scan.source0_pipeline_primed = 0U;
        return -1;
    }

    g_scan.source0_accept_ack_seq = expected_ack;
    g_scan.source0_pipeline_ack_seq = g_scan.source0_host_seq;

    return ch585_scan_decode_source0_capture(frame);
}
#endif

#if APP_CH585_SPI_REQUEST_ONLY_SHORT && APP_CH585_SPI_WIRE_SHORT
static int ch585_scan_fetch_source0_hw_spi2_request_only(ch585_scan_frame_v1_t *frame)
{
    if ((frame == RT_NULL) || (g_scan.source0_spi2_ready == 0U))
    {
        return -1;
    }

    if (g_scan.source0_need_resync != 0U)
    {
        g_scan.source0_resync_runs++;
        if (ch585_hw_spi2_drain_xfer((uint16_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES) != 0)
        {
            return -1;
        }
        g_scan.source0_need_resync = 0U;
        ch585_cycle_delay_us(APP_CH585_SPI_RESYNC_TO_CMD_US);
    }

    memset(g_source0_spi2_tx, 0xFF, sizeof(g_source0_spi2_tx));
    if (ch585_hw_spi2_dma_frame_xfer() != 0)
    {
        g_scan.source0_need_resync = 1U;
        return -1;
    }

    g_scan.source0_accept_ack_seq = 0xFFFFU;
    return ch585_scan_decode_source0_capture(frame);
}
#endif

static int CH585_MAYBE_UNUSED ch585_scan_fetch_source0_hw_spi2(ch585_scan_frame_v1_t *frame)
{
    DMA_InitTypeDef dma_tx = {0};
    DMA_InitTypeDef dma_rx = {0};
    uint32_t cycle_begin;
    uint32_t cycle_delta;
    uint32_t timeout_cycles;
    uint32_t xfer_us;
    uint32_t bit_count;
    uint32_t flags;

    if ((frame == RT_NULL) || (g_scan.source0_spi2_ready == 0U))
    {
        return -1;
    }

    if ((g_scan.source0_need_resync != 0U) ||
        (APP_CH585_SPI_RESYNC_EVERY_POLL != 0U))
    {
        g_scan.source0_resync_runs++;
        if (ch585_hw_spi2_drain_xfer((uint16_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES) != 0)
        {
            return -1;
        }
        g_scan.source0_need_resync = 0U;
#if APP_CH585_SPI_CMD_TO_DATA_MS
        rt_thread_mdelay(APP_CH585_SPI_CMD_TO_DATA_MS);
#endif
        ch585_cycle_delay_us(APP_CH585_SPI_RESYNC_TO_CMD_US);
    }

    ch585_hw_spi2_prepare_cmd();
    if (ch585_hw_spi2_cmd_xfer(&g_source0_spi2_cmd_tx) != 0)
    {
        g_scan.source0_need_resync = 1U;
        return -1;
    }
    g_scan.source0_accept_ack_seq = g_scan.source0_host_seq;
#if APP_CH585_SPI_CMD_TO_DATA_MS
    rt_thread_mdelay(APP_CH585_SPI_CMD_TO_DATA_MS);
#endif
    ch585_cycle_delay_us(APP_CH585_SPI_CMD_TO_DATA_US);

    memset(g_source0_spi2_tx, 0xFF, sizeof(g_source0_spi2_tx));
    memset(g_source0_spi2_rx, 0x00, sizeof(g_source0_spi2_rx));

    g_scan.source0_miso_idle[0] =
        ((GPIOC->INDR & GPIO_Pin_2) != 0U) ? 1U : 0U;

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif

    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_Cmd(SPI2, DISABLE);
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL2 | DMA1_FLAG_GL3);
    ch585_hw_spi2_flush_rx();

    dma_tx.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    dma_tx.DMA_Memory0BaseAddr = (uint32_t)g_source0_spi2_tx;
    dma_tx.DMA_DIR = DMA_DIR_PeripheralDST;
    dma_tx.DMA_BufferSize = APP_CH585_SPI_SOURCE0_CAPTURE_BYTES;
    dma_tx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_tx.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_tx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_tx.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma_tx.DMA_Mode = DMA_Mode_Normal;
    dma_tx.DMA_Priority = DMA_Priority_VeryHigh;
    dma_tx.DMA_M2M = DMA_M2M_Disable;

    dma_rx.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    dma_rx.DMA_Memory0BaseAddr = (uint32_t)g_source0_spi2_rx;
    dma_rx.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma_rx.DMA_BufferSize = APP_CH585_SPI_SOURCE0_CAPTURE_BYTES;
    dma_rx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_rx.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_rx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_rx.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma_rx.DMA_Mode = DMA_Mode_Normal;
    dma_rx.DMA_Priority = DMA_Priority_VeryHigh;
    dma_rx.DMA_M2M = DMA_M2M_Disable;

    DMA_DeInit(DMA1_Channel2);
    DMA_DeInit(DMA1_Channel3);
    DMA_Init(DMA1_Channel2, &dma_rx);
    DMA_Init(DMA1_Channel3, &dma_tx);
    DMA_MuxChannelConfig(DMA_MuxChannel2, CH585_SPI2_RX_DMA_REQ);
    DMA_MuxChannelConfig(DMA_MuxChannel3, CH585_SPI2_TX_DMA_REQ);
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL2 | DMA1_FLAG_GL3);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
#endif
    SPI_Cmd(SPI2, ENABLE);

    if (APP_CH585_SPI_CS_SETUP_MS != 0U)
    {
        rt_thread_mdelay(APP_CH585_SPI_CS_SETUP_MS);
    }
    else
    {
        ch585_soft_spi_delay();
        ch585_soft_spi_delay();
    }

    cycle_begin = ch585_cycle_now();
    DMA_Cmd(DMA1_Channel2, ENABLE);
    DMA_Cmd(DMA1_Channel3, ENABLE);

    timeout_cycles = (SystemCoreClock != 0U) ? (SystemCoreClock / 5U) : 80000000U;
    while ((((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC2) == RESET) ||
             (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) == RESET)) ||
            (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET)) &&
           ((uint32_t)(ch585_cycle_now() - cycle_begin) < timeout_cycles))
    {
    }

    cycle_delta = ch585_cycle_now() - cycle_begin;
    flags = DMA1->INTFR;

#if APP_CH585_SPI_HW_SPI2_GPIO_CS
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
#endif
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_Cmd(DMA1_Channel2, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    g_scan.source0_spi2_sr_last = SPI2->STATR;
    g_scan.source0_miso_idle[1] =
        ((GPIOC->INDR & GPIO_Pin_2) != 0U) ? 1U : 0U;
    SPI_Cmd(SPI2, DISABLE);
    ch585_soft_spi_delay();
    g_scan.source0_miso_idle[2] =
        ((GPIOC->INDR & GPIO_Pin_2) != 0U) ? 1U : 0U;

    g_scan.source0_spi2_runs++;
    if (((flags & DMA1_FLAG_TC2) == 0U) || ((flags & DMA1_FLAG_TC3) == 0U))
    {
        g_scan.source0_spi2_timeouts++;
    }
    if ((flags & (DMA1_FLAG_TE2 | DMA1_FLAG_TE3)) != 0U)
    {
        g_scan.source0_spi2_te_flags++;
    }

    memcpy(g_source0_capture, g_source0_spi2_rx, sizeof(g_source0_capture));

    bit_count = (uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U;
    g_scan.source0_xfer_cycles = cycle_delta;
    if (SystemCoreClock != 0U)
    {
        xfer_us = (uint32_t)((((uint64_t)cycle_delta * 1000000ULL) +
                              ((uint64_t)SystemCoreClock / 2ULL)) /
                             (uint64_t)SystemCoreClock);
        if (xfer_us == 0U)
        {
            xfer_us = 1U;
        }
        g_scan.source0_xfer_us = xfer_us;
        g_scan.source0_sck_khz_x10 = (uint32_t)(((uint64_t)bit_count * 10000ULL) /
                                                (uint64_t)xfer_us);
        g_scan.source0_frame_fps_x10 = (uint32_t)(10000000ULL / (uint64_t)xfer_us);
    }
    else
    {
        g_scan.source0_xfer_us = 0U;
        g_scan.source0_sck_khz_x10 = ch585_hw_spi2_expected_sck_khz_x10();
        g_scan.source0_frame_fps_x10 = 0U;
    }

    g_scan.source0_backend = CH585_SPI_BACKEND_HW_SPI2_HARDNSS;
    return ch585_scan_decode_source0_capture(frame);
}
#endif

#if APP_CH585_SPI_DMA_BACKEND
static void ch585_dma_pack_source0_capture(void)
{
    uint32_t bit;
    uint32_t byte_index;
    uint8_t bit_mask;
    uint16_t sample;
    uint16_t sample_or = 0U;
    uint16_t sample_and = 0xFFFFU;

    memset(g_source0_capture, 0, sizeof(g_source0_capture));

    for (bit = 0U; bit < ((uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U); bit++)
    {
        sample = g_source0_dma_miso_sample[(bit * CH585_SPI_DMA_PHASES_PER_BIT) +
                                           CH585_SPI_DMA_SAMPLE_PHASE];
        sample_or |= sample;
        sample_and &= sample;
        if ((sample & GPIO_Pin_4) != 0U)
        {
            byte_index = bit >> 3;
            bit_mask = (uint8_t)(0x80U >> (bit & 7U));
            g_source0_capture[byte_index] |= bit_mask;
        }
    }

    g_scan.source0_dma_sample_or = sample_or;
    g_scan.source0_dma_sample_and = sample_and;
}

static int ch585_scan_fetch_source0_dma_spi(ch585_scan_frame_v1_t *frame)
{
    DMA_InitTypeDef dma_out = {0};
    DMA_InitTypeDef dma_in = {0};
    uint32_t cycle_begin;
    uint32_t cycle_delta;
    uint32_t timeout_cycles;
    uint32_t xfer_us;
    uint32_t bit_count;
    uint32_t flags;
    uint32_t i;

    if ((frame == RT_NULL) || (g_soft_spi.ready == 0U) || (g_scan.source0_dma_ready == 0U))
    {
        return -1;
    }

    g_scan.source0_miso_idle[0] = (uint8_t)rt_pin_read(g_soft_spi.miso0);

    for (i = 0U; i < CH585_SPI_DMA_PHASE_COUNT; i++)
    {
        g_source0_dma_miso_sample[i] = 0U;
    }

    DMA_Cmd(DMA1_Channel1, DISABLE);
    DMA_Cmd(DMA1_Channel2, DISABLE);
    TIM_Cmd(TIM8, DISABLE);
    TIM_DMACmd(TIM8, TIM_DMA_Update, DISABLE);
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL1 | DMA1_FLAG_GL2);

    dma_out.DMA_PeripheralBaseAddr =
#if APP_CH585_SPI_DMA_USE_OUTDR
        (uint32_t)&GPIOD->OUTDR;
#else
        (uint32_t)&GPIOD->BSHR;
#endif
    dma_out.DMA_Memory0BaseAddr = (uint32_t)g_source0_dma_sck_wave;
    dma_out.DMA_DIR = DMA_DIR_PeripheralDST;
    dma_out.DMA_BufferSize = CH585_SPI_DMA_PHASE_COUNT;
    dma_out.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_out.DMA_MemoryInc = DMA_MemoryInc_Enable;
#if APP_CH585_SPI_DMA_USE_OUTDR
    dma_out.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma_out.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
#else
    dma_out.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    dma_out.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
#endif
    dma_out.DMA_Mode = DMA_Mode_Normal;
    dma_out.DMA_Priority = DMA_Priority_VeryHigh;
    dma_out.DMA_M2M = DMA_M2M_Disable;

    dma_in.DMA_PeripheralBaseAddr = (uint32_t)&GPIOD->INDR;
    dma_in.DMA_Memory0BaseAddr = (uint32_t)g_source0_dma_miso_sample;
    dma_in.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma_in.DMA_BufferSize = CH585_SPI_DMA_PHASE_COUNT;
    dma_in.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_in.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_in.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma_in.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma_in.DMA_Mode = DMA_Mode_Normal;
    dma_in.DMA_Priority = DMA_Priority_High;
    dma_in.DMA_M2M = DMA_M2M_Disable;

    DMA_DeInit(DMA1_Channel1);
    DMA_DeInit(DMA1_Channel2);
    DMA_Init(DMA1_Channel1, &dma_out);
    DMA_Init(DMA1_Channel2, &dma_in);
    DMA_MuxChannelConfig(DMA_MuxChannel1, CH585_SPI_DMA_TIM8_UP_REQ);
    DMA_MuxChannelConfig(DMA_MuxChannel2, CH585_SPI_DMA_TIM8_UP_REQ);

    GPIOD->BSHR = GPIO_BSHR_BR2;
    rt_pin_write(g_soft_spi.mosi, PIN_HIGH);
    rt_pin_write(g_soft_spi.cs0, PIN_LOW);
    if (APP_CH585_SPI_CS_SETUP_MS != 0U)
    {
        rt_thread_mdelay(APP_CH585_SPI_CS_SETUP_MS);
    }
    else
    {
        ch585_soft_spi_delay();
        ch585_soft_spi_delay();
    }
    ch585_dma_wave_prepare();

    TIM8->CNT = 0U;
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL1 | DMA1_FLAG_GL2);
    TIM_DMACmd(TIM8, TIM_DMA_Update, ENABLE);
    cycle_begin = ch585_cycle_now();
    DMA_Cmd(DMA1_Channel1, ENABLE);
    DMA_Cmd(DMA1_Channel2, ENABLE);
    TIM_Cmd(TIM8, ENABLE);

    timeout_cycles = (SystemCoreClock != 0U) ? (SystemCoreClock / 5U) : 80000000U;
    while (((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC1) == RESET) ||
            (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC2) == RESET)) &&
           ((uint32_t)(ch585_cycle_now() - cycle_begin) < timeout_cycles))
    {
    }

    cycle_delta = ch585_cycle_now() - cycle_begin;
    TIM_Cmd(TIM8, DISABLE);
    TIM_DMACmd(TIM8, TIM_DMA_Update, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    DMA_Cmd(DMA1_Channel2, DISABLE);
    flags = DMA1->INTFR;
    GPIOD->BSHR = GPIO_BSHR_BR2;

    g_scan.source0_miso_idle[1] = (uint8_t)rt_pin_read(g_soft_spi.miso0);
    rt_pin_write(g_soft_spi.cs0, PIN_HIGH);
    ch585_soft_spi_delay();
    g_scan.source0_miso_idle[2] = (uint8_t)rt_pin_read(g_soft_spi.miso0);

    g_scan.source0_dma_runs++;
    if (((flags & DMA1_FLAG_TC1) == 0U) || ((flags & DMA1_FLAG_TC2) == 0U))
    {
        g_scan.source0_dma_timeouts++;
    }
    if ((flags & (DMA1_FLAG_TE1 | DMA1_FLAG_TE2)) != 0U)
    {
        g_scan.source0_dma_te_flags++;
    }

    bit_count = (uint32_t)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U;
    g_scan.source0_xfer_cycles = cycle_delta;
    if (SystemCoreClock != 0U)
    {
        xfer_us = (uint32_t)((((uint64_t)cycle_delta * 1000000ULL) +
                              ((uint64_t)SystemCoreClock / 2ULL)) /
                             (uint64_t)SystemCoreClock);
        if (xfer_us == 0U)
        {
            xfer_us = 1U;
        }
        g_scan.source0_xfer_us = xfer_us;
        g_scan.source0_sck_khz_x10 = (uint32_t)(((uint64_t)bit_count * 10000ULL) /
                                                (uint64_t)xfer_us);
        g_scan.source0_frame_fps_x10 = (uint32_t)(10000000ULL / (uint64_t)xfer_us);
    }
    else
    {
        g_scan.source0_xfer_us = 0U;
        g_scan.source0_sck_khz_x10 = 0U;
        g_scan.source0_frame_fps_x10 = 0U;
    }

    ch585_dma_pack_source0_capture();
    g_scan.source0_backend = CH585_SPI_BACKEND_DMA_GPIO;
    return ch585_scan_decode_source0_capture(frame);
}
#endif

static int ch585_scan_fetch_real_or_fake(uint8_t source_id, ch585_scan_frame_v1_t *frame)
{
#if APP_CH585_SPI_REAL_SOURCE0
    if (source_id == 0U)
    {
#if APP_CH585_SPI_HW_SPI2_BACKEND
#if APP_CH585_SPI_REQUEST_ONLY_SHORT && APP_CH585_SPI_WIRE_SHORT
        if (ch585_scan_fetch_source0_hw_spi2_request_only(frame) == 0)
        {
            return 0;
        }
#elif APP_CH585_SPI_PIPELINE_SHORT && APP_CH585_SPI_WIRE_SHORT
        if (ch585_scan_fetch_source0_hw_spi2_pipeline(frame) == 0)
        {
            return 0;
        }
#else
        if (ch585_scan_fetch_source0_hw_spi2(frame) == 0)
        {
            return 0;
        }
#endif
#endif
#if APP_CH585_SPI_DMA_BACKEND
        if (ch585_scan_fetch_source0_dma_spi(frame) == 0)
        {
            return 0;
        }
        return ch585_scan_fetch_source0_soft_spi(frame);
#else
        return ch585_scan_fetch_source0_soft_spi(frame);
#endif
    }
#endif

#if APP_CH585_SPI_FAKE_SOURCE1
    if (source_id == 1U)
    {
        return ch585_scan_fetch_source(source_id, frame);
    }
#endif

    return ch585_scan_fetch_source(source_id, frame);
}

static void ch585_scan_record_flags(ch585_scan_source_stats_t *stats, uint16_t flags)
{
    if ((flags & CH585_SCAN_FLAG_OVERRUN) != 0U)
    {
        stats->flag_overrun++;
    }
    if ((flags & CH585_SCAN_FLAG_ADC_ERROR) != 0U)
    {
        stats->flag_adc_error++;
    }
    if ((flags & CH585_SCAN_FLAG_STALE) != 0U)
    {
        stats->flag_stale++;
    }
    if ((flags & CH585_SCAN_FLAG_SYNC_LOST) != 0U)
    {
        stats->flag_sync_lost++;
    }
}

static void ch585_scan_mark_resync_if_source0(uint8_t source_id)
{
    if (source_id == 0U)
    {
        g_scan.source0_need_resync = 1U;
        g_scan.source0_pipeline_primed = 0U;
    }
}

static int ch585_scan_accept_frame(uint8_t expected_source, const ch585_scan_frame_v1_t *frame)
{
    ch585_scan_source_stats_t *stats;
    uint16_t expected_crc;
    uint16_t expected_base;
    uint16_t expected_seq;
    uint16_t frame_seq;
    uint16_t frame_flags;
    uint8_t i;
    uint8_t is_down;

    if ((expected_source >= CH585_SCAN_SOURCE_COUNT) || (frame == RT_NULL))
    {
        return -1;
    }

    stats = &g_scan.source[expected_source];

#if APP_CH585_SPI_WIRE_SHORT
    if (frame->magic != CH585_SCAN_SHORT_FRAME_MAGIC)
    {
        if (expected_source == 0U)
        {
            memcpy(g_scan.source0_bad_head, frame, sizeof(g_scan.source0_bad_head));
        }
        stats->magic_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

    if (frame->type != CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE)
    {
        stats->version_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

    expected_base = ch585_scan_expected_base(expected_source);
    if (frame->source_id != expected_source)
    {
        stats->source_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }
#else
    if (frame->magic != CH585_SCAN_FRAME_MAGIC)
    {
        if (expected_source == 0U)
        {
            memcpy(g_scan.source0_bad_head, frame, sizeof(g_scan.source0_bad_head));
        }
        stats->magic_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

    if (frame->version != CH585_SCAN_FRAME_VERSION)
    {
        stats->version_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

    if (frame->type != CH585_SCAN_FRAME_TYPE_KEY_STATE)
    {
        stats->version_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

    expected_base = ch585_scan_expected_base(expected_source);
    if (frame->source_id != expected_source)
    {
        stats->source_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

    if (frame->key_count != CH585_SCAN_KEYS_PER_SOURCE)
    {
        stats->length_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }
#endif

    expected_crc = ch585_spi_scan_crc16((const uint8_t *)frame,
                                        (uint16_t)offsetof(ch585_scan_frame_v1_t, crc16));
    if (frame->crc16 != expected_crc)
    {
        stats->crc_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

#if APP_CH585_SPI_WIRE_SHORT
#if !APP_CH585_SPI_REQUEST_ONLY_SHORT
    if ((expected_source == 0U) &&
        (frame->ack_seq != (uint8_t)g_scan.source0_accept_ack_seq))
    {
        g_scan.source0_ack_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }
#endif

    frame_seq = frame->seq;
    frame_flags = ch585_scan_flags_from_short(frame->flags);
#else
    if ((expected_source == 0U) && (frame->ack_seq != g_scan.source0_accept_ack_seq))
    {
        g_scan.source0_ack_errors++;
        ch585_scan_mark_resync_if_source0(expected_source);
        return -1;
    }

    frame_seq = frame->seq;
    frame_flags = frame->flags;
#endif

    if (stats->have_seq != 0U)
    {
#if APP_CH585_SPI_WIRE_SHORT
        expected_seq = (uint16_t)((stats->last_seq + 1U) & 0xFFU);
#else
        expected_seq = (uint16_t)(stats->last_seq + 1U);
#endif
        if (frame_seq != expected_seq)
        {
            stats->seq_drops++;
        }
    }

    stats->last_seq = frame_seq;
    stats->have_seq = 1U;
    stats->frames_ok++;
    ch585_scan_record_flags(stats, frame_flags);

    for (i = 0; i < CH585_SCAN_KEYS_PER_SOURCE; i++)
    {
        is_down = (uint8_t)((frame->down_bits[i >> 3] >> (i & 7U)) & 1U);
        g_scan.raw[expected_base + i] =
            (is_down != 0U) ? CH585_SCAN_PRESSED_ADC : CH585_SCAN_RELEASED_ADC;
    }

    return 0;
}

int ch585_spi_scan_init(void)
{
    memset(&g_scan, 0, sizeof(g_scan));
    memset(g_fake_seq, 0, sizeof(g_fake_seq));
    g_scan.source0_need_resync = 1U;
#if APP_CH585_SPI_REAL_SOURCE0
#if APP_CH585_SPI_HW_SPI2_BACKEND
    (void)ch585_hw_spi2_init();
#endif
    (void)ch585_soft_spi_init();
#if APP_CH585_SPI_DMA_BACKEND
    (void)ch585_dma_soft_spi_init();
#endif
    rt_kprintf("CH585 SPI scan ingest: source0=real preferred=%s fallback=%s, source1=%s\r\n",
#if APP_CH585_SPI_HW_SPI2_BACKEND
#if APP_CH585_SPI_HW_SPI2_GPIO_CS
               "hw-spi2-gpiocs",
#else
               "hw-spi2-hardnss",
#endif
#elif APP_CH585_SPI_DMA_BACKEND
               "dma-gpio",
#else
               "cpu-gpio",
#endif
#if APP_CH585_SPI_HW_SPI2_BACKEND && APP_CH585_SPI_DMA_BACKEND
               "dma-gpio/cpu-gpio",
#elif APP_CH585_SPI_HW_SPI2_BACKEND
               "cpu-gpio",
#elif APP_CH585_SPI_DMA_BACKEND
               "cpu-gpio",
#else
               "none",
#endif
#if APP_CH585_SPI_FAKE_SOURCE1
               "fake"
#else
               "disabled"
#endif
    );
#else
    rt_kprintf("CH585 SPI scan ingest: fake source backend enabled\r\n");
#endif
    rt_kprintf("CH585 SPI wire=%s cmd=%u bytes frame=%u bytes, sources=%u, keys/source=%u\r\n",
#if APP_CH585_SPI_WIRE_SHORT
               "short",
#else
               "legacy",
#endif
               (unsigned int)sizeof(ch585_scan_cmd_v1_t),
               (unsigned int)sizeof(ch585_scan_frame_v1_t),
               (unsigned int)CH585_SCAN_SOURCE_COUNT,
               (unsigned int)CH585_SCAN_KEYS_PER_SOURCE);
    return 0;
}

void ch585_spi_scan_poll_once(void)
{
    ch585_scan_frame_v1_t frame;
    uint8_t source_id;

    g_scan.poll_count++;

#if APP_CH585_SPI_AUTO_TRAIN && APP_CH585_SPI_HW_SPI2_BACKEND
    if ((g_scan.source0_train_done == 0U) &&
        (g_scan.poll_count >= APP_CH585_SPI_AUTO_TRAIN_AFTER_POLLS))
    {
        (void)ch585_hw_spi2_train();
    }
#endif

    for (source_id = 0; source_id < CH585_SCAN_SOURCE_COUNT; source_id++)
    {
        if (ch585_scan_fetch_real_or_fake(source_id, &frame) != 0)
        {
            g_scan.source[source_id].fetch_errors++;
            ch585_scan_mark_resync_if_source0(source_id);
            continue;
        }

        (void)ch585_scan_accept_frame(source_id, &frame);
    }
}

void ch585_spi_scan_dump_stats(void)
{
    uint8_t source_id;

    rt_kprintf("CH585 scan poll=%u raw[0]=%u raw[63]=%u raw[64]=%u raw[127]=%u\r\n",
               (unsigned int)g_scan.poll_count,
               (unsigned int)g_scan.raw[0],
               (unsigned int)g_scan.raw[63],
               (unsigned int)g_scan.raw[64],
               (unsigned int)g_scan.raw[127]);

    for (source_id = 0; source_id < CH585_SCAN_SOURCE_COUNT; source_id++)
    {
        const ch585_scan_source_stats_t *stats = &g_scan.source[source_id];
        rt_kprintf("  src%u ok=%u fetch=%u magic=%u ver=%u src=%u len=%u crc=%u seq_drop=%u flags[o/a/s/y]=%u/%u/%u/%u last_seq=%u\r\n",
                   (unsigned int)source_id,
                   (unsigned int)stats->frames_ok,
                   (unsigned int)stats->fetch_errors,
                   (unsigned int)stats->magic_errors,
                   (unsigned int)stats->version_errors,
                   (unsigned int)stats->source_errors,
                   (unsigned int)stats->length_errors,
                   (unsigned int)stats->crc_errors,
                   (unsigned int)stats->seq_drops,
                   (unsigned int)stats->flag_overrun,
                   (unsigned int)stats->flag_adc_error,
                   (unsigned int)stats->flag_stale,
                   (unsigned int)stats->flag_sync_lost,
                   (unsigned int)stats->last_seq);
    }

    rt_kprintf("  src0 req_rx=%02x %02x %02x %02x head=%02x %02x %02x %02x %02x %02x %02x %02x bad=%02x %02x %02x %02x %02x %02x %02x %02x miso=%u/%u/%u sync=%u@%u.%u repair=%u\r\n",
               (unsigned int)g_scan.source0_req_rx[0],
               (unsigned int)g_scan.source0_req_rx[1],
               (unsigned int)g_scan.source0_req_rx[2],
               (unsigned int)g_scan.source0_req_rx[3],
               (unsigned int)g_scan.source0_head[0],
               (unsigned int)g_scan.source0_head[1],
               (unsigned int)g_scan.source0_head[2],
               (unsigned int)g_scan.source0_head[3],
               (unsigned int)g_scan.source0_head[4],
               (unsigned int)g_scan.source0_head[5],
               (unsigned int)g_scan.source0_head[6],
               (unsigned int)g_scan.source0_head[7],
               (unsigned int)g_scan.source0_bad_head[0],
               (unsigned int)g_scan.source0_bad_head[1],
               (unsigned int)g_scan.source0_bad_head[2],
               (unsigned int)g_scan.source0_bad_head[3],
               (unsigned int)g_scan.source0_bad_head[4],
               (unsigned int)g_scan.source0_bad_head[5],
               (unsigned int)g_scan.source0_bad_head[6],
               (unsigned int)g_scan.source0_bad_head[7],
               (unsigned int)g_scan.source0_miso_idle[0],
               (unsigned int)g_scan.source0_miso_idle[1],
               (unsigned int)g_scan.source0_miso_idle[2],
               (unsigned int)g_scan.source0_sync_found,
               (unsigned int)g_scan.source0_sync_byte,
               (unsigned int)g_scan.source0_sync_bit,
               (unsigned int)g_scan.source0_first_bit_repairs);
    rt_kprintf("  src0 tail=%02x %02x %02x %02x %02x %02x %02x %02x repair_crc rx=%04x exp=%04x miss=%u delay=%u setup_ms=%u cmd_wait_us=%u\r\n",
               (unsigned int)g_scan.source0_tail[0],
               (unsigned int)g_scan.source0_tail[1],
               (unsigned int)g_scan.source0_tail[2],
               (unsigned int)g_scan.source0_tail[3],
               (unsigned int)g_scan.source0_tail[4],
               (unsigned int)g_scan.source0_tail[5],
               (unsigned int)g_scan.source0_tail[6],
               (unsigned int)g_scan.source0_tail[7],
               (unsigned int)g_scan.source0_first_bit_repair_rx_crc,
               (unsigned int)g_scan.source0_first_bit_repair_expected_crc,
               (unsigned int)g_scan.source0_first_bit_repair_crc_misses,
               (unsigned int)APP_CH585_SPI_SOFT_DELAY_CYCLES,
               (unsigned int)APP_CH585_SPI_CS_SETUP_MS,
               (unsigned int)APP_CH585_SPI_CMD_TO_DATA_US);
    rt_kprintf("  src0 backend=%s hwspi2=%u prescaler=0x%04x hsrx=%u cpha=%u train_err=%u/%u resync=%u need_resync=%u cmd_runs=%u cmd_timeout=%u ack_err=%u host_seq=%u spi2_runs=%u spi2_timeout=%u spi2_te=%u spi2_sr=%04x spi2_expect=%u.%u kHz dma_cfg=%u runs=%u timeout=%u te=%u target=%u kHz phase=%u period=%u sample_or=%04x sample_and=%04x\r\n",
               ch585_scan_source0_backend_name(),
#if APP_CH585_SPI_HW_SPI2_BACKEND
               1U,
#else
               0U,
#endif
               (unsigned int)g_scan.source0_spi2_prescaler,
               (unsigned int)g_scan.source0_spi2_hsrx,
               (unsigned int)g_scan.source0_spi2_cpha,
               (unsigned int)g_scan.source0_train_errors,
               (unsigned int)g_scan.source0_train_frames,
               (unsigned int)g_scan.source0_resync_runs,
               (unsigned int)g_scan.source0_need_resync,
               (unsigned int)g_scan.source0_cmd_runs,
               (unsigned int)g_scan.source0_cmd_timeouts,
               (unsigned int)g_scan.source0_ack_errors,
               (unsigned int)g_scan.source0_host_seq,
               (unsigned int)g_scan.source0_spi2_runs,
               (unsigned int)g_scan.source0_spi2_timeouts,
               (unsigned int)g_scan.source0_spi2_te_flags,
               (unsigned int)g_scan.source0_spi2_sr_last,
#if APP_CH585_SPI_HW_SPI2_BACKEND
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() / 10U),
               (unsigned int)(ch585_hw_spi2_expected_sck_khz_x10() % 10U),
#else
               0U,
               0U,
#endif
               (unsigned int)APP_CH585_SPI_DMA_BACKEND,
               (unsigned int)g_scan.source0_dma_runs,
               (unsigned int)g_scan.source0_dma_timeouts,
               (unsigned int)g_scan.source0_dma_te_flags,
               (unsigned int)APP_CH585_SPI_DMA_TARGET_KHZ,
               (unsigned int)CH585_SPI_DMA_PHASES_PER_BIT,
               (unsigned int)g_scan.source0_dma_period,
               (unsigned int)g_scan.source0_dma_sample_or,
               (unsigned int)g_scan.source0_dma_sample_and);
    if (g_scan.source0_train_candidate_count != 0U)
    {
        uint8_t i;

        rt_kprintf("  src0 train candidates:");
        for (i = 0U; i < g_scan.source0_train_candidate_count; i++)
        {
            rt_kprintf(" p%02x/h%u/c%u bad=%u seq=%u;",
                       (unsigned int)g_scan.source0_train_candidate_prescaler[i],
                       (unsigned int)g_scan.source0_train_candidate_hsrx[i],
                       (unsigned int)g_scan.source0_train_candidate_cpha[i],
                       (unsigned int)g_scan.source0_train_candidate_bad[i],
                       (unsigned int)g_scan.source0_train_candidate_seq[i]);
        }
        rt_kprintf("\r\n");
    }
    rt_kprintf("  src0 speed: %u bytes %u bits xfer=%u us cycles=%u sck=%u.%u kHz frame=%u.%u fps core=%u Hz hclk=%u Hz\r\n",
               (unsigned int)APP_CH585_SPI_SOURCE0_CAPTURE_BYTES,
               (unsigned int)(APP_CH585_SPI_SOURCE0_CAPTURE_BYTES * 8U),
               (unsigned int)g_scan.source0_xfer_us,
               (unsigned int)g_scan.source0_xfer_cycles,
               (unsigned int)(g_scan.source0_sck_khz_x10 / 10U),
               (unsigned int)(g_scan.source0_sck_khz_x10 % 10U),
               (unsigned int)(g_scan.source0_frame_fps_x10 / 10U),
               (unsigned int)(g_scan.source0_frame_fps_x10 % 10U),
               (unsigned int)SystemCoreClock,
               (unsigned int)HCLKClock);
}

const uint16_t *ch585_spi_scan_raw(void)
{
    return g_scan.raw;
}

const ch585_scan_source_stats_t *ch585_spi_scan_source_stats(uint8_t source_id)
{
    if (source_id >= CH585_SCAN_SOURCE_COUNT)
    {
        return RT_NULL;
    }

    return &g_scan.source[source_id];
}

uint32_t ch585_spi_scan_source0_sck_khz_x10(void)
{
    return g_scan.source0_sck_khz_x10;
}

uint16_t ch585_spi_scan_source0_prescaler(void)
{
    return g_scan.source0_spi2_prescaler;
}

uint8_t ch585_spi_scan_source0_hsrx(void)
{
    return g_scan.source0_spi2_hsrx;
}

uint8_t ch585_spi_scan_source0_cpha_edges(void)
{
    return (uint8_t)((g_scan.source0_spi2_cpha == SPI_CPHA_2Edge) ? 2U : 1U);
}

uint8_t ch585_spi_scan_source0_train_done(void)
{
    return g_scan.source0_train_done;
}

uint16_t ch585_spi_scan_source0_train_errors(void)
{
    return g_scan.source0_train_errors;
}

uint16_t ch585_spi_scan_source0_train_frames(void)
{
    return g_scan.source0_train_frames;
}

uint8_t ch585_spi_scan_source0_train_candidate_count(void)
{
    return g_scan.source0_train_candidate_count;
}

int ch585_spi_scan_source0_train_candidate(uint8_t index,
                                           uint16_t *prescaler,
                                           uint8_t *hsrx,
                                           uint8_t *cpha_edges,
                                           uint16_t *bad_errors,
                                           uint16_t *seq_errors)
{
    if ((index >= g_scan.source0_train_candidate_count) ||
        (prescaler == RT_NULL) ||
        (hsrx == RT_NULL) ||
        (cpha_edges == RT_NULL) ||
        (bad_errors == RT_NULL) ||
        (seq_errors == RT_NULL))
    {
        return -1;
    }

    *prescaler = g_scan.source0_train_candidate_prescaler[index];
    *hsrx = g_scan.source0_train_candidate_hsrx[index];
    *cpha_edges = g_scan.source0_train_candidate_cpha[index];
    *bad_errors = g_scan.source0_train_candidate_bad[index];
    *seq_errors = g_scan.source0_train_candidate_seq[index];
    return 0;
}
