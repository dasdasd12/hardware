/*
 * Temporary H417 -> left CH585 BLE HID bridge.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <rtthread.h>

#include "ch32h417_dma.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "ch32h417_spi.h"

#include "ch585_ble_bridge.h"
#include "ch585_spi_scan.h"

extern uint32_t SystemCoreClock;

#ifndef APP_CH585_BLE_BRIDGE_DOWN_THRESHOLD_ADC
#define APP_CH585_BLE_BRIDGE_DOWN_THRESHOLD_ADC \
    ((CH585_SCAN_RELEASED_ADC + CH585_SCAN_PRESSED_ADC) / 2U)
#endif

#ifndef APP_CH585_BLE_BRIDGE_REFRESH_LOOPS
#define APP_CH585_BLE_BRIDGE_REFRESH_LOOPS 100U
#endif

#ifndef APP_CH585_BLE_BRIDGE_DEBUG_ONLY_KEY_ID
#define APP_CH585_BLE_BRIDGE_DEBUG_ONLY_KEY_ID 0xFFU
#endif

#define BLE_BRIDGE_SPIx SPI1
#define BLE_BRIDGE_SPI_RCC_ENABLE() RCC_HB2PeriphClockCmd(RCC_HB2Periph_SPI1, ENABLE)

#define BLE_BRIDGE_RIGHT_CS_PORT GPIOD
#define BLE_BRIDGE_RIGHT_CS_PIN  GPIO_Pin_9
#define BLE_BRIDGE_LEFT_CS_PORT  GPIOF
#define BLE_BRIDGE_LEFT_CS_PIN   GPIO_Pin_2

#define BLE_BRIDGE_FRAME_MAGIC   0xB8U
#define BLE_BRIDGE_FRAME_TYPE_REPORT 0x31U
#define BLE_BRIDGE_FRAME_VERSION 1U
#define BLE_BRIDGE_REPORT_LEN    8U

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t type;
    uint8_t version;
    uint8_t seq;
    uint8_t flags;
    uint8_t report[BLE_BRIDGE_REPORT_LEN];
    uint8_t reserved;
    uint16_t crc16;
} ch585_ble_bridge_frame_t;

typedef struct
{
    uint8_t initialized;
    uint8_t seq;
    uint8_t last_report[BLE_BRIDGE_REPORT_LEN];
    uint32_t poll_count;
    uint32_t reports_sent;
    uint32_t send_errors;
} ch585_ble_bridge_state_t;

typedef struct
{
    uint8_t key_id;
    uint8_t usage;
    uint8_t modifier;
} ch585_ble_bridge_keymap_t;

static ch585_ble_bridge_state_t g_bridge;

/*
 * Debug map for the right half PCB, copied from the right CH585 ADS7948 probe
 * key map. This is a temporary stand-in for the future compiled Profile table.
 */
static const ch585_ble_bridge_keymap_t g_right_keymap[] = {
    {0U,  0x45U, 0U},    /* F12 */
    {1U,  0x44U, 0U},    /* F11 */
    {2U,  0x43U, 0U},    /* F10 */
    {3U,  0x42U, 0U},    /* F9 */
    {4U,  0x41U, 0U},    /* F8 */
    {5U,  0x40U, 0U},    /* F7 */
    {6U,  0x3FU, 0U},    /* F6 */
    {7U,  0x2AU, 0U},    /* Backspace */
    {8U,  0x2EU, 0U},    /* Equal */
    {9U,  0x2DU, 0U},    /* Minus */
    {16U, 0x27U, 0U},    /* 0 */
    {17U, 0x26U, 0U},    /* 9 */
    {18U, 0x25U, 0U},    /* 8 */
    {19U, 0x24U, 0U},    /* 7 */
    {20U, 0x31U, 0U},    /* Backslash */
    {21U, 0x30U, 0U},    /* Right bracket */
    {22U, 0x2FU, 0U},    /* Left bracket */
    {23U, 0x13U, 0U},    /* P */
    {24U, 0x12U, 0U},    /* O */
    {25U, 0x0CU, 0U},    /* I */
    {32U, 0x18U, 0U},    /* U */
    {33U, 0x1CU, 0U},    /* Y */
    {34U, 0x28U, 0U},    /* Enter */
    {35U, 0x34U, 0U},    /* Quote */
    {36U, 0x33U, 0U},    /* Semicolon */
    {37U, 0x0FU, 0U},    /* L */
    {38U, 0x0EU, 0U},    /* K */
    {39U, 0x0DU, 0U},    /* J */
    {40U, 0x0BU, 0U},    /* H */
    {41U, 0U,    0x20U}, /* Right shift */
    {48U, 0x38U, 0U},    /* Slash */
    {49U, 0x37U, 0U},    /* Dot */
    {50U, 0x36U, 0U},    /* Comma */
    {51U, 0x10U, 0U},    /* M */
    {52U, 0x11U, 0U},    /* N */
    {53U, 0x05U, 0U},    /* B */
    {54U, 0U,    0x10U}, /* Right ctrl */
    {55U, 0U,    0x80U}, /* Right GUI */
    {56U, 0U,    0U},    /* Fn: local layer key, no HID output yet */
    {57U, 0U,    0x40U}, /* Right alt */
    {58U, 0x2CU, 0U},    /* Space */
};

