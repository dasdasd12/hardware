/*
 * Minimal CH585M SPI0 slave key-state source for the H417 SPI bring-up.
 *
 * Final PCB wiring expected by the current H417 test firmware:
 *   CH585 PA12 / SPI0 CS   <- H417 PD9 or PF2 GPIO CS
 *   CH585 PA13 / SPI0 SCK  <- H417 PB3 / SPI1 SCK
 *   CH585 PA14 / SPI0 MOSI <- H417 PB5 / SPI1 MOSI
 *   CH585 PA15 / SPI0 MISO -> H417 PB4 / SPI1 MISO
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CH58x_common.h"
#include "ads7948.h"

#define CH585_SCAN_FRAME_MAGIC       0x4BD3U
#define CH585_SCAN_FRAME_VERSION     2U
#define CH585_SCAN_FRAME_TYPE_KEY_STATE 0x10U
#define CH585_SCAN_CMD_MAGIC         0x524BU
#define CH585_SCAN_CMD_GET_STATE     0x01U
#define CH585_SCAN_CMD_GET_DEBUG     0x02U
#define CH585_SCAN_CMD_GET_CONFIG    0x03U
#define CH585_SCAN_CMD_SET_CONFIG    0x04U
#define CH585_SCAN_CMD_CALIBRATE_KEY 0x05U
#define CH585_SCAN_CMD_CALIBRATE_ALL 0x06U
#define CH585_SCAN_SHORT_FRAME_MAGIC 0xD7U
#define CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE 0x11U
#define CH585_SCAN_SHORT_FRAME_TYPE_KEY_DEBUG 0x12U
#define CH585_SCAN_SHORT_CMD_MAGIC   0xA7U
#define CH585_SCAN_SOURCE_ID         0U
#define CH585_SCAN_KEYS_PER_SOURCE   64U
#define CH585_SCAN_DOWN_BYTES        (CH585_SCAN_KEYS_PER_SOURCE / 8U)
#define CH585_SCAN_FLAG_READY        (1U << 15)
#define CH585_SCAN_FLAG_CMD_ERROR    (1U << 4)
#define CH585_SCAN_SHORT_FLAG_READY  (1U << 7)
#define CH585_SCAN_SHORT_FLAG_CMD_ERROR (1U << 4)
#define CH585_SCAN_SHORT_FLAG_ADC_ERROR (1U << 1)
#define CH585_SCAN_SHORT_DEBUG_FLAG_DOWN     (1U << 0)
#define CH585_SCAN_SHORT_DEBUG_FLAG_RT_ARMED (1U << 1)
#define CH585_SHORT_DIAG_VALID_FLAG  (1U << 7)
#define CH585_SHORT_DIAG_CMD_ERROR_FLAG (1U << 6)
#define CH585_SHORT_DIAG_CMD_MASK    0x0FU
#define CH585_SCAN_REQ0              'K'
#define CH585_SCAN_REQ1              'R'
#define CH585_SCAN_CFG_RELEASED_ADC       0x01U
#define CH585_SCAN_CFG_PRESSED_ADC        0x02U
#define CH585_SCAN_CFG_MIN_ADC            0x03U
#define CH585_SCAN_CFG_MAX_ADC            0x04U
#define CH585_SCAN_CFG_PRESS_POSITION     0x05U
#define CH585_SCAN_CFG_RELEASE_POSITION   0x06U
#define CH585_SCAN_CFG_RT_PRESS_DELTA     0x07U
#define CH585_SCAN_CFG_RT_RELEASE_DELTA   0x08U
#define CH585_SCAN_CFG_FILTER_SHIFT       0x09U
#define CH585_SCAN_CFG_RT_ENABLE          0x0AU
#define CH585_SCAN_CFG_VALID              0x0BU
#define CH585_SCAN_CFG_GLOBAL_KEY_ID      0x0CU
#define CH585_SIM_RELEASED_ADC       1000U
#define CH585_SIM_PRESSED_ADC        3000U
#define CH585_KEY_DEFAULT_MIN_ADC    0U
#define CH585_KEY_DEFAULT_MAX_ADC    4095U
#define CH585_KEY_DEFAULT_FILTER_SHIFT 2U
#define CH585_KEY_DEFAULT_PRESS_POSITION_PM 500U
#define CH585_KEY_DEFAULT_RELEASE_POSITION_PM 350U
#define CH585_KEY_DEFAULT_RT_PRESS_DELTA_PM 80U
#define CH585_KEY_DEFAULT_RT_RELEASE_DELTA_PM 80U
#define CH585_KEY_INVALID_GLOBAL_ID  0xFFFFU
#define CH585_SIM_ACTIVE_KEYS        4U
#define CH585_SIM_PERIOD_FRAMES      32U

#ifndef CH585_DEBUG_FRAME_INTERVAL
#define CH585_DEBUG_FRAME_INTERVAL   8U
#endif

#ifndef CH585_LINK_TEST_MISO_LOW
#define CH585_LINK_TEST_MISO_LOW     0
#endif

#ifndef CH585_LINK_TEST_SPI_PATTERN
#define CH585_LINK_TEST_SPI_PATTERN  0
#endif

#ifndef CH585_SPI0_MISO_STRONG_DRIVE
#define CH585_SPI0_MISO_STRONG_DRIVE 0
#endif

#ifndef CH585_FAST_SIM_FRAME
#define CH585_FAST_SIM_FRAME       0
#endif

#ifndef CH585_KEY_ENABLE_RAPID_TRIGGER
#define CH585_KEY_ENABLE_RAPID_TRIGGER 1
#endif

#ifndef CH585_STATIC_FRAME_TEST
#define CH585_STATIC_FRAME_TEST    0
#endif

#ifndef CH585_USE_SPI0_SLAVE_DMA
#define CH585_USE_SPI0_SLAVE_DMA     1
#endif

#ifndef CH585_USE_SHORT_FRAME
#define CH585_USE_SHORT_FRAME        1
#endif

#ifndef CH585_USE_PIPELINE_SHORT
#define CH585_USE_PIPELINE_SHORT     0
#endif

#ifndef CH585_USE_REQUEST_ONLY_SHORT
#define CH585_USE_REQUEST_ONLY_SHORT CH585_USE_SHORT_FRAME
#endif

#ifndef CH585_REQUEST_ONLY_CAPTURE_CMD
#define CH585_REQUEST_ONLY_CAPTURE_CMD 1
#endif

#ifndef CH585_SHORT_DIAG_IN_DOWN_BITS
#define CH585_SHORT_DIAG_IN_DOWN_BITS 1
#endif

#ifndef CH585_ADC_PROBE_MODE
#define CH585_ADC_PROBE_MODE 0
#endif

#ifndef CH585_ADC_PROBE_RIGHT_HALF
#define CH585_ADC_PROBE_RIGHT_HALF 1
#endif

#ifndef CH585_ADC_PROBE_SPI1_DIV
#define CH585_ADC_PROBE_SPI1_DIV 2U
#endif

#ifndef CH585_ADC_PROBE_SETTLE_US
#define CH585_ADC_PROBE_SETTLE_US 20U
#endif

#ifndef CH585_ADC_PROBE_CS_HIGH_US
#define CH585_ADC_PROBE_CS_HIGH_US 0U
#endif

#ifndef CH585_ADC_PROBE_DISCARD_FRAMES
#define CH585_ADC_PROBE_DISCARD_FRAMES 1U
#endif

#ifndef CH585_ADC_PROBE_PDEN_ENABLE_LEVEL
#define CH585_ADC_PROBE_PDEN_ENABLE_LEVEL 1U
#endif

#ifndef CH585_ADC_PROBE_FAST_REPEAT
#define CH585_ADC_PROBE_FAST_REPEAT 1
#endif

#ifndef CH585_ADC_PROBE_DEBUG_KEY
#define CH585_ADC_PROBE_DEBUG_KEY 58U
#endif

#ifndef CH585_ADC_PROBE_UART_MODE
#define CH585_ADC_PROBE_UART_MODE 0
#endif

#ifndef CH585_ADC_PROBE_UART_PORT
#define CH585_ADC_PROBE_UART_PORT 1
#endif

#ifndef CH585_ADC_PROBE_UART_BAUD
#define CH585_ADC_PROBE_UART_BAUD 115200U
#endif

#ifndef CH585_ADC_PROBE_UART0_REMAP
#define CH585_ADC_PROBE_UART0_REMAP 0
#endif

#ifndef CH585_ADC_PROBE_UART_PERIOD_MS
#define CH585_ADC_PROBE_UART_PERIOD_MS 10U
#endif

#ifndef CH585_ADC_PROBE_UART_EVENT_MODE
#define CH585_ADC_PROBE_UART_EVENT_MODE 0
#endif

#ifndef CH585_ADC_PROBE_USE_RIGHT_KEYMAP
#define CH585_ADC_PROBE_USE_RIGHT_KEYMAP CH585_ADC_PROBE_RIGHT_HALF
#endif

#ifndef CH585_ADC_PROBE_USE_LEFT_KEYMAP
#define CH585_ADC_PROBE_USE_LEFT_KEYMAP (!CH585_ADC_PROBE_USE_RIGHT_KEYMAP)
#endif

#ifndef CH585_ADC_PROBE_CAL_ENABLE
#define CH585_ADC_PROBE_CAL_ENABLE 1
#endif

#ifndef CH585_ADC_PROBE_CAL_RELEASED_ADC
#define CH585_ADC_PROBE_CAL_RELEASED_ADC 500U
#endif

#ifndef CH585_ADC_PROBE_CAL_PRESSED_ADC
#define CH585_ADC_PROBE_CAL_PRESSED_ADC 350U
#endif

#ifndef CH585_ADC_PROBE_CAL_PRESS_POSITION_PM
#define CH585_ADC_PROBE_CAL_PRESS_POSITION_PM CH585_KEY_DEFAULT_PRESS_POSITION_PM
#endif

#ifndef CH585_ADC_PROBE_CAL_RELEASE_POSITION_PM
#define CH585_ADC_PROBE_CAL_RELEASE_POSITION_PM CH585_KEY_DEFAULT_RELEASE_POSITION_PM
#endif

#ifndef CH585_ADC_PROBE_CAL_FILTER_SHIFT
#define CH585_ADC_PROBE_CAL_FILTER_SHIFT 0U
#endif

#ifndef CH585_ADC_PROBE_CAL_RT_ENABLE
#define CH585_ADC_PROBE_CAL_RT_ENABLE 0
#endif

#ifndef CH585_SPI_UART_TELEMETRY
#define CH585_SPI_UART_TELEMETRY 0
#endif

#ifndef CH585_SPI_UART_TELEMETRY_PORT
#define CH585_SPI_UART_TELEMETRY_PORT 1
#endif

#ifndef CH585_SPI_UART_TELEMETRY_BAUD
#define CH585_SPI_UART_TELEMETRY_BAUD 115200U
#endif

#ifndef CH585_SPI_UART_TELEMETRY_PERIOD_FRAMES
#define CH585_SPI_UART_TELEMETRY_PERIOD_FRAMES 1000U
#endif

#define CH585_ADC_PROBE_SCAN_ALL_KEY 0xFFU
#define CH585_ADC_PROBE_LANE_COUNT 4U
#define CH585_ADC_PROBE_MUX_COUNT 16U
#define CH585_ADC_PROBE_KEY_COUNT \
    (CH585_ADC_PROBE_LANE_COUNT * CH585_ADC_PROBE_MUX_COUNT)

#define CH585_MODE_PIPELINE_SHORT \
    (CH585_USE_PIPELINE_SHORT && CH585_USE_SHORT_FRAME)
#define CH585_MODE_REQUEST_ONLY_SHORT \
    (CH585_USE_REQUEST_ONLY_SHORT && CH585_USE_SHORT_FRAME)
#define CH585_MODE_COMMAND_RESPONSE \
    (!CH585_MODE_PIPELINE_SHORT && !CH585_MODE_REQUEST_ONLY_SHORT)

typedef struct __attribute__((packed))
{
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t source_id;
    uint8_t key_count;
    uint16_t seq;
    uint16_t flags;
    uint16_t ack_seq;
    uint8_t down_bits[CH585_SCAN_DOWN_BYTES];
    uint16_t diag;
    uint16_t crc16;
} ch585_scan_frame_v2_t;

typedef struct __attribute__((packed))
{
    uint16_t magic;
    uint8_t version;
    uint8_t cmd;
    uint16_t host_seq;
    uint16_t ack_seq;
    uint16_t flags;
    uint16_t crc16;
} ch585_scan_cmd_legacy_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t type;
    uint8_t source_id;
    uint8_t seq;
    uint8_t ack_seq;
    uint8_t flags;
    uint8_t down_bits[CH585_SCAN_DOWN_BYTES];
    uint16_t crc16;
} ch585_scan_frame_short_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t type;
    uint8_t source_id;
    uint8_t seq;
    uint8_t key_id;
    uint8_t flags;
    uint16_t raw_adc;
    uint16_t filtered_adc;
    uint16_t position_pm;
    uint16_t peak_pm;
    uint16_t crc16;
} ch585_scan_debug_short_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t cmd;
    uint8_t host_seq;
    uint8_t ack_seq;
    uint8_t target_key;
    uint8_t param_id;
    uint16_t value;
    uint16_t flags;
    uint16_t aux;
    uint8_t reserved[2];
    uint16_t crc16;
} ch585_scan_cmd_short_t;

#if CH585_USE_SHORT_FRAME
typedef ch585_scan_frame_short_t ch585_scan_wire_frame_t;
typedef ch585_scan_cmd_short_t ch585_scan_wire_cmd_t;
#else
typedef ch585_scan_frame_v2_t ch585_scan_wire_frame_t;
typedef ch585_scan_cmd_legacy_t ch585_scan_wire_cmd_t;
#endif

typedef struct
{
    uint16_t released_adc;
    uint16_t pressed_adc;
    uint16_t min_adc;
    uint16_t max_adc;
    uint16_t press_position_pm;
    uint16_t release_position_pm;
    uint16_t rt_press_delta_pm;
    uint16_t rt_release_delta_pm;
    uint16_t global_key_id;
    uint8_t filter_shift;
    uint8_t rt_enable;
    uint8_t valid;
} ch585_key_config_t;

#if CH585_MODE_PIPELINE_SHORT || CH585_MODE_COMMAND_RESPONSE
static __attribute__((aligned(4))) ch585_scan_wire_frame_t g_frame;
#endif
#if CH585_MODE_REQUEST_ONLY_SHORT
static __attribute__((aligned(4))) ch585_scan_wire_frame_t g_frame_pingpong[2];
#endif
#if CH585_MODE_PIPELINE_SHORT || CH585_MODE_REQUEST_ONLY_SHORT || CH585_MODE_COMMAND_RESPONSE
static __attribute__((aligned(4))) ch585_scan_wire_cmd_t g_cmd;
#endif
#if CH585_MODE_PIPELINE_SHORT
static __attribute__((aligned(4))) uint8_t g_pipe_rx[sizeof(ch585_scan_wire_frame_t)];
#endif
#if CH585_MODE_REQUEST_ONLY_SHORT && CH585_REQUEST_ONLY_CAPTURE_CMD && CH585_USE_SPI0_SLAVE_DMA
static __attribute__((aligned(4))) uint8_t g_req_rx[sizeof(ch585_scan_wire_frame_t)];
#endif
static ch585_key_config_t g_key_config[CH585_SCAN_KEYS_PER_SOURCE];
static uint8_t g_key_down[CH585_SCAN_KEYS_PER_SOURCE];
static uint8_t g_key_filter_valid[CH585_SCAN_KEYS_PER_SOURCE];
static uint8_t g_key_rt_armed[CH585_SCAN_KEYS_PER_SOURCE];
static uint16_t g_key_raw_adc[CH585_SCAN_KEYS_PER_SOURCE];
static uint16_t g_key_filtered_adc[CH585_SCAN_KEYS_PER_SOURCE];
static uint32_t g_key_filtered_q8[CH585_SCAN_KEYS_PER_SOURCE];
static uint16_t g_key_position_pm[CH585_SCAN_KEYS_PER_SOURCE];
static uint16_t g_key_peak_pm[CH585_SCAN_KEYS_PER_SOURCE];
static uint16_t g_key_valley_pm[CH585_SCAN_KEYS_PER_SOURCE];
static uint8_t g_last_cmd_valid;
static uint8_t g_last_cmd_id;
static uint8_t g_last_cmd_host_seq;
static uint8_t g_last_cmd_error;
static uint8_t g_invalid_cmd_count;
static uint8_t g_last_cmd_invalid_reason;
static uint8_t g_last_cmd_raw_magic;
static uint8_t g_last_cmd_raw_cmd;
static uint8_t g_last_cmd_raw_host_seq;
static uint8_t g_last_cmd_raw_ack_seq;
static uint8_t g_bad_cmd_raw_magic;
static uint8_t g_bad_cmd_raw_cmd;
static uint8_t g_bad_cmd_raw_host_seq;
static uint8_t g_bad_cmd_raw_ack_seq;
static uint8_t g_bad_cmd_invalid_reason;
static uint16_t g_last_cmd_rx_crc;
static uint16_t g_last_cmd_expected_crc;

#if CH585_SPI_UART_TELEMETRY
static uint32_t g_spi_tlm_frames;
static uint32_t g_spi_tlm_cmd_ok;
static uint32_t g_spi_tlm_cmd_bad;
static uint32_t g_spi_tlm_ack_ok;
static uint32_t g_spi_tlm_ack_miss;
static uint8_t g_spi_tlm_have_sent;
static uint8_t g_spi_tlm_last_sent_seq;
static uint8_t g_spi_tlm_last_host_seq;
static uint8_t g_spi_tlm_last_host_ack_seq;
static uint8_t g_spi_tlm_last_ack_ok;
static uint8_t g_spi_tlm_last_cmd;
static uint8_t g_spi_tlm_last_invalid_reason;
static uint16_t g_spi_tlm_last_sck_10khz;
static uint8_t g_spi_tlm_prev_down[CH585_SCAN_DOWN_BYTES];
static uint8_t g_spi_tlm_have_down;
#endif

static void update_key_state_from_adc(uint8_t key_id, uint16_t adc);

#if CH585_ADC_PROBE_MODE
typedef struct
{
    uint8_t adc_index;
} ch585_adc_probe_ads_user_t;

static ads7948_t g_adc_probe_ads[2];
static ch585_adc_probe_ads_user_t g_adc_probe_ads_user[2] = {
    {0U},
    {1U},
};
static uint8_t g_adc_probe_initialized;
static uint8_t g_adc_probe_scan_cursor;
static uint8_t g_adc_probe_fast_key_valid;
static uint8_t g_adc_probe_fast_key_id;

typedef struct
{
    uint8_t hall_id;
    uint8_t key_id;
    const char *label;
} ch585_adc_probe_key_map_t;

#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP
static const ch585_adc_probe_key_map_t g_adc_probe_right_key_map[] = {
    {1U, 0U, "F12"},
    {2U, 1U, "F11"},
    {3U, 2U, "F10"},
    {4U, 3U, "F9"},
    {5U, 4U, "F8"},
    {6U, 5U, "F7"},
    {7U, 6U, "F6"},
    {8U, 7U, "Backspace"},
    {9U, 8U, "Equal"},
    {10U, 9U, "Minus"},
    {11U, 16U, "0"},
    {12U, 17U, "9"},
    {13U, 18U, "8"},
    {14U, 19U, "7"},
    {15U, 20U, "Backslash"},
    {16U, 21U, "RBracket"},
    {17U, 22U, "LBracket"},
    {18U, 23U, "P"},
    {19U, 24U, "O"},
    {20U, 25U, "I"},
    {21U, 32U, "U"},
    {22U, 33U, "Y"},
    {23U, 34U, "Enter"},
    {24U, 35U, "Quote"},
    {25U, 36U, "Semicolon"},
    {26U, 37U, "L"},
    {27U, 38U, "K"},
    {28U, 39U, "J"},
    {29U, 40U, "H"},
    {30U, 41U, "Shift"},
    {31U, 48U, "Slash"},
    {32U, 49U, "Dot"},
    {33U, 50U, "Comma"},
    {34U, 51U, "M"},
    {35U, 52U, "N"},
    {36U, 53U, "B"},
    {37U, 54U, "Ctrl"},
    {38U, 55U, "Win"},
    {39U, 56U, "Fn"},
    {40U, 57U, "Alt"},
    {41U, 58U, "Space"},
};

#define CH585_ADC_PROBE_RIGHT_KEY_COUNT \
    ((uint8_t)(sizeof(g_adc_probe_right_key_map) / \
               sizeof(g_adc_probe_right_key_map[0])))
#endif

#if CH585_ADC_PROBE_USE_LEFT_KEYMAP
static const ch585_adc_probe_key_map_t g_adc_probe_left_key_map[] = {
    {42U, 0U, "F5"},
    {43U, 1U, "F4"},
    {44U, 2U, "F3"},
    {45U, 3U, "F2"},
    {46U, 4U, "F1"},
    {47U, 5U, "Esc"},
    {48U, 6U, "6"},
    {49U, 7U, "5"},
    {50U, 8U, "4"},
    {51U, 16U, "3"},
    {52U, 17U, "2"},
    {53U, 18U, "1"},
    {54U, 19U, "Grave"},
    {55U, 20U, "Y"},
    {56U, 21U, "T"},
    {57U, 22U, "R"},
    {58U, 23U, "E"},
    {59U, 24U, "W"},
    {60U, 32U, "Q"},
    {61U, 33U, "Tab"},
    {62U, 34U, "G"},
    {63U, 35U, "F"},
    {64U, 36U, "D"},
    {65U, 37U, "S"},
    {66U, 38U, "A"},
    {67U, 39U, "Caps"},
    {68U, 40U, "B"},
    {69U, 48U, "V"},
    {70U, 49U, "C"},
    {71U, 50U, "X"},
    {72U, 51U, "Z"},
    {73U, 52U, "Shift"},
    {74U, 53U, "Space"},
    {75U, 54U, "Alt"},
    {76U, 55U, "Win"},
    {77U, 56U, "Ctrl"},
};

#define CH585_ADC_PROBE_LEFT_KEY_COUNT \
    ((uint8_t)(sizeof(g_adc_probe_left_key_map) / \
               sizeof(g_adc_probe_left_key_map[0])))
#endif

#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP || CH585_ADC_PROBE_USE_LEFT_KEYMAP
static uint8_t ch585_adc_probe_active_map_count(void)
{
#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP
    return CH585_ADC_PROBE_RIGHT_KEY_COUNT;
#else
    return CH585_ADC_PROBE_LEFT_KEY_COUNT;
#endif
}

static const ch585_adc_probe_key_map_t *ch585_adc_probe_active_map_by_index(uint8_t index)
{
#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP
    if (index >= CH585_ADC_PROBE_RIGHT_KEY_COUNT)
    {
        return NULL;
    }
    return &g_adc_probe_right_key_map[index];
#else
    if (index >= CH585_ADC_PROBE_LEFT_KEY_COUNT)
    {
        return NULL;
    }
    return &g_adc_probe_left_key_map[index];
#endif
}

static const ch585_adc_probe_key_map_t *ch585_adc_probe_active_map_by_key(uint8_t key_id)
{
    uint8_t i;
    uint8_t count = ch585_adc_probe_active_map_count();

    for (i = 0U; i < count; i++)
    {
        const ch585_adc_probe_key_map_t *map =
            ch585_adc_probe_active_map_by_index(i);
        if ((map != NULL) && (map->key_id == key_id))
        {
            return map;
        }
    }

    return NULL;
}
#endif

static void ch585_adc_probe_delay_us(uint32_t us, void *user)
{
    (void)user;

    while (us > 60000U)
    {
        mDelayuS(60000U);
        us -= 60000U;
    }
    if (us != 0U)
    {
        mDelayuS((uint16_t)us);
    }
}

static void ch585_adc_probe_set_pden(uint8_t enabled)
{
#if CH585_ADC_PROBE_PDEN_ENABLE_LEVEL
    if (enabled != 0U)
    {
        GPIOB_SetBits(GPIO_Pin_19);
    }
    else
    {
        GPIOB_ResetBits(GPIO_Pin_19);
    }
#else
    if (enabled != 0U)
    {
        GPIOB_ResetBits(GPIO_Pin_19);
    }
    else
    {
        GPIOB_SetBits(GPIO_Pin_19);
    }
#endif
}

static uint32_t ch585_adc_probe_cs_pin(uint8_t adc_index)
{
#if CH585_ADC_PROBE_RIGHT_HALF
    return (adc_index == 0U) ? GPIO_Pin_15 : GPIO_Pin_14;
#else
    return (adc_index == 0U) ? GPIO_Pin_14 : GPIO_Pin_15;
#endif
}

static void ch585_adc_probe_set_cs(uint8_t level, void *user)
{
    ch585_adc_probe_ads_user_t *ctx = (ch585_adc_probe_ads_user_t *)user;
    uint32_t pin = ch585_adc_probe_cs_pin(ctx->adc_index);

    if (level != 0U)
    {
        GPIOB_SetBits(pin);
    }
    else
    {
        GPIOB_ResetBits(pin);
    }
}

static void ch585_adc_probe_set_ch_sel(uint8_t level, void *user)
{
    (void)user;

    if (level != 0U)
    {
        GPIOB_SetBits(GPIO_Pin_18);
    }
    else
    {
        GPIOB_ResetBits(GPIO_Pin_18);
    }
}

static int ch585_adc_probe_read16(uint16_t *rx_word, void *user)
{
    uint8_t hi;
    uint8_t lo;

    (void)user;

    if (rx_word == NULL)
    {
        return -1;
    }

    hi = SPI1_MasterRecvByte();
    lo = SPI1_MasterRecvByte();
    *rx_word = (uint16_t)(((uint16_t)hi << 8) | lo);

    return 0;
}

static void ch585_adc_probe_set_mux(uint8_t mux_channel)
{
    uint32_t pins = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    uint32_t value = 0U;

    if ((mux_channel & 0x01U) != 0U)
    {
        value |= GPIO_Pin_0;
    }
    if ((mux_channel & 0x02U) != 0U)
    {
        value |= GPIO_Pin_1;
    }
    if ((mux_channel & 0x04U) != 0U)
    {
        value |= GPIO_Pin_2;
    }
    if ((mux_channel & 0x08U) != 0U)
    {
        value |= GPIO_Pin_3;
    }

    GPIOB_ResetBits(pins);
    if (value != 0U)
    {
        GPIOB_SetBits(value);
    }
}

#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP || CH585_ADC_PROBE_USE_LEFT_KEYMAP
static uint8_t ch585_adc_probe_key_is_enabled(uint8_t key_id)
{
    return (ch585_adc_probe_active_map_by_key(key_id) != NULL) ? 1U : 0U;
}
#else
static uint8_t ch585_adc_probe_key_is_enabled(uint8_t key_id)
{
    return (key_id < CH585_ADC_PROBE_KEY_COUNT) ? 1U : 0U;
}
#endif

static void ch585_adc_probe_init(void)
{
    ads7948_config_t cfg;
    uint8_t i;

    if (g_adc_probe_initialized != 0U)
    {
        return;
    }

    GPIOA_ModeCfg(GPIO_Pin_0 | GPIO_Pin_1, GPIO_ModeOut_PP_20mA);
    GPIOA_ModeCfg(GPIO_Pin_2, GPIO_ModeIN_PU);

    GPIOB_SetBits(GPIO_Pin_14 | GPIO_Pin_15);
    GPIOB_ModeCfg(GPIO_Pin_14 | GPIO_Pin_15, GPIO_ModeOut_PP_20mA);
    GPIOB_ResetBits(GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3);
    GPIOB_ModeCfg(GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3,
                  GPIO_ModeOut_PP_20mA);
    GPIOB_ModeCfg(GPIO_Pin_18 | GPIO_Pin_19, GPIO_ModeOut_PP_20mA);
    ch585_adc_probe_set_ch_sel(0U, NULL);
    ch585_adc_probe_set_pden(1U);

    SPI1_MasterDefInit();
    SPI1_CLKCfg((uint8_t)CH585_ADC_PROBE_SPI1_DIV);
    SPI1_DataMode(Mode0_HighBitINFront);

    for (i = 0; i < 2U; i++)
    {
        ads7948_default_config(&cfg);
        cfg.set_cs = ch585_adc_probe_set_cs;
        cfg.set_ch_sel = ch585_adc_probe_set_ch_sel;
        cfg.read16 = ch585_adc_probe_read16;
        cfg.delay_us = ch585_adc_probe_delay_us;
        cfg.user = &g_adc_probe_ads_user[i];
        cfg.input_settle_us = (uint16_t)CH585_ADC_PROBE_SETTLE_US;
        cfg.cs_high_us = (uint16_t)CH585_ADC_PROBE_CS_HIGH_US;
        cfg.discard_frames = (uint8_t)CH585_ADC_PROBE_DISCARD_FRAMES;
        (void)ads7948_init(&g_adc_probe_ads[i], &cfg);
    }

    g_adc_probe_initialized = 1U;
}

static uint8_t ch585_adc_probe_pick_key(uint8_t requested_key)
{
    uint8_t key_id = (uint8_t)CH585_ADC_PROBE_DEBUG_KEY;

    (void)requested_key;

    if (key_id == CH585_ADC_PROBE_SCAN_ALL_KEY)
    {
#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP || CH585_ADC_PROBE_USE_LEFT_KEYMAP
        const ch585_adc_probe_key_map_t *map;
        uint8_t map_count = ch585_adc_probe_active_map_count();

        if (g_adc_probe_scan_cursor >= map_count)
        {
            g_adc_probe_scan_cursor = 0U;
        }
        map = ch585_adc_probe_active_map_by_index(g_adc_probe_scan_cursor);
        g_adc_probe_scan_cursor++;
        if (map != NULL)
        {
            return map->key_id;
        }
#else
        key_id = g_adc_probe_scan_cursor;
        g_adc_probe_scan_cursor++;
        if (g_adc_probe_scan_cursor >= CH585_ADC_PROBE_KEY_COUNT)
        {
            g_adc_probe_scan_cursor = 0U;
        }
        return key_id;
#endif
    }

    if (key_id >= CH585_ADC_PROBE_KEY_COUNT)
    {
        key_id = 0U;
    }

#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP || CH585_ADC_PROBE_USE_LEFT_KEYMAP
    if (ch585_adc_probe_key_is_enabled(key_id) == 0U)
    {
        const ch585_adc_probe_key_map_t *map =
            ch585_adc_probe_active_map_by_index(0U);
        key_id = (map != NULL) ? map->key_id : 0U;
    }
#endif

    return key_id;
}

static int ch585_adc_probe_sample_key(uint8_t key_id,
                                      uint16_t *code,
                                      uint16_t *word)
{
    uint8_t lane;
    uint8_t mux_channel;
    uint8_t adc_index;
    uint8_t adc_channel;
    int status;

    if ((code == NULL) || (word == NULL) || (key_id >= CH585_ADC_PROBE_KEY_COUNT))
    {
        return -1;
    }

    ch585_adc_probe_init();

    lane = (uint8_t)(key_id / CH585_ADC_PROBE_MUX_COUNT);
    mux_channel = (uint8_t)(key_id % CH585_ADC_PROBE_MUX_COUNT);
    adc_index = (uint8_t)(lane / ADS7948_CHANNEL_COUNT);
    adc_channel = (uint8_t)(lane % ADS7948_CHANNEL_COUNT);

#if CH585_ADC_PROBE_FAST_REPEAT
    if ((g_adc_probe_fast_key_valid != 0U) &&
        (g_adc_probe_fast_key_id == key_id))
    {
        status = ads7948_read_frame(&g_adc_probe_ads[adc_index], word);
        if (status == ADS7948_STATUS_OK)
        {
            *code = ads7948_decode_10bit(*word);
        }
        return status;
    }
#endif

    ch585_adc_probe_set_mux(mux_channel);
    status = ads7948_read_channel(&g_adc_probe_ads[adc_index], adc_channel, code);
    *word = g_adc_probe_ads[adc_index].last_word;
#if CH585_ADC_PROBE_FAST_REPEAT
    if (status == ADS7948_STATUS_OK)
    {
        g_adc_probe_fast_key_valid = 1U;
        g_adc_probe_fast_key_id = key_id;
    }
    else
    {
        g_adc_probe_fast_key_valid = 0U;
    }
#endif

    return status;
}

static int ch585_adc_probe_update_key_state(uint8_t key_id,
                                            uint16_t *code,
                                            uint16_t *word)
{
    uint16_t local_code = 0xFFFFU;
    uint16_t local_word = 0U;
    int status;

    if (key_id >= CH585_ADC_PROBE_KEY_COUNT)
    {
        return -1;
    }

    status = ch585_adc_probe_sample_key(key_id, &local_code, &local_word);
    if ((status == 0) && (local_code <= ADS7948_CODE_MAX))
    {
        update_key_state_from_adc(key_id, local_code);
    }

    if (code != NULL)
    {
        *code = local_code;
    }
    if (word != NULL)
    {
        *word = local_word;
    }

    return status;
}
#endif

static uint16_t scan_crc16(const uint8_t *data, uint16_t len)
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

static void key_config_init_defaults(void)
{
    uint8_t i;

    for (i = 0; i < CH585_SCAN_KEYS_PER_SOURCE; i++)
    {
        ch585_key_config_t *cfg = &g_key_config[i];

        cfg->released_adc = CH585_SIM_RELEASED_ADC;
        cfg->pressed_adc = CH585_SIM_PRESSED_ADC;
        cfg->min_adc = CH585_KEY_DEFAULT_MIN_ADC;
        cfg->max_adc = CH585_KEY_DEFAULT_MAX_ADC;
        cfg->press_position_pm = CH585_KEY_DEFAULT_PRESS_POSITION_PM;
        cfg->release_position_pm = CH585_KEY_DEFAULT_RELEASE_POSITION_PM;
        cfg->rt_press_delta_pm = CH585_KEY_DEFAULT_RT_PRESS_DELTA_PM;
        cfg->rt_release_delta_pm = CH585_KEY_DEFAULT_RT_RELEASE_DELTA_PM;
        cfg->global_key_id = (uint16_t)i;
        cfg->filter_shift = CH585_KEY_DEFAULT_FILTER_SHIFT;
#if CH585_KEY_ENABLE_RAPID_TRIGGER
        cfg->rt_enable = 1U;
#else
        cfg->rt_enable = 0U;
#endif
        cfg->valid = 1U;

        g_key_raw_adc[i] = cfg->released_adc;
        g_key_filtered_adc[i] = cfg->released_adc;
        g_key_filtered_q8[i] = (uint32_t)cfg->released_adc << 8;
        g_key_position_pm[i] = 0U;
        g_key_peak_pm[i] = 0U;
        g_key_valley_pm[i] = 0U;
    }
}

static const ch585_key_config_t *key_config(uint8_t key_id)
{
    if (key_id >= CH585_SCAN_KEYS_PER_SOURCE)
    {
        return &g_key_config[0];
    }

    return &g_key_config[key_id];
}

#if CH585_MODE_PIPELINE_SHORT || CH585_MODE_REQUEST_ONLY_SHORT || CH585_MODE_COMMAND_RESPONSE
static void key_runtime_reset(uint8_t key_id)
{
    const ch585_key_config_t *cfg = key_config(key_id);

    if (key_id >= CH585_SCAN_KEYS_PER_SOURCE)
    {
        return;
    }

    g_key_down[key_id] = 0U;
    g_key_filter_valid[key_id] = 0U;
    g_key_rt_armed[key_id] = 0U;
    g_key_raw_adc[key_id] = cfg->released_adc;
    g_key_filtered_adc[key_id] = cfg->released_adc;
    g_key_filtered_q8[key_id] = (uint32_t)cfg->released_adc << 8;
    g_key_position_pm[key_id] = 0U;
    g_key_peak_pm[key_id] = 0U;
    g_key_valley_pm[key_id] = 0U;
}

#if CH585_ADC_PROBE_MODE
static void ch585_adc_probe_apply_calibration(void)
{
#if CH585_ADC_PROBE_CAL_ENABLE
    uint8_t i;

    for (i = 0U; i < CH585_SCAN_KEYS_PER_SOURCE; i++)
    {
        g_key_config[i].valid = 0U;
        key_runtime_reset(i);
    }

#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP || CH585_ADC_PROBE_USE_LEFT_KEYMAP
    for (i = 0U; i < ch585_adc_probe_active_map_count(); i++)
    {
        const ch585_adc_probe_key_map_t *map =
            ch585_adc_probe_active_map_by_index(i);
        ch585_key_config_t *cfg;

        if (map == NULL)
        {
            continue;
        }

        cfg = &g_key_config[map->key_id];

        cfg->released_adc = (uint16_t)CH585_ADC_PROBE_CAL_RELEASED_ADC;
        cfg->pressed_adc = (uint16_t)CH585_ADC_PROBE_CAL_PRESSED_ADC;
        cfg->min_adc = CH585_KEY_DEFAULT_MIN_ADC;
        cfg->max_adc = CH585_KEY_DEFAULT_MAX_ADC;
        cfg->press_position_pm =
            (uint16_t)CH585_ADC_PROBE_CAL_PRESS_POSITION_PM;
        cfg->release_position_pm =
            (uint16_t)CH585_ADC_PROBE_CAL_RELEASE_POSITION_PM;
        cfg->filter_shift = (uint8_t)CH585_ADC_PROBE_CAL_FILTER_SHIFT;
        cfg->rt_enable = (uint8_t)CH585_ADC_PROBE_CAL_RT_ENABLE;
        cfg->global_key_id = map->hall_id;
        cfg->valid = 1U;

        key_runtime_reset(map->key_id);
    }
#else
    uint8_t key_id = (uint8_t)CH585_ADC_PROBE_DEBUG_KEY;
    ch585_key_config_t *cfg;

    if ((key_id == CH585_ADC_PROBE_SCAN_ALL_KEY) ||
        (key_id >= CH585_SCAN_KEYS_PER_SOURCE))
    {
        return;
    }

    cfg = &g_key_config[key_id];
    cfg->released_adc = (uint16_t)CH585_ADC_PROBE_CAL_RELEASED_ADC;
    cfg->pressed_adc = (uint16_t)CH585_ADC_PROBE_CAL_PRESSED_ADC;
    cfg->min_adc = CH585_KEY_DEFAULT_MIN_ADC;
    cfg->max_adc = CH585_KEY_DEFAULT_MAX_ADC;
    cfg->press_position_pm =
        (uint16_t)CH585_ADC_PROBE_CAL_PRESS_POSITION_PM;
    cfg->release_position_pm =
        (uint16_t)CH585_ADC_PROBE_CAL_RELEASE_POSITION_PM;
    cfg->filter_shift = (uint8_t)CH585_ADC_PROBE_CAL_FILTER_SHIFT;
    cfg->rt_enable = (uint8_t)CH585_ADC_PROBE_CAL_RT_ENABLE;
    cfg->valid = 1U;

    key_runtime_reset(key_id);
#endif
#endif
}
#endif

static uint8_t scan_cmd_id_is_supported(uint8_t cmd)
{
    switch (cmd)
    {
    case CH585_SCAN_CMD_GET_STATE:
    case CH585_SCAN_CMD_GET_DEBUG:
    case CH585_SCAN_CMD_GET_CONFIG:
    case CH585_SCAN_CMD_SET_CONFIG:
    case CH585_SCAN_CMD_CALIBRATE_KEY:
    case CH585_SCAN_CMD_CALIBRATE_ALL:
        return 1U;
    default:
        return 0U;
    }
}

static uint8_t key_config_set_u16(uint8_t key_id, uint8_t param_id, uint16_t value)
{
    ch585_key_config_t *cfg;

    if (key_id >= CH585_SCAN_KEYS_PER_SOURCE)
    {
        return 0U;
    }

    cfg = &g_key_config[key_id];

    switch (param_id)
    {
    case CH585_SCAN_CFG_RELEASED_ADC:
        cfg->released_adc = value;
        break;
    case CH585_SCAN_CFG_PRESSED_ADC:
        cfg->pressed_adc = value;
        break;
    case CH585_SCAN_CFG_MIN_ADC:
        cfg->min_adc = value;
        break;
    case CH585_SCAN_CFG_MAX_ADC:
        cfg->max_adc = value;
        break;
    case CH585_SCAN_CFG_PRESS_POSITION:
        cfg->press_position_pm = (value > 1000U) ? 1000U : value;
        break;
    case CH585_SCAN_CFG_RELEASE_POSITION:
        cfg->release_position_pm = (value > 1000U) ? 1000U : value;
        break;
    case CH585_SCAN_CFG_RT_PRESS_DELTA:
        cfg->rt_press_delta_pm = (value > 1000U) ? 1000U : value;
        break;
    case CH585_SCAN_CFG_RT_RELEASE_DELTA:
        cfg->rt_release_delta_pm = (value > 1000U) ? 1000U : value;
        break;
    case CH585_SCAN_CFG_FILTER_SHIFT:
        cfg->filter_shift = (value > 15U) ? 15U : (uint8_t)value;
        break;
    case CH585_SCAN_CFG_RT_ENABLE:
        cfg->rt_enable = (value != 0U) ? 1U : 0U;
        break;
    case CH585_SCAN_CFG_VALID:
        cfg->valid = (value != 0U) ? 1U : 0U;
        break;
    case CH585_SCAN_CFG_GLOBAL_KEY_ID:
        cfg->global_key_id = value;
        break;
    default:
        return 0U;
    }

    key_runtime_reset(key_id);
    return 1U;
}

static uint16_t scan_cmd_apply(const ch585_scan_wire_cmd_t *cmd)
{
    if (cmd == NULL)
    {
        return CH585_SCAN_FLAG_CMD_ERROR;
    }

    switch (cmd->cmd)
    {
    case CH585_SCAN_CMD_GET_STATE:
    case CH585_SCAN_CMD_GET_DEBUG:
    case CH585_SCAN_CMD_GET_CONFIG:
        return 0U;
    case CH585_SCAN_CMD_SET_CONFIG:
#if CH585_USE_SHORT_FRAME
        if (key_config_set_u16(cmd->target_key, cmd->param_id, cmd->value) == 0U)
        {
            return CH585_SCAN_FLAG_CMD_ERROR;
        }
        return 0U;
#else
        return CH585_SCAN_FLAG_CMD_ERROR;
#endif
    case CH585_SCAN_CMD_CALIBRATE_KEY:
#if CH585_USE_SHORT_FRAME
        if (cmd->target_key >= CH585_SCAN_KEYS_PER_SOURCE)
        {
            return CH585_SCAN_FLAG_CMD_ERROR;
        }
        key_runtime_reset(cmd->target_key);
        return 0U;
#else
        return CH585_SCAN_FLAG_CMD_ERROR;
#endif
    case CH585_SCAN_CMD_CALIBRATE_ALL:
        key_config_init_defaults();
        return 0U;
    default:
        return CH585_SCAN_FLAG_CMD_ERROR;
    }
}
#endif

static uint16_t sim_clamp_adc(int32_t value)
{
    if (value < 0)
    {
        return 0U;
    }

    if (value > 4095)
    {
        return 4095U;
    }

    return (uint16_t)value;
}

static uint16_t sim_key_position_pm(uint16_t seq, uint8_t key_id)
{
    uint16_t phase;

    if (key_id >= CH585_SIM_ACTIVE_KEYS)
    {
        return 0U;
    }

    phase = (uint16_t)((seq + ((uint16_t)key_id * 8U)) &
                       (CH585_SIM_PERIOD_FRAMES - 1U));

    if (phase < 8U)
    {
        return (uint16_t)((phase * 1000U) / 7U);
    }

    if (phase < 16U)
    {
        return 1000U;
    }

    if (phase < 24U)
    {
        return (uint16_t)(((23U - phase) * 1000U) / 7U);
    }

    return 0U;
}

static uint16_t sim_adc_value(uint16_t seq, uint8_t key_id)
{
    const ch585_key_config_t *cfg = key_config(key_id);
    uint16_t position_pm = sim_key_position_pm(seq, key_id);
    int32_t span = (int32_t)cfg->pressed_adc - (int32_t)cfg->released_adc;
    int16_t noise = (int16_t)(((seq * 17U) + ((uint16_t)key_id * 13U)) & 7U) - 3;
    int32_t value;

    if (cfg->valid == 0U)
    {
        return cfg->released_adc;
    }

    value = (int32_t)cfg->released_adc +
                    ((span * (int32_t)position_pm) / 1000) +
                    (int32_t)noise;

    return sim_clamp_adc(value);
}

static void key_state_set_bit(ch585_scan_wire_frame_t *frame, uint8_t key_id)
{
    frame->down_bits[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
}

static uint16_t key_adc_to_position_pm(uint8_t key_id, uint16_t adc)
{
    const ch585_key_config_t *cfg = key_config(key_id);
    int32_t span = (int32_t)cfg->pressed_adc - (int32_t)cfg->released_adc;
    int32_t pos;

    if ((cfg->valid == 0U) || (span == 0))
    {
        return 0U;
    }

    pos = (((int32_t)adc - (int32_t)cfg->released_adc) * 1000) / span;
    if (pos < 0)
    {
        return 0U;
    }

    if (pos > 1000)
    {
        return 1000U;
    }

    return (uint16_t)pos;
}

static uint16_t key_filter_adc(uint8_t key_id, uint16_t adc)
{
    const ch585_key_config_t *cfg = key_config(key_id);
    uint32_t raw_q8 = (uint32_t)adc << 8;
    uint32_t filtered;
    uint8_t shift = cfg->filter_shift;

    if (shift > 15U)
    {
        shift = 15U;
    }

    if (g_key_filter_valid[key_id] == 0U)
    {
        g_key_filtered_q8[key_id] = raw_q8;
        g_key_filter_valid[key_id] = 1U;
    }
    else
    {
        filtered = g_key_filtered_q8[key_id];
        if (raw_q8 >= filtered)
        {
            filtered += (raw_q8 - filtered) >> shift;
        }
        else
        {
            filtered -= (filtered - raw_q8) >> shift;
        }
        g_key_filtered_q8[key_id] = filtered;
    }

    return (uint16_t)((g_key_filtered_q8[key_id] + 128U) >> 8);
}

static void key_press(uint8_t key_id, uint16_t position_pm)
{
    g_key_down[key_id] = 1U;
    g_key_rt_armed[key_id] = 0U;
    g_key_peak_pm[key_id] = position_pm;
    g_key_valley_pm[key_id] = position_pm;
}

static void key_release(uint8_t key_id, uint16_t position_pm, uint8_t rt_armed)
{
    g_key_down[key_id] = 0U;
    g_key_rt_armed[key_id] = rt_armed;
    g_key_peak_pm[key_id] = position_pm;
    g_key_valley_pm[key_id] = position_pm;
}

static void update_key_state_from_adc(uint8_t key_id, uint16_t adc)
{
    const ch585_key_config_t *cfg = key_config(key_id);
    uint16_t filtered_adc = key_filter_adc(key_id, adc);
    uint16_t position_pm = key_adc_to_position_pm(key_id, filtered_adc);

    if (cfg->valid == 0U)
    {
        g_key_down[key_id] = 0U;
        g_key_rt_armed[key_id] = 0U;
        g_key_raw_adc[key_id] = adc;
        g_key_filtered_adc[key_id] = filtered_adc;
        g_key_position_pm[key_id] = 0U;
        return;
    }

    g_key_raw_adc[key_id] = adc;
    g_key_filtered_adc[key_id] = filtered_adc;
    g_key_position_pm[key_id] = position_pm;

    if (g_key_down[key_id] == 0U)
    {
        if (position_pm < g_key_valley_pm[key_id])
        {
            g_key_valley_pm[key_id] = position_pm;
        }

#if CH585_KEY_ENABLE_RAPID_TRIGGER
        if ((cfg->rt_enable != 0U) &&
            (g_key_rt_armed[key_id] != 0U) &&
            (position_pm >=
             (uint16_t)(g_key_valley_pm[key_id] + cfg->rt_press_delta_pm)))
        {
            key_press(key_id, position_pm);
            return;
        }
#endif

        if (position_pm >= cfg->press_position_pm)
        {
            key_press(key_id, position_pm);
        }
    }
    else
    {
        if (position_pm > g_key_peak_pm[key_id])
        {
            g_key_peak_pm[key_id] = position_pm;
        }

#if CH585_KEY_ENABLE_RAPID_TRIGGER
        if ((cfg->rt_enable != 0U) &&
            ((position_pm + cfg->rt_release_delta_pm) <= g_key_peak_pm[key_id]))
        {
            key_release(key_id, position_pm, 1U);
            return;
        }
#endif

        if (position_pm <= cfg->release_position_pm)
        {
            key_release(key_id, position_pm, 0U);
        }
    }
}

#if CH585_ADC_PROBE_MODE
static int ch585_adc_probe_update_enabled_keys(void)
{
    int first_status = 0;

#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP || CH585_ADC_PROBE_USE_LEFT_KEYMAP
    uint8_t i;
    uint8_t map_count = ch585_adc_probe_active_map_count();

    for (i = 0U; i < map_count; i++)
    {
        const ch585_adc_probe_key_map_t *map =
            ch585_adc_probe_active_map_by_index(i);
        uint8_t key_id;
        int status;

        if (map == NULL)
        {
            continue;
        }
        key_id = map->key_id;
        status = ch585_adc_probe_update_key_state(key_id, NULL, NULL);
        if ((status != 0) && (first_status == 0))
        {
            first_status = status;
        }
    }
#else
    uint8_t key_id;

    for (key_id = 0U; key_id < CH585_ADC_PROBE_KEY_COUNT; key_id++)
    {
        int status = ch585_adc_probe_update_key_state(key_id, NULL, NULL);

        if ((status != 0) && (first_status == 0))
        {
            first_status = status;
        }
    }
#endif

    return first_status;
}
#endif

static void update_all_key_states(uint16_t seq)
{
    uint8_t i;

    for (i = 0; i < CH585_SCAN_KEYS_PER_SOURCE; i++)
    {
        update_key_state_from_adc(i, sim_adc_value(seq, i));
    }
}

#if CH585_MODE_PIPELINE_SHORT || CH585_MODE_REQUEST_ONLY_SHORT || CH585_MODE_COMMAND_RESPONSE
static uint8_t scan_cmd_is_valid(const ch585_scan_wire_cmd_t *cmd)
{
    uint16_t expected_crc;

    g_last_cmd_rx_crc = (cmd != NULL) ? cmd->crc16 : 0U;
    g_last_cmd_expected_crc = 0U;
    g_last_cmd_invalid_reason = 0U;
    g_last_cmd_raw_magic = (cmd != NULL) ? cmd->magic : 0U;
    g_last_cmd_raw_cmd = (cmd != NULL) ? cmd->cmd : 0U;
    g_last_cmd_raw_host_seq = (cmd != NULL) ? cmd->host_seq : 0U;
    g_last_cmd_raw_ack_seq = (cmd != NULL) ? cmd->ack_seq : 0U;

#if CH585_USE_SHORT_FRAME
    if (cmd->magic != CH585_SCAN_SHORT_CMD_MAGIC)
    {
        g_last_cmd_invalid_reason = 1U;
        return 0U;
    }
    if (scan_cmd_id_is_supported(cmd->cmd) == 0U)
    {
        g_last_cmd_invalid_reason = 2U;
        return 0U;
    }
#else
    if (cmd->magic != CH585_SCAN_CMD_MAGIC)
    {
        g_last_cmd_invalid_reason = 1U;
        return 0U;
    }
    if (cmd->version != CH585_SCAN_FRAME_VERSION)
    {
        g_last_cmd_invalid_reason = 2U;
        return 0U;
    }
    if (scan_cmd_id_is_supported(cmd->cmd) == 0U)
    {
        g_last_cmd_invalid_reason = 2U;
        return 0U;
    }
#endif

    expected_crc = scan_crc16((const uint8_t *)cmd,
                              (uint16_t)offsetof(ch585_scan_wire_cmd_t, crc16));
    g_last_cmd_expected_crc = expected_crc;
    if (cmd->crc16 != expected_crc)
    {
        g_last_cmd_invalid_reason = 3U;
        return 0U;
    }

    return 1U;
}
#endif

static void build_scan_frame_into(ch585_scan_wire_frame_t *frame,
                                  uint16_t seq,
                                  uint16_t flags,
                                  uint16_t ack_seq)
{
    uint8_t i;

    memset(frame, 0, sizeof(*frame));

#if CH585_USE_SHORT_FRAME
    (void)flags;
    frame->magic = CH585_SCAN_SHORT_FRAME_MAGIC;
    frame->type = CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE;
    frame->source_id = CH585_SCAN_SOURCE_ID;
    frame->seq = (uint8_t)seq;
    frame->flags = CH585_SCAN_SHORT_FLAG_READY;
    frame->ack_seq = (uint8_t)ack_seq;
#else
    frame->magic = CH585_SCAN_FRAME_MAGIC;
    frame->version = CH585_SCAN_FRAME_VERSION;
    frame->type = CH585_SCAN_FRAME_TYPE_KEY_STATE;
    frame->source_id = CH585_SCAN_SOURCE_ID;
    frame->key_count = CH585_SCAN_KEYS_PER_SOURCE;
    frame->seq = seq;
    frame->flags = flags;
    frame->ack_seq = ack_seq;
#endif

#if CH585_FAST_SIM_FRAME
    (void)i;
    frame->down_bits[0] = (uint8_t)(1U << ((seq >> 3U) & 7U));
    frame->down_bits[1] = (uint8_t)(1U << ((seq >> 4U) & 7U));
#else
#if CH585_ADC_PROBE_MODE
    {
        uint8_t probe_key = (uint8_t)CH585_ADC_PROBE_DEBUG_KEY;
        int probe_status;

        if ((probe_key != CH585_ADC_PROBE_SCAN_ALL_KEY) &&
            (probe_key < CH585_SCAN_KEYS_PER_SOURCE))
        {
            probe_status = ch585_adc_probe_update_key_state(probe_key, NULL, NULL);
            if (probe_status != 0)
            {
#if CH585_USE_SHORT_FRAME
                frame->flags |= CH585_SCAN_SHORT_FLAG_ADC_ERROR;
#else
                frame->flags |= CH585_SCAN_FLAG_CMD_ERROR;
#endif
            }
        }
        else
        {
            probe_status = ch585_adc_probe_update_enabled_keys();
            if (probe_status != 0)
            {
#if CH585_USE_SHORT_FRAME
                frame->flags |= CH585_SCAN_SHORT_FLAG_ADC_ERROR;
#else
                frame->flags |= CH585_SCAN_FLAG_CMD_ERROR;
#endif
            }
        }
    }
#else
    update_all_key_states(seq);
#endif
    for (i = 0; i < CH585_SCAN_KEYS_PER_SOURCE; i++)
    {
        if (g_key_down[i] != 0U)
        {
            key_state_set_bit(frame, i);
        }
    }
#endif

#if CH585_USE_SHORT_FRAME && CH585_SHORT_DIAG_IN_DOWN_BITS && !CH585_ADC_PROBE_MODE
    frame->down_bits[0] = g_bad_cmd_invalid_reason;
    frame->down_bits[1] = g_invalid_cmd_count;
    frame->down_bits[2] = g_bad_cmd_raw_magic;
    frame->down_bits[3] = g_bad_cmd_raw_cmd;
    frame->down_bits[4] = g_bad_cmd_raw_host_seq;
    frame->down_bits[5] = g_bad_cmd_raw_ack_seq;
    frame->down_bits[6] = (uint8_t)((g_last_cmd_id & CH585_SHORT_DIAG_CMD_MASK) |
                                    ((g_last_cmd_valid != 0U) ? CH585_SHORT_DIAG_VALID_FLAG : 0U) |
                                    ((g_last_cmd_error != 0U) ? CH585_SHORT_DIAG_CMD_ERROR_FLAG : 0U));
    frame->down_bits[7] = g_last_cmd_host_seq;
#endif

    frame->crc16 = scan_crc16((const uint8_t *)frame,
                              (uint16_t)offsetof(ch585_scan_wire_frame_t, crc16));
}

#if CH585_USE_SHORT_FRAME && (CH585_MODE_REQUEST_ONLY_SHORT || CH585_MODE_COMMAND_RESPONSE)
static void build_debug_frame_into(ch585_scan_wire_frame_t *frame,
                                   uint16_t seq,
                                   uint8_t key_id)
{
    ch585_scan_debug_short_t *debug = (ch585_scan_debug_short_t *)frame;
    uint8_t debug_flags = CH585_SCAN_SHORT_FLAG_READY;
#if CH585_ADC_PROBE_MODE
    uint16_t code = 0xFFFFU;
    uint16_t word = 0U;
    int probe_status;
#endif

    memset(frame, 0, sizeof(*frame));

#if CH585_ADC_PROBE_MODE
    key_id = ch585_adc_probe_pick_key(key_id);
    probe_status = ch585_adc_probe_update_key_state(key_id, &code, &word);
    if (probe_status != 0)
    {
        debug_flags |= CH585_SCAN_SHORT_FLAG_ADC_ERROR;
    }

    if (g_key_down[key_id] != 0U)
    {
        debug_flags |= CH585_SCAN_SHORT_DEBUG_FLAG_DOWN;
    }
    if (g_key_rt_armed[key_id] != 0U)
    {
        debug_flags |= CH585_SCAN_SHORT_DEBUG_FLAG_RT_ARMED;
    }

    debug->magic = CH585_SCAN_SHORT_FRAME_MAGIC;
    debug->type = CH585_SCAN_SHORT_FRAME_TYPE_KEY_DEBUG;
    debug->source_id = CH585_SCAN_SOURCE_ID;
    debug->seq = (uint8_t)seq;
    debug->key_id = key_id;
    debug->flags = debug_flags;
    debug->raw_adc = code;
    debug->filtered_adc = g_key_filtered_adc[key_id];
    debug->position_pm = g_key_position_pm[key_id];
    debug->peak_pm = word;
    debug->crc16 = scan_crc16((const uint8_t *)debug,
                              (uint16_t)offsetof(ch585_scan_debug_short_t, crc16));
    return;
#else
    update_all_key_states(seq);

    if (key_id >= CH585_SCAN_KEYS_PER_SOURCE)
    {
        key_id = 0U;
    }

    if (g_key_down[key_id] != 0U)
    {
        debug_flags |= CH585_SCAN_SHORT_DEBUG_FLAG_DOWN;
    }
    if (g_key_rt_armed[key_id] != 0U)
    {
        debug_flags |= CH585_SCAN_SHORT_DEBUG_FLAG_RT_ARMED;
    }

    debug->magic = CH585_SCAN_SHORT_FRAME_MAGIC;
    debug->type = CH585_SCAN_SHORT_FRAME_TYPE_KEY_DEBUG;
    debug->source_id = CH585_SCAN_SOURCE_ID;
    debug->seq = (uint8_t)seq;
    debug->key_id = key_id;
    debug->flags = debug_flags;
    debug->raw_adc = g_key_raw_adc[key_id];
    debug->filtered_adc = g_key_filtered_adc[key_id];
    debug->position_pm = g_key_position_pm[key_id];
    debug->peak_pm = g_key_peak_pm[key_id];
    debug->crc16 = scan_crc16((const uint8_t *)debug,
                              (uint16_t)offsetof(ch585_scan_debug_short_t, crc16));
#endif
}
#endif

#if CH585_USE_SHORT_FRAME
#if CH585_MODE_REQUEST_ONLY_SHORT
static void build_scan_or_debug_frame_into(ch585_scan_wire_frame_t *frame,
                                           uint16_t seq,
                                           uint16_t flags,
                                           uint16_t ack_seq)
{
#if CH585_DEBUG_FRAME_INTERVAL != 0
    if ((CH585_DEBUG_FRAME_INTERVAL != 0U) &&
        (seq != 0U) &&
        ((seq % CH585_DEBUG_FRAME_INTERVAL) == 0U))
    {
        uint8_t key_id = (uint8_t)(((uint16_t)(seq / CH585_DEBUG_FRAME_INTERVAL)) %
                                   CH585_SIM_ACTIVE_KEYS);

        (void)flags;
        (void)ack_seq;
        build_debug_frame_into(frame, seq, key_id);
        return;
    }
#endif

    build_scan_frame_into(frame, seq, flags, ack_seq);
}
#endif
#endif

#if CH585_MODE_PIPELINE_SHORT || CH585_MODE_COMMAND_RESPONSE
static void build_scan_frame(uint16_t seq, uint16_t flags, uint16_t ack_seq)
{
    build_scan_frame_into(&g_frame, seq, flags, ack_seq);
}
#endif

#if CH585_MODE_COMMAND_RESPONSE
static void finish_scan_frame(uint16_t flags, uint16_t ack_seq)
{
#if CH585_USE_SHORT_FRAME
    uint8_t short_flags = 0U;

    if ((flags & CH585_SCAN_FLAG_READY) != 0U)
    {
        short_flags |= CH585_SCAN_SHORT_FLAG_READY;
    }
    if ((flags & CH585_SCAN_FLAG_CMD_ERROR) != 0U)
    {
        short_flags |= CH585_SCAN_SHORT_FLAG_CMD_ERROR;
    }

    g_frame.flags = short_flags;
    g_frame.ack_seq = (uint8_t)ack_seq;
#else
    g_frame.flags = flags;
    g_frame.ack_seq = ack_seq;
#endif
    g_frame.crc16 = scan_crc16((const uint8_t *)&g_frame,
                               (uint16_t)offsetof(ch585_scan_wire_frame_t, crc16));
}
#endif

static void spi0_slave_stream_reset(void)
{
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_MODE_SLAVE;
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;
    SPI0_DataMode(Mode0_HighBitINFront);
}

static void spi0_slave_pin_init(void)
{
#if CH585_SPI0_MISO_STRONG_DRIVE
    /*
     * One-CH585 high-speed experiment only: keep PA15/MISO on the strong
     * 20mA output driver. Do not use this unchanged on a shared MISO bus.
     */
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeOut_PP_20mA);
#else
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15, GPIO_ModeIN_PU);
#endif
}