static uint32_t bridge_cycle_now(void)
{
    uint32_t value;
    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static void bridge_delay_us(uint32_t us)
{
    uint32_t cycles;
    uint32_t start;

    if ((SystemCoreClock == 0U) || (us == 0U))
    {
        return;
    }

    cycles = (uint32_t)((((uint64_t)SystemCoreClock * (uint64_t)us) +
                         999999ULL) / 1000000ULL);
    start = bridge_cycle_now();
    while ((uint32_t)(bridge_cycle_now() - start) < cycles)
    {
    }
}

static uint16_t bridge_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static void bridge_flush_rx(void)
{
    volatile uint16_t dummy;

    while (SPI_I2S_GetFlagStatus(BLE_BRIDGE_SPIx, SPI_I2S_FLAG_RXNE) != RESET)
    {
        dummy = SPI_I2S_ReceiveData(BLE_BRIDGE_SPIx);
        (void)dummy;
    }

    dummy = BLE_BRIDGE_SPIx->STATR;
    dummy = BLE_BRIDGE_SPIx->DATAR;
    (void)dummy;
}

static void bridge_apply_spi_config(void)
{
    SPI_InitTypeDef spi = {0};

    SPI_Cmd(BLE_BRIDGE_SPIx, DISABLE);
    SPI_I2S_DeInit(BLE_BRIDGE_SPIx);
    SPI_SSOutputCmd(BLE_BRIDGE_SPIx, DISABLE);

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_Mode4;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7U;
    SPI_Init(BLE_BRIDGE_SPIx, &spi);
    SPI_NSSInternalSoftwareConfig(BLE_BRIDGE_SPIx, SPI_NSSInternalSoft_Set);
    SPI_HighSpeedMode_Config(BLE_BRIDGE_SPIx, SPI_HIGH_SPEED_MODE2, ENABLE);
    SPI_Cmd(BLE_BRIDGE_SPIx, DISABLE);
}

static int bridge_spi_send_left(const uint8_t *tx, uint16_t len)
{
    uint32_t timeout_cycles;
    uint32_t start;
    uint16_t i;
    volatile uint16_t rx_discard;

    if ((tx == RT_NULL) || (len == 0U))
    {
        return -1;
    }

    timeout_cycles = (SystemCoreClock != 0U) ? (SystemCoreClock / 100U) : 4000000U;

    GPIO_SetBits(BLE_BRIDGE_RIGHT_CS_PORT, BLE_BRIDGE_RIGHT_CS_PIN);
    GPIO_SetBits(BLE_BRIDGE_LEFT_CS_PORT, BLE_BRIDGE_LEFT_CS_PIN);

    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    SPI_I2S_DMACmd(BLE_BRIDGE_SPIx, SPI_I2S_DMAReq_Rx, DISABLE);
    SPI_I2S_DMACmd(BLE_BRIDGE_SPIx, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_Cmd(BLE_BRIDGE_SPIx, DISABLE);
    bridge_flush_rx();

    SPI_Cmd(BLE_BRIDGE_SPIx, ENABLE);
    GPIO_ResetBits(BLE_BRIDGE_LEFT_CS_PORT, BLE_BRIDGE_LEFT_CS_PIN);
    bridge_delay_us(1U);

    for (i = 0U; i < len; i++)
    {
        start = bridge_cycle_now();
        while ((SPI_I2S_GetFlagStatus(BLE_BRIDGE_SPIx, SPI_I2S_FLAG_TXE) == RESET) &&
               ((uint32_t)(bridge_cycle_now() - start) < timeout_cycles))
        {
        }
        if (SPI_I2S_GetFlagStatus(BLE_BRIDGE_SPIx, SPI_I2S_FLAG_TXE) == RESET)
        {
            goto timeout;
        }

        SPI_I2S_SendData(BLE_BRIDGE_SPIx, tx[i]);

        start = bridge_cycle_now();
        while ((SPI_I2S_GetFlagStatus(BLE_BRIDGE_SPIx, SPI_I2S_FLAG_RXNE) == RESET) &&
               ((uint32_t)(bridge_cycle_now() - start) < timeout_cycles))
        {
        }
        if (SPI_I2S_GetFlagStatus(BLE_BRIDGE_SPIx, SPI_I2S_FLAG_RXNE) == RESET)
        {
            goto timeout;
        }

        rx_discard = SPI_I2S_ReceiveData(BLE_BRIDGE_SPIx);
        (void)rx_discard;
    }

    start = bridge_cycle_now();
    while ((SPI_I2S_GetFlagStatus(BLE_BRIDGE_SPIx, SPI_I2S_FLAG_BSY) != RESET) &&
           ((uint32_t)(bridge_cycle_now() - start) < timeout_cycles))
    {
    }
    if (SPI_I2S_GetFlagStatus(BLE_BRIDGE_SPIx, SPI_I2S_FLAG_BSY) != RESET)
    {
        goto timeout;
    }

    GPIO_SetBits(BLE_BRIDGE_LEFT_CS_PORT, BLE_BRIDGE_LEFT_CS_PIN);
    SPI_Cmd(BLE_BRIDGE_SPIx, DISABLE);
    return 0;

timeout:
    GPIO_SetBits(BLE_BRIDGE_LEFT_CS_PORT, BLE_BRIDGE_LEFT_CS_PIN);
    SPI_Cmd(BLE_BRIDGE_SPIx, DISABLE);
    return -1;
}

static uint8_t bridge_build_report_from_raw(const uint16_t *raw_adc,
                                            uint16_t key_count,
                                            uint8_t report[BLE_BRIDGE_REPORT_LEN],
                                            uint8_t *first_key)
{
    uint16_t map_index;
    uint8_t slot = 2U;
    uint8_t any_down = 0U;

    memset(report, 0, BLE_BRIDGE_REPORT_LEN);
    if (first_key != RT_NULL)
    {
        *first_key = 0xFFU;
    }

    if (raw_adc == RT_NULL)
    {
        return 0U;
    }

    for (map_index = 0U;
         map_index < (uint16_t)(sizeof(g_right_keymap) / sizeof(g_right_keymap[0]));
         map_index++)
    {
        const ch585_ble_bridge_keymap_t *entry = &g_right_keymap[map_index];

#if APP_CH585_BLE_BRIDGE_DEBUG_ONLY_KEY_ID != 0xFFU
        if (entry->key_id != (uint8_t)APP_CH585_BLE_BRIDGE_DEBUG_ONLY_KEY_ID)
        {
            continue;
        }
#endif

        if (entry->key_id >= key_count)
        {
            continue;
        }

        if (raw_adc[entry->key_id] < APP_CH585_BLE_BRIDGE_DOWN_THRESHOLD_ADC)
        {
            continue;
        }

        any_down = 1U;
        if ((first_key != RT_NULL) && (*first_key == 0xFFU))
        {
            *first_key = entry->key_id;
        }

        report[0] |= entry->modifier;

        if (entry->usage == 0U)
        {
            continue;
        }

        if (slot < BLE_BRIDGE_REPORT_LEN)
        {
            report[slot++] = entry->usage;
        }
    }

    return any_down;
}

static int bridge_send_report(const uint8_t report[BLE_BRIDGE_REPORT_LEN],
                              uint8_t any_down,
                              uint8_t first_key)
{
    ch585_ble_bridge_frame_t frame;
    const ch585_scan_source_stats_t *stats;

    memset(&frame, 0, sizeof(frame));
    frame.magic = BLE_BRIDGE_FRAME_MAGIC;
    frame.type = BLE_BRIDGE_FRAME_TYPE_REPORT;
    frame.version = BLE_BRIDGE_FRAME_VERSION;
    frame.seq = ++g_bridge.seq;
    frame.flags = (any_down != 0U) ? 0x01U : 0x00U;
    frame.reserved = first_key;

    stats = ch585_spi_scan_source_stats(0U);
    if (stats != RT_NULL)
    {
        if (stats->have_seq != 0U)
        {
            frame.flags |= 0x02U;
        }
        if ((stats->fetch_errors != 0U) ||
            (stats->magic_errors != 0U) ||
            (stats->version_errors != 0U) ||
            (stats->source_errors != 0U) ||
            (stats->length_errors != 0U) ||
            (stats->crc_errors != 0U) ||
            (stats->seq_drops != 0U))
        {
            frame.flags |= 0x04U;
        }
    }
    else
    {
        frame.flags |= 0x80U;
    }

    memcpy(frame.report, report, BLE_BRIDGE_REPORT_LEN);
    frame.crc16 = bridge_crc16((const uint8_t *)&frame,
                               (uint16_t)offsetof(ch585_ble_bridge_frame_t, crc16));

    if (bridge_spi_send_left((const uint8_t *)&frame, (uint16_t)sizeof(frame)) != 0)
    {
        g_bridge.send_errors++;
        return -1;
    }

    g_bridge.reports_sent++;
    memcpy(g_bridge.last_report, report, BLE_BRIDGE_REPORT_LEN);
    return 0;
}

int ch585_ble_bridge_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    memset(&g_bridge, 0, sizeof(g_bridge));
    memset(g_bridge.last_report, 0xFF, sizeof(g_bridge.last_report));

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOF, ENABLE);
    BLE_BRIDGE_SPI_RCC_ENABLE();
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap_VIO3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VDD3V3_IO_HSLV, ENABLE);

    GPIO_SetBits(BLE_BRIDGE_RIGHT_CS_PORT, BLE_BRIDGE_RIGHT_CS_PIN);
    GPIO_SetBits(BLE_BRIDGE_LEFT_CS_PORT, BLE_BRIDGE_LEFT_CS_PIN);

    gpio.GPIO_Pin = BLE_BRIDGE_RIGHT_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(BLE_BRIDGE_RIGHT_CS_PORT, &gpio);

    gpio.GPIO_Pin = BLE_BRIDGE_LEFT_CS_PIN;
    GPIO_Init(BLE_BRIDGE_LEFT_CS_PORT, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_5;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    bridge_apply_spi_config();
    g_bridge.initialized = 1U;

    rt_kprintf("CH585 BLE bridge: left/U2 CS=PF2 report frame=%u bytes\r\n",
               (unsigned int)sizeof(ch585_ble_bridge_frame_t));
    return 0;
}