#if CH585_USE_SPI0_SLAVE_DMA
static void spi0_slave_dma_trans_arm(uint8_t *pbuf, uint16_t len)
{
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R32_SPI0_DMA_BEG = (uint32_t)pbuf;
    R32_SPI0_DMA_END = (uint32_t)(pbuf + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_DMA_END |
                       RB_SPI_IF_BYTE_END | RB_SPI_IF_FIFO_OV;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
}

#if CH585_MODE_COMMAND_RESPONSE
static void spi0_slave_dma_recv_arm(uint8_t *pbuf, uint16_t len)
{
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R32_SPI0_DMA_BEG = (uint32_t)pbuf;
    R32_SPI0_DMA_END = (uint32_t)(pbuf + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_DMA_END;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
}
#endif

#if CH585_MODE_COMMAND_RESPONSE || (CH585_MODE_REQUEST_ONLY_SHORT && !CH585_REQUEST_ONLY_CAPTURE_CMD)
static void spi0_slave_dma_wait_done(void)
{
    while ((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) == 0U)
    {
    }
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}
#endif

#if CH585_MODE_REQUEST_ONLY_SHORT && CH585_REQUEST_ONLY_CAPTURE_CMD && CH585_USE_SPI0_SLAVE_DMA
static void spi0_slave_dma_wait_done_capture(uint8_t *rx, uint16_t len)
{
    uint16_t rxi = 0U;

    if (rx != NULL)
    {
        memset(rx, 0, len);
    }

    while ((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) == 0U)
    {
        if ((R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END) != 0U)
        {
            if ((rx != NULL) && (rxi < len))
            {
                rx[rxi] = R8_SPI0_BUFFER;
                rxi++;
            }
            else
            {
                (void)R8_SPI0_BUFFER;
            }
            R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
        }
    }

    while ((R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END) != 0U)
    {
        if ((rx != NULL) && (rxi < len))
        {
            rx[rxi] = R8_SPI0_BUFFER;
            rxi++;
        }
        else
        {
            (void)R8_SPI0_BUFFER;
        }
        R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
    }

    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}
#endif
#endif

static void spi0_wait_cs_high(void)
{
    while ((R8_SPI0_RUN_FLAG & RB_SPI_SLV_SELECT) != 0U)
    {
    }
}

static void spi0_wait_cs_low(void)
{
    while ((R8_SPI0_RUN_FLAG & RB_SPI_SLV_SELECT) == 0U)
    {
    }
}

#if CH585_USE_PIPELINE_SHORT && CH585_USE_SHORT_FRAME
static void spi0_slave_pipeline_transrecv(uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint16_t txi = 0U;
    uint16_t rxi = 0U;

    memset(rx, 0, len);
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_BYTE_END | RB_SPI_IF_FIFO_OV;

    SetFirstData(tx[0]);
    spi0_wait_cs_low();

    while ((rxi < len) || (txi < len))
    {
        while ((txi < len) && (R8_SPI0_FIFO_COUNT < SPI_FIFO_SIZE))
        {
            R8_SPI0_FIFO = tx[txi];
            txi++;
        }

        if ((R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END) != 0U)
        {
            if (rxi < len)
            {
                rx[rxi] = R8_SPI0_BUFFER;
                rxi++;
            }
            R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
        }

        if ((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) != 0U)
        {
            break;
        }
    }

    while ((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) == 0U)
    {
        if ((R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END) != 0U)
        {
            if (rxi < len)
            {
                rx[rxi] = R8_SPI0_BUFFER;
                rxi++;
            }
            R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
        }
    }

    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_BYTE_END | RB_SPI_IF_FIFO_OV;
}
#endif

#if CH585_SPI_UART_TELEMETRY
static void ch585_spi_uart_telemetry_send(const char *text)
{
    uint16_t len;

    if (text == NULL)
    {
        return;
    }

    len = (uint16_t)strlen(text);
    if (len == 0U)
    {
        return;
    }

#if CH585_SPI_UART_TELEMETRY_PORT == 1
    UART1_SendString((uint8_t *)text, len);
#else
    UART0_SendString((uint8_t *)text, len);
#endif
}

static void ch585_spi_uart_telemetry_printf(const char *fmt, ...)
{
    char line[192];
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (n < 0)
    {
        return;
    }

    line[sizeof(line) - 1U] = '\0';
    ch585_spi_uart_telemetry_send(line);
}

static void ch585_spi_uart_telemetry_init(void)
{
#if CH585_SPI_UART_TELEMETRY_PORT == 1
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOPinRemap(DISABLE, RB_PIN_UART1);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    UART1_BaudRateCfg(CH585_SPI_UART_TELEMETRY_BAUD);
#else
    GPIOB_SetBits(GPIO_Pin_7);
    GPIOPinRemap(DISABLE, RB_PIN_UART0);
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_7, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
    UART0_BaudRateCfg(CH585_SPI_UART_TELEMETRY_BAUD);
#endif

    ch585_spi_uart_telemetry_printf("\r\nCH585 SPI UART telemetry start\r\n");
    ch585_spi_uart_telemetry_printf("uart=UART%u baud=%u mode short=%u req_only=%u pipeline=%u\r\n",
                                    (unsigned int)CH585_SPI_UART_TELEMETRY_PORT,
                                    (unsigned int)CH585_SPI_UART_TELEMETRY_BAUD,
                                    (unsigned int)CH585_USE_SHORT_FRAME,
                                    (unsigned int)CH585_MODE_REQUEST_ONLY_SHORT,
                                    (unsigned int)CH585_MODE_PIPELINE_SHORT);
    ch585_spi_uart_telemetry_printf("loopback: CH585 frame seq -> H417 cmd ack_seq -> CH585 UART ack_ok\r\n");
}

static void ch585_spi_uart_down_hex(const uint8_t *down, char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t i;

    if ((down == NULL) || (out == NULL))
    {
        return;
    }

    for (i = 0U; i < CH585_SCAN_DOWN_BYTES; i++)
    {
        out[(uint8_t)(i * 2U)] = hex[(down[i] >> 4U) & 0x0FU];
        out[(uint8_t)(i * 2U + 1U)] = hex[down[i] & 0x0FU];
    }
    out[CH585_SCAN_DOWN_BYTES * 2U] = '\0';
}

static void ch585_spi_uart_telemetry_dump(const char *tag,
                                          const ch585_scan_wire_frame_t *frame)
{
    char down_hex[(CH585_SCAN_DOWN_BYTES * 2U) + 1U];

    if (frame != NULL)
    {
        ch585_spi_uart_down_hex(frame->down_bits, down_hex);
    }
    else
    {
        ch585_spi_uart_down_hex(g_spi_tlm_prev_down, down_hex);
    }

    ch585_spi_uart_telemetry_printf(
        "SP %s frm=%lu cmd_ok=%lu cmd_bad=%lu ack_ok=%lu ack_miss=%lu "
        "sent=%u host=%u host_ack=%u ack=%u cmd=%u bad=%u "
        "sck=%u.%02uMHz down=%s\r\n",
        tag,
        (unsigned long)g_spi_tlm_frames,
        (unsigned long)g_spi_tlm_cmd_ok,
        (unsigned long)g_spi_tlm_cmd_bad,
        (unsigned long)g_spi_tlm_ack_ok,
        (unsigned long)g_spi_tlm_ack_miss,
        (unsigned int)g_spi_tlm_last_sent_seq,
        (unsigned int)g_spi_tlm_last_host_seq,
        (unsigned int)g_spi_tlm_last_host_ack_seq,
        (unsigned int)g_spi_tlm_last_ack_ok,
        (unsigned int)g_spi_tlm_last_cmd,
        (unsigned int)g_spi_tlm_last_invalid_reason,
        (unsigned int)(g_spi_tlm_last_sck_10khz / 100U),
        (unsigned int)(g_spi_tlm_last_sck_10khz % 100U),
        down_hex);
}

static void ch585_spi_uart_telemetry_note_cmd(const ch585_scan_wire_cmd_t *cmd,
                                              uint8_t valid)
{
    if (cmd == NULL)
    {
        return;
    }

    g_spi_tlm_last_host_seq = (uint8_t)cmd->host_seq;
    g_spi_tlm_last_host_ack_seq = (uint8_t)cmd->ack_seq;
    g_spi_tlm_last_cmd = cmd->cmd;
    g_spi_tlm_last_invalid_reason = g_last_cmd_invalid_reason;

    if (valid != 0U)
    {
#if CH585_USE_SHORT_FRAME
        g_spi_tlm_last_sck_10khz =
            (uint16_t)cmd->reserved[0] | (uint16_t)((uint16_t)cmd->reserved[1] << 8U);
#endif
        g_spi_tlm_cmd_ok++;
        if (g_spi_tlm_have_sent != 0U)
        {
            if ((uint8_t)cmd->ack_seq == g_spi_tlm_last_sent_seq)
            {
                g_spi_tlm_ack_ok++;
                g_spi_tlm_last_ack_ok = 1U;
            }
            else
            {
                g_spi_tlm_ack_miss++;
                g_spi_tlm_last_ack_ok = 0U;
            }
        }
    }
    else
    {
        g_spi_tlm_cmd_bad++;
        g_spi_tlm_last_ack_ok = 0U;
        ch585_spi_uart_telemetry_dump("BAD", NULL);
    }
}

static void ch585_spi_uart_telemetry_note_frame(const ch585_scan_wire_frame_t *frame)
{
    uint8_t down_changed = 0U;

    if (frame == NULL)
    {
        return;
    }

    g_spi_tlm_frames++;
    g_spi_tlm_last_sent_seq = (uint8_t)frame->seq;
    g_spi_tlm_have_sent = 1U;

    if ((g_spi_tlm_have_down == 0U) ||
        (memcmp(g_spi_tlm_prev_down, frame->down_bits, CH585_SCAN_DOWN_BYTES) != 0))
    {
        memcpy(g_spi_tlm_prev_down, frame->down_bits, CH585_SCAN_DOWN_BYTES);
        g_spi_tlm_have_down = 1U;
        down_changed = 1U;
    }

    if (down_changed != 0U)
    {
        ch585_spi_uart_telemetry_dump("KEY", frame);
    }
    else if ((CH585_SPI_UART_TELEMETRY_PERIOD_FRAMES != 0U) &&
             ((g_spi_tlm_frames % CH585_SPI_UART_TELEMETRY_PERIOD_FRAMES) == 0U))
    {
        ch585_spi_uart_telemetry_dump("ST", frame);
    }
}
#else
#define ch585_spi_uart_telemetry_init() ((void)0)
#define ch585_spi_uart_telemetry_note_cmd(cmd, valid) ((void)0)
#define ch585_spi_uart_telemetry_note_frame(frame) ((void)0)
#endif

#if CH585_ADC_PROBE_MODE && CH585_ADC_PROBE_UART_MODE
static void ch585_adc_probe_uart_init(void)
{
#if CH585_ADC_PROBE_UART_PORT == 1
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOPinRemap(DISABLE, RB_PIN_UART1);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    UART1_BaudRateCfg(CH585_ADC_PROBE_UART_BAUD);
#else
#if CH585_ADC_PROBE_UART0_REMAP
    GPIOA_SetBits(GPIO_Pin_14);
    GPIOPinRemap(ENABLE, RB_PIN_UART0);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
#else
    GPIOB_SetBits(GPIO_Pin_7);
    GPIOPinRemap(DISABLE, RB_PIN_UART0);
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_7, GPIO_ModeOut_PP_5mA);
#endif
    UART0_DefInit();
    UART0_BaudRateCfg(CH585_ADC_PROBE_UART_BAUD);
#endif
}

static void ch585_adc_probe_uart_send(const char *text)
{
    uint16_t len;

    if (text == NULL)
    {
        return;
    }

    len = (uint16_t)strlen(text);
    if (len == 0U)
    {
        return;
    }

#if CH585_ADC_PROBE_UART_PORT == 1
    UART1_SendString((uint8_t *)text, len);
#else
    UART0_SendString((uint8_t *)text, len);
#endif
}

static void ch585_adc_probe_uart_printf(const char *fmt, ...)
{
    char line[160];
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (n < 0)
    {
        return;
    }

    line[sizeof(line) - 1U] = '\0';
    ch585_adc_probe_uart_send(line);
}

static void ch585_adc_probe_uart_loop(void)
{
    uint16_t seq = 0U;

    ch585_adc_probe_uart_init();
    ch585_adc_probe_uart_printf("\r\nCH585 ADS7948 UART probe start\r\n");
    ch585_adc_probe_uart_printf("baud=%u key=%u right=%u spi1_div=%u period_ms=%u\r\n",
                                (unsigned int)CH585_ADC_PROBE_UART_BAUD,
                                (unsigned int)CH585_ADC_PROBE_DEBUG_KEY,
                                (unsigned int)CH585_ADC_PROBE_RIGHT_HALF,
                                (unsigned int)CH585_ADC_PROBE_SPI1_DIV,
                                (unsigned int)CH585_ADC_PROBE_UART_PERIOD_MS);
#if CH585_ADC_PROBE_CAL_ENABLE
    ch585_adc_probe_uart_printf("cal=on released=%u pressed=%u press_pm=%u release_pm=%u filter_shift=%u rt=%u\r\n",
                                (unsigned int)CH585_ADC_PROBE_CAL_RELEASED_ADC,
                                (unsigned int)CH585_ADC_PROBE_CAL_PRESSED_ADC,
                                (unsigned int)CH585_ADC_PROBE_CAL_PRESS_POSITION_PM,
                                (unsigned int)CH585_ADC_PROBE_CAL_RELEASE_POSITION_PM,
                                (unsigned int)CH585_ADC_PROBE_CAL_FILTER_SHIFT,
                                (unsigned int)CH585_ADC_PROBE_CAL_RT_ENABLE);
#else
    ch585_adc_probe_uart_printf("cal=off\r\n");
#endif
#if CH585_ADC_PROBE_UART_PORT == 1
    ch585_adc_probe_uart_printf("uart=UART1 tx=PA9 rx=PA8\r\n");
#else
    ch585_adc_probe_uart_printf("uart=UART0 remap=%u tx=%s rx=%s\r\n",
                                (unsigned int)CH585_ADC_PROBE_UART0_REMAP,
#if CH585_ADC_PROBE_UART0_REMAP
                                "PA14",
                                "PA15"
#else
                                "PB7",
                                "PB4"
#endif
    );
#endif
#if CH585_ADC_PROBE_UART_EVENT_MODE
    {
        uint8_t prev_down[CH585_SCAN_KEYS_PER_SOURCE];
        uint8_t prev_valid[CH585_SCAN_KEYS_PER_SOURCE];

        memset(prev_down, 0, sizeof(prev_down));
        memset(prev_valid, 0, sizeof(prev_valid));

        ch585_adc_probe_uart_printf("event_mode=on\r\n");
#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP
        ch585_adc_probe_uart_printf("right_half_valid_keys=%u slots: MUX1 D1-D10, MUX2 D1-D10, MUX3 D1-D10, MUX4 D1-D11\r\n",
                                    (unsigned int)CH585_ADC_PROBE_RIGHT_KEY_COUNT);
#endif
        ch585_adc_probe_uart_printf("line: EV seq hall slot lane mux d key raw filt pos word down rt st\r\n");

        while (1)
        {
            uint8_t down_count = 0U;
            uint8_t error_count = 0U;

#if CH585_ADC_PROBE_USE_RIGHT_KEYMAP
            uint8_t map_index;

            for (map_index = 0U;
                 map_index < CH585_ADC_PROBE_RIGHT_KEY_COUNT;
                 map_index++)
            {
                const ch585_adc_probe_key_map_t *map =
                    &g_adc_probe_right_key_map[map_index];
                uint8_t key_id = map->key_id;
                uint8_t lane = (uint8_t)(key_id / CH585_ADC_PROBE_MUX_COUNT);
                uint8_t mux_channel = (uint8_t)(key_id % CH585_ADC_PROBE_MUX_COUNT);
                uint16_t code = 0xFFFFU;
                uint16_t word = 0U;
                int status = ch585_adc_probe_update_key_state(key_id, &code, &word);

                if (status != 0)
                {
                    error_count++;
                }
                if (g_key_down[key_id] != 0U)
                {
                    down_count++;
                }

                if (prev_valid[key_id] == 0U)
                {
                    prev_down[key_id] = g_key_down[key_id];
                    prev_valid[key_id] = 1U;
                    continue;
                }

                if ((prev_down[key_id] != g_key_down[key_id]) || (status != 0))
                {
                    prev_down[key_id] = g_key_down[key_id];
                    ch585_adc_probe_uart_printf("EV %u %u %u %u %u %u %s %u %u %u 0x%04x %u %u %d\r\n",
                                                (unsigned int)seq,
                                                (unsigned int)map->hall_id,
                                                (unsigned int)key_id,
                                                (unsigned int)lane,
                                                (unsigned int)(lane + 1U),
                                                (unsigned int)(mux_channel + 1U),
                                                map->label,
                                                (unsigned int)code,
                                                (unsigned int)g_key_filtered_adc[key_id],
                                                (unsigned int)g_key_position_pm[key_id],
                                                (unsigned int)word,
                                                (unsigned int)g_key_down[key_id],
                                                (unsigned int)g_key_rt_armed[key_id],
                                                status);
                }
            }
#else
            uint8_t key_id;

            for (key_id = 0U; key_id < CH585_ADC_PROBE_KEY_COUNT; key_id++)
            {
                uint8_t lane = (uint8_t)(key_id / CH585_ADC_PROBE_MUX_COUNT);
                uint8_t mux_channel = (uint8_t)(key_id % CH585_ADC_PROBE_MUX_COUNT);
                uint16_t code = 0xFFFFU;
                uint16_t word = 0U;
                int status = ch585_adc_probe_update_key_state(key_id, &code, &word);

                if (status != 0)
                {
                    error_count++;
                }
                if (g_key_down[key_id] != 0U)
                {
                    down_count++;
                }

                if (prev_valid[key_id] == 0U)
                {
                    prev_down[key_id] = g_key_down[key_id];
                    prev_valid[key_id] = 1U;
                    continue;
                }

                if ((prev_down[key_id] != g_key_down[key_id]) || (status != 0))
                {
                    prev_down[key_id] = g_key_down[key_id];
                    ch585_adc_probe_uart_printf("EV %u %u %u %u %u %u %s %u %u %u 0x%04x %u %u %d\r\n",
                                                (unsigned int)seq,
                                                (unsigned int)key_id,
                                                (unsigned int)key_id,
                                                (unsigned int)lane,
                                                (unsigned int)(lane + 1U),
                                                (unsigned int)(mux_channel + 1U),
                                                "-",
                                                (unsigned int)code,
                                                (unsigned int)g_key_filtered_adc[key_id],
                                                (unsigned int)g_key_position_pm[key_id],
                                                (unsigned int)word,
                                                (unsigned int)g_key_down[key_id],
                                                (unsigned int)g_key_rt_armed[key_id],
                                                status);
                }
            }
#endif

            if ((seq % 200U) == 0U)
            {
                ch585_adc_probe_uart_printf("ST %u down=%u err=%u\r\n",
                                            (unsigned int)seq,
                                            (unsigned int)down_count,
                                            (unsigned int)error_count);
            }

            seq++;
            mDelaymS(CH585_ADC_PROBE_UART_PERIOD_MS);
        }
    }
#else
    ch585_adc_probe_uart_printf("line: AP seq key lane mux raw filt pos word down rt st\r\n");

    while (1)
    {
        uint8_t key_id = ch585_adc_probe_pick_key(0U);
        uint8_t lane = (uint8_t)(key_id / CH585_ADC_PROBE_MUX_COUNT);
        uint8_t mux_channel = (uint8_t)(key_id % CH585_ADC_PROBE_MUX_COUNT);
        uint16_t code = 0xFFFFU;
        uint16_t word = 0U;
        int status = ch585_adc_probe_sample_key(key_id, &code, &word);

        if ((status == 0) && (code <= ADS7948_CODE_MAX))
        {
            update_key_state_from_adc(key_id, code);
        }

        ch585_adc_probe_uart_printf("AP %u %u %u %u %u %u %u 0x%04x %u %u %d\r\n",
                                    (unsigned int)seq,
                                    (unsigned int)key_id,
                                    (unsigned int)lane,
                                    (unsigned int)mux_channel,
                                    (unsigned int)code,
                                    (unsigned int)g_key_filtered_adc[key_id],
                                    (unsigned int)g_key_position_pm[key_id],
                                    (unsigned int)word,
                                    (unsigned int)g_key_down[key_id],
                                    (unsigned int)g_key_rt_armed[key_id],
                                    status);

        seq++;
        mDelaymS(CH585_ADC_PROBE_UART_PERIOD_MS);
    }
#endif
}
#endif

int main(void)
{
#if CH585_ADC_PROBE_MODE && CH585_ADC_PROBE_UART_MODE
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);
    key_config_init_defaults();
    ch585_adc_probe_apply_calibration();
    ch585_adc_probe_uart_loop();
#elif CH585_LINK_TEST_MISO_LOW
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    /*
     * Physical-link test only: drive PA15/MISO low as a plain GPIO.
     * H417 should then read 0x00 on every sampled SPI byte if the wire,
     * power and common ground are correct.
     */
    GPIOA_ResetBits(GPIO_Pin_15);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeOut_PP_20mA);

    while (1)
    {
        GPIOA_ResetBits(GPIO_Pin_15);
    }