void ch585_ble_bridge_poll_from_raw(const uint16_t *raw_adc, uint16_t key_count)
{
    uint8_t report[BLE_BRIDGE_REPORT_LEN];
    uint8_t any_down;
    uint8_t changed;
    uint8_t first_key;
    uint8_t refresh;

    if (g_bridge.initialized == 0U)
    {
        return;
    }

    any_down = bridge_build_report_from_raw(raw_adc, key_count, report, &first_key);
    changed = (memcmp(report, g_bridge.last_report, BLE_BRIDGE_REPORT_LEN) != 0) ? 1U : 0U;
    refresh = ((APP_CH585_BLE_BRIDGE_REFRESH_LOOPS != 0U) &&
               ((g_bridge.poll_count % APP_CH585_BLE_BRIDGE_REFRESH_LOOPS) == 0U)) ? 1U : 0U;
    g_bridge.poll_count++;

    if ((changed != 0U) || (refresh != 0U))
    {
        (void)bridge_send_report(report, any_down, first_key);
    }
}

uint32_t ch585_ble_bridge_reports_sent(void)
{
    return g_bridge.reports_sent;
}

uint32_t ch585_ble_bridge_send_errors(void)
{
    return g_bridge.send_errors;
}

uint8_t ch585_ble_bridge_last_seq(void)
{
    return g_bridge.seq;
}