#elif CH585_LINK_TEST_SPI_PATTERN
    static uint8_t pattern[] = {
        0xA5U, 0x5AU, 0x3CU, 0xC3U,
        0x11U, 0x22U, 0x33U, 0x44U,
        0x55U, 0x66U, 0x77U, 0x88U,
        0x99U, 0xAAU, 0xBBU, 0xCCU,
    };

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    spi0_slave_pin_init();
    SPI0_SlaveInit();
    SPI0_DataMode(Mode0_HighBitINFront);
    SetFirstData(pattern[0]);

    while (1)
    {
        SPI0_SlaveTrans(pattern, (uint16_t)sizeof(pattern));
    }
#else
    uint16_t seq = 0U;
    uint16_t ack_host_seq = 0xFFFFU;
    uint16_t frame_flags;

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);
    key_config_init_defaults();
#if CH585_ADC_PROBE_MODE
    ch585_adc_probe_apply_calibration();
#endif
    ch585_spi_uart_telemetry_init();

    spi0_slave_pin_init();
    SPI0_SlaveInit();
    SPI0_DataMode(Mode0_HighBitINFront);

#if CH585_USE_PIPELINE_SHORT && CH585_USE_SHORT_FRAME
    build_scan_frame(seq, CH585_SCAN_FLAG_READY, 0xFFFFU);
    while (1)
    {
        spi0_wait_cs_high();
        spi0_slave_stream_reset();
        spi0_slave_pipeline_transrecv((uint8_t *)&g_frame,
                                      g_pipe_rx,
                                      (uint16_t)sizeof(g_frame));
        spi0_wait_cs_high();
        ch585_spi_uart_telemetry_note_frame(&g_frame);

        memset(&g_cmd, 0, sizeof(g_cmd));
        memcpy(&g_cmd, g_pipe_rx, sizeof(g_cmd));

        if (scan_cmd_is_valid(&g_cmd) != 0U)
        {
            ch585_spi_uart_telemetry_note_cmd(&g_cmd, 1U);
            ack_host_seq = g_cmd.host_seq;
            seq++;
            frame_flags = (uint16_t)(CH585_SCAN_FLAG_READY | scan_cmd_apply(&g_cmd));
            build_scan_frame(seq, frame_flags, ack_host_seq);
        }
        else
        {
            ch585_spi_uart_telemetry_note_cmd(&g_cmd, 0U);
        }
    }
#elif CH585_USE_REQUEST_ONLY_SHORT && CH585_USE_SHORT_FRAME
    uint8_t tx_index = 0U;
    uint16_t next_ack_seq = 0xFFFFU;
    uint16_t next_frame_flags = CH585_SCAN_FLAG_READY;

    (void)ack_host_seq;
    (void)frame_flags;
    build_scan_or_debug_frame_into(&g_frame_pingpong[0],
                                   seq,
                                   CH585_SCAN_FLAG_READY,
                                   0xFFFFU);
#if !CH585_STATIC_FRAME_TEST
    build_scan_or_debug_frame_into(&g_frame_pingpong[1],
                                   (uint16_t)(seq + 1U),
                                   next_frame_flags,
                                   next_ack_seq);
#endif

    while (1)
    {
        ch585_scan_wire_frame_t *tx_frame = &g_frame_pingpong[tx_index];
        ch585_scan_wire_frame_t *next_frame = &g_frame_pingpong[tx_index ^ 1U];

        spi0_wait_cs_high();
        spi0_slave_stream_reset();
        SetFirstData(((uint8_t *)tx_frame)[0]);
#if CH585_USE_SPI0_SLAVE_DMA
        spi0_slave_dma_trans_arm((uint8_t *)tx_frame, (uint16_t)sizeof(*tx_frame));
        spi0_wait_cs_low();
#if CH585_REQUEST_ONLY_CAPTURE_CMD
        spi0_slave_dma_wait_done_capture(g_req_rx, (uint16_t)sizeof(g_req_rx));
#else
        spi0_slave_dma_wait_done();
#endif
#else
        spi0_wait_cs_low();
        SPI0_SlaveTrans((uint8_t *)tx_frame, (uint16_t)sizeof(*tx_frame));
#endif
        spi0_wait_cs_high();
        ch585_spi_uart_telemetry_note_frame(tx_frame);
#if !CH585_STATIC_FRAME_TEST
        next_ack_seq = 0xFFFFU;
        next_frame_flags = CH585_SCAN_FLAG_READY;
#if CH585_REQUEST_ONLY_CAPTURE_CMD && CH585_USE_SPI0_SLAVE_DMA
        memset(&g_cmd, 0, sizeof(g_cmd));
        memcpy(&g_cmd, g_req_rx, sizeof(g_cmd));
        if (scan_cmd_is_valid(&g_cmd) != 0U)
        {
            ch585_spi_uart_telemetry_note_cmd(&g_cmd, 1U);
            next_ack_seq = g_cmd.host_seq;
            next_frame_flags |= scan_cmd_apply(&g_cmd);
        }
        else
        {
            ch585_spi_uart_telemetry_note_cmd(&g_cmd, 0U);
        }
#endif
        build_scan_or_debug_frame_into(next_frame,
                                       (uint16_t)(seq + 1U),
                                       next_frame_flags,
                                       next_ack_seq);
#endif
#if !CH585_STATIC_FRAME_TEST
        seq++;
        tx_index ^= 1U;
#endif
    }
#else
    while (1)
    {
        spi0_wait_cs_high();

        /*
         * Prepare the key snapshot before the host command arrives. After a
         * valid command we only need to patch ack_seq and CRC, which keeps the
         * command-to-data turnaround short.
         */
        build_scan_frame(seq, CH585_SCAN_FLAG_READY, 0xFFFFU);

        memset(&g_cmd, 0, sizeof(g_cmd));
        spi0_slave_stream_reset();
#if CH585_USE_SPI0_SLAVE_DMA
        spi0_slave_dma_recv_arm((uint8_t *)&g_cmd, (uint16_t)sizeof(g_cmd));
        spi0_wait_cs_low();
        spi0_slave_dma_wait_done();
#else
        spi0_wait_cs_low();
        SPI0_SlaveRecv((uint8_t *)&g_cmd, (uint16_t)sizeof(g_cmd));
#endif
        spi0_wait_cs_high();

        frame_flags = CH585_SCAN_FLAG_READY;
        if (scan_cmd_is_valid(&g_cmd) != 0U)
        {
            ch585_spi_uart_telemetry_note_cmd(&g_cmd, 1U);
            ack_host_seq = g_cmd.host_seq;
            frame_flags |= scan_cmd_apply(&g_cmd);
            g_last_cmd_valid = 1U;
            g_last_cmd_id = g_cmd.cmd;
            g_last_cmd_host_seq = g_cmd.host_seq;
            g_last_cmd_error = ((frame_flags & CH585_SCAN_FLAG_CMD_ERROR) != 0U) ? 1U : 0U;
        }
        else
        {
            g_last_cmd_valid = 0U;
            g_last_cmd_id = g_cmd.cmd;
            g_last_cmd_host_seq = g_cmd.host_seq;
            g_last_cmd_error = 1U;
            g_bad_cmd_raw_magic = g_cmd.magic;
            g_bad_cmd_raw_cmd = g_cmd.cmd;
            g_bad_cmd_raw_host_seq = g_cmd.host_seq;
            g_bad_cmd_raw_ack_seq = g_cmd.ack_seq;
            g_bad_cmd_invalid_reason = g_last_cmd_invalid_reason;
            g_invalid_cmd_count++;
            ch585_spi_uart_telemetry_note_cmd(&g_cmd, 0U);
            continue;
        }

        if (((frame_flags & CH585_SCAN_FLAG_CMD_ERROR) == 0U) &&
            (g_cmd.cmd == CH585_SCAN_CMD_GET_DEBUG))
        {
            build_debug_frame_into(&g_frame, seq, g_cmd.target_key);
        }
        else
        {
            finish_scan_frame(frame_flags, ack_host_seq);
        }
        spi0_slave_stream_reset();
        SetFirstData(((uint8_t *)&g_frame)[0]);
#if CH585_USE_SPI0_SLAVE_DMA
        spi0_slave_dma_trans_arm((uint8_t *)&g_frame, (uint16_t)sizeof(g_frame));
        spi0_wait_cs_low();
        spi0_slave_dma_wait_done();
#else
        spi0_wait_cs_low();
        SPI0_SlaveTrans((uint8_t *)&g_frame, (uint16_t)sizeof(g_frame));
#endif
        ch585_spi_uart_telemetry_note_frame(&g_frame);
        seq++;
    }
#endif
#endif
}
