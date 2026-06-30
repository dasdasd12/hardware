#include <string.h>
#include <stdio.h>

#include "aik_spi_protocol.h"
#include "board_init.h"
#include "ch585_link.h"
#include "default_profile.h"
#include "half_state.h"
#include "rf_report_bridge.h"
#include "rgb_status.h"

#ifndef V3F_ENABLE_USBHS_8K
#define V3F_ENABLE_USBHS_8K 0
#endif

#if V3F_ENABLE_USBHS_8K
#include "ch32h417_usbhs_hid_nkro.h"
typedef ch32h417_usbhs_hid_nkro_diag_t v3f_usb_hid_nkro_diag_t;
#define v3f_usb_hid_nkro_init ch32h417_usbhs_hid_nkro_init
#define v3f_usb_hid_nkro_send ch32h417_usbhs_hid_nkro_send
#define v3f_usb_hid_nkro_pending_empty ch32h417_usbhs_hid_nkro_pending_empty
#define v3f_usb_hid_nkro_submit ch32h417_usbhs_hid_nkro_submit
#define v3f_usb_hid_nkro_reports ch32h417_usbhs_hid_nkro_reports
#define v3f_usb_hid_nkro_debug_write ch32h417_usbhs_hid_nkro_debug_write
#define v3f_usb_hid_nkro_diag_snapshot ch32h417_usbhs_hid_nkro_diag_snapshot
#else
#include "ch32h417_usbfs_hid_nkro.h"
typedef ch32h417_usbfs_hid_nkro_diag_t v3f_usb_hid_nkro_diag_t;
#define v3f_usb_hid_nkro_init ch32h417_usbfs_hid_nkro_init
#define v3f_usb_hid_nkro_send ch32h417_usbfs_hid_nkro_send
#define v3f_usb_hid_nkro_pending_empty ch32h417_usbfs_hid_nkro_pending_empty
#define v3f_usb_hid_nkro_submit ch32h417_usbfs_hid_nkro_submit
#define v3f_usb_hid_nkro_reports ch32h417_usbfs_hid_nkro_reports
#define v3f_usb_hid_nkro_debug_write ch32h417_usbfs_hid_nkro_debug_write
#define v3f_usb_hid_nkro_diag_snapshot ch32h417_usbfs_hid_nkro_diag_snapshot
#endif

#ifndef V3F_USB_REPORT_INTERVAL_US
#define V3F_USB_REPORT_INTERVAL_US 1000U
#endif

#if V3F_USB_REPORT_INTERVAL_US == 0
#error "V3F_USB_REPORT_INTERVAL_US must be non-zero"
#endif

#define V3F_LINK_STALE_US 5000U
#define V3F_LINK_STALE_TICKS \
    ((uint8_t)((V3F_LINK_STALE_US + V3F_USB_REPORT_INTERVAL_US - 1U) / \
               V3F_USB_REPORT_INTERVAL_US))

#ifndef V3F_ENABLE_RF_BRIDGE
#define V3F_ENABLE_RF_BRIDGE 0
#endif

#ifndef V3F_ENABLE_SPI_HOST_CMD
#define V3F_ENABLE_SPI_HOST_CMD 0
#endif

#ifndef V3F_ENABLE_USBFS_CDC_DEBUG
#define V3F_ENABLE_USBFS_CDC_DEBUG 0
#endif

#ifndef V3F_OUTPUT_MODE_DEFAULT
#define V3F_OUTPUT_MODE_DEFAULT AIK_OUTPUT_MODE_USBHS
#endif

#ifndef V3F_CDC_DEBUG_PERIOD_TICKS
#define V3F_CDC_DEBUG_PERIOD_TICKS 25U
#endif

#define V3F_SWITCH_KEY_ESC 46U
#define V3F_SWITCH_KEY_F1  45U
#define V3F_SWITCH_KEY_F2  44U
#define V3F_SWITCH_KEY_F3  43U
#define V3F_SWITCH_KEY_F5  41U
#define V3F_SWITCH_KEY_F6  6U

enum
{
    V3F_TRACE_TICK = 4,
    V3F_TRACE_LEFT_OK = 5,
    V3F_TRACE_RIGHT_OK = 6,
    V3F_TRACE_LEFT_STALE = 7,
    V3F_TRACE_RIGHT_STALE = 8,
    V3F_TRACE_USB_REPORTS = 9,
    V3F_TRACE_USB_IRQ = 10,
    V3F_TRACE_USB_SETUP = 11,
    V3F_TRACE_USB_RESET = 12,
    V3F_TRACE_USB_LAST_REQ = 13,
    V3F_TRACE_USB_LAST_VALUE = 14,
    V3F_TRACE_USB_CLOCK_READY = 15,
    V3F_TRACE_USB_CLOCK_ERROR = 16,
    V3F_TRACE_USB_RCC_CFGR2 = 17,
    V3F_TRACE_USB_RCC_CTLR = 18,
    V3F_TRACE_USB_RCC_PLLCFGR2 = 19,
    V3F_TRACE_USB_BASE_CTRL = 20,
    V3F_TRACE_USB_UDEV_CTRL = 21,
    V3F_TRACE_USB_INT_EN = 22,
    V3F_TRACE_USB_UEP0_DMA = 23,
    V3F_TRACE_USB_XFER_BUF0 = 24,
    V3F_TRACE_USB_XFER_BUF1 = 25,
    V3F_TRACE_USB_RESP0 = 26,
    V3F_TRACE_USB_TX_LEN = 27,
    V3F_TRACE_USB_RX_LEN = 28,
    V3F_TRACE_LEFT_LINK_ERRORS = 29,
    V3F_TRACE_RIGHT_LINK_ERRORS = 30,
    V3F_TRACE_LEFT_INVALID_FRAMES = 31,
    V3F_TRACE_RIGHT_INVALID_FRAMES = 32,
    V3F_TRACE_LEFT_RX_HEADER = 33,
    V3F_TRACE_RIGHT_RX_HEADER = 34,
    V3F_TRACE_LEFT_RX_CRC = 35,
    V3F_TRACE_RIGHT_RX_CRC = 36,
    V3F_TRACE_LEFT_DOWN0 = 37,
    V3F_TRACE_RIGHT_DOWN0 = 38,
    V3F_TRACE_KEYS_DOWN01 = 39,
    V3F_TRACE_NKRO_0205 = 40,
    V3F_TRACE_NKRO_0609 = 41,
    V3F_TRACE_OUTPUT_MODE = 42,
};

typedef struct
{
    aik_spi_half_state_v1_t frame;
    uint8_t valid;
    uint8_t stale_ticks;
} v3f_half_cache_t;

static void update_half_cache(v3f_half_cache_t *cache,
                              uint8_t got_frame,
                              const aik_spi_half_state_v1_t *next)
{
    if(got_frame != 0U)
    {
        cache->frame = *next;
        cache->valid = 1U;
        cache->stale_ticks = 0U;
        return;
    }
}

static void age_half_cache_on_usb_report(v3f_half_cache_t *cache,
                                         uint8_t got_frame)
{
    if((cache == 0) || (got_frame != 0U))
    {
        return;
    }
    if(cache->stale_ticks < V3F_LINK_STALE_TICKS)
    {
        cache->stale_ticks++;
    }
    if(cache->stale_ticks >= V3F_LINK_STALE_TICKS)
    {
        cache->valid = 0U;
    }
}

static void v3f_usb_diag_trace(void)
{
    v3f_usb_hid_nkro_diag_t usb_diag;

    v3f_usb_hid_nkro_diag_snapshot(&usb_diag);
    v3f_trace_set(V3F_TRACE_USB_REPORTS, v3f_usb_hid_nkro_reports());
    v3f_trace_set(V3F_TRACE_USB_IRQ, usb_diag.irq_count);
    v3f_trace_set(V3F_TRACE_USB_SETUP, usb_diag.setup_count);
    v3f_trace_set(V3F_TRACE_USB_RESET, usb_diag.bus_reset_count);
    v3f_trace_set(V3F_TRACE_USB_LAST_REQ, usb_diag.last_setup_request);
    v3f_trace_set(V3F_TRACE_USB_LAST_VALUE, usb_diag.last_setup_value);
    v3f_trace_set(V3F_TRACE_USB_CLOCK_READY, usb_diag.clock_ready);
    v3f_trace_set(V3F_TRACE_USB_CLOCK_ERROR, usb_diag.clock_error);
    v3f_trace_set(V3F_TRACE_USB_RCC_CFGR2, usb_diag.rcc_cfgr2);
    v3f_trace_set(V3F_TRACE_USB_RCC_CTLR, usb_diag.rcc_ctlr);
    v3f_trace_set(V3F_TRACE_USB_RCC_PLLCFGR2, usb_diag.rcc_pllcfgr2);
    v3f_trace_set(V3F_TRACE_USB_BASE_CTRL, usb_diag.usb_base_ctrl);
    v3f_trace_set(V3F_TRACE_USB_UDEV_CTRL, usb_diag.usb_udev_ctrl);
    v3f_trace_set(V3F_TRACE_USB_INT_EN, usb_diag.usb_int_en);
    v3f_trace_set(V3F_TRACE_USB_UEP0_DMA, usb_diag.uep0_dma);
    v3f_trace_set(V3F_TRACE_USB_XFER_BUF0, usb_diag.last_xfer_buf0);
    v3f_trace_set(V3F_TRACE_USB_XFER_BUF1, usb_diag.last_xfer_buf1);
    v3f_trace_set(V3F_TRACE_USB_RESP0, usb_diag.last_resp0);
    v3f_trace_set(V3F_TRACE_USB_TX_LEN, usb_diag.last_tx_len);
    v3f_trace_set(V3F_TRACE_USB_RX_LEN, usb_diag.last_rx_len);
}

static uint32_t pack4(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    return ((uint32_t)b0) |
           ((uint32_t)b1 << 8) |
           ((uint32_t)b2 << 16) |
           ((uint32_t)b3 << 24);
}

static uint8_t v3f_output_mode_sanitize(uint8_t mode)
{
    if((mode == AIK_OUTPUT_MODE_RF24) || (mode == AIK_OUTPUT_MODE_BLE))
    {
        return mode;
    }
    return AIK_OUTPUT_MODE_USBHS;
}

static uint8_t v3f_output_mode_is_wireless(uint8_t mode)
{
    return (uint8_t)((mode == AIK_OUTPUT_MODE_RF24) ||
                     (mode == AIK_OUTPUT_MODE_BLE));
}

static void v3f_global_key_clear_local(v3f_global_key_state_t *keys,
                                       uint8_t key_id)
{
    if((keys != 0) && (key_id < AIK_KEY_COUNT_TOTAL))
    {
        keys->down[key_id >> 3] &=
            (uint8_t)~(uint8_t)(1U << (key_id & 7U));
    }
}

static uint8_t v3f_output_mode_update_from_keys(v3f_global_key_state_t *keys,
                                                uint8_t current_mode)
{
    uint8_t next_mode = current_mode;
    uint8_t combo_active = 0U;

    if((keys != 0) &&
       (v3f_global_key_is_down(keys, V3F_SWITCH_KEY_ESC) != 0U))
    {
        if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F1) != 0U)
        {
            next_mode = AIK_OUTPUT_MODE_USBHS;
            combo_active = 1U;
        }
        else if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F2) != 0U)
        {
            next_mode = AIK_OUTPUT_MODE_RF24;
            combo_active = 1U;
        }
        else if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F3) != 0U)
        {
            next_mode = AIK_OUTPUT_MODE_BLE;
            combo_active = 1U;
        }
    }

    if(combo_active != 0U)
    {
        v3f_global_key_clear_local(keys, V3F_SWITCH_KEY_ESC);
        v3f_global_key_clear_local(keys, V3F_SWITCH_KEY_F1);
        v3f_global_key_clear_local(keys, V3F_SWITCH_KEY_F2);
        v3f_global_key_clear_local(keys, V3F_SWITCH_KEY_F3);
    }

    return v3f_output_mode_sanitize(next_mode);
}

static void v3f_lighting_update_from_keys(v3f_global_key_state_t *keys)
{
    static uint8_t combo_latched;
    uint8_t combo_down = 0U;
    uint8_t handled = 0U;

    if((keys != 0) &&
       (v3f_global_key_is_down(keys, V3F_SWITCH_KEY_ESC) != 0U))
    {
        if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F5) != 0U)
        {
            combo_down = 1U;
            if(combo_latched == 0U)
            {
                v3f_rgb_status_toggle_enabled();
            }
            handled = 1U;
        }
        else if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F6) != 0U)
        {
            combo_down = 1U;
            if(combo_latched == 0U)
            {
                v3f_rgb_status_next_effect();
            }
            handled = 1U;
        }
    }

    combo_latched = combo_down;
    if(handled != 0U)
    {
        v3f_global_key_clear_local(keys, V3F_SWITCH_KEY_ESC);
        v3f_global_key_clear_local(keys, V3F_SWITCH_KEY_F5);
        v3f_global_key_clear_local(keys, V3F_SWITCH_KEY_F6);
    }
}

static void v3f_link_diag_trace(const v3f_half_cache_t *left,
                                const v3f_half_cache_t *right)
{
    v3f_ch585_link_stats_t left_stats;
    v3f_ch585_link_stats_t right_stats;

    memset(&left_stats, 0, sizeof(left_stats));
    memset(&right_stats, 0, sizeof(right_stats));
    v3f_ch585_link_stats(AIK_HALF_ID_LEFT, &left_stats);
    v3f_ch585_link_stats(AIK_HALF_ID_RIGHT, &right_stats);

    v3f_trace_set(V3F_TRACE_LEFT_LINK_ERRORS, left_stats.link_errors);
    v3f_trace_set(V3F_TRACE_RIGHT_LINK_ERRORS, right_stats.link_errors);
    v3f_trace_set(V3F_TRACE_LEFT_INVALID_FRAMES, left_stats.invalid_frames);
    v3f_trace_set(V3F_TRACE_RIGHT_INVALID_FRAMES, right_stats.invalid_frames);
    v3f_trace_set(V3F_TRACE_LEFT_RX_HEADER,
                  pack4(left_stats.last_magic,
                        left_stats.last_type,
                        (uint8_t)(left_stats.last_seq & 0xFFU),
                        (uint8_t)(left_stats.last_seq >> 8)));
    v3f_trace_set(V3F_TRACE_RIGHT_RX_HEADER,
                  pack4(right_stats.last_magic,
                        right_stats.last_type,
                        (uint8_t)(right_stats.last_seq & 0xFFU),
                        (uint8_t)(right_stats.last_seq >> 8)));
    v3f_trace_set(V3F_TRACE_LEFT_RX_CRC,
                  ((uint32_t)left_stats.last_crc) |
                  ((uint32_t)left_stats.last_calc_crc << 16));
    v3f_trace_set(V3F_TRACE_RIGHT_RX_CRC,
                  ((uint32_t)right_stats.last_crc) |
                  ((uint32_t)right_stats.last_calc_crc << 16));
    v3f_trace_set(V3F_TRACE_LEFT_DOWN0,
                  (left != 0 && left->valid != 0U) ?
                  pack4(left->frame.down_bits[0],
                        left->frame.down_bits[1],
                        left->frame.down_bits[2],
                        left->frame.down_bits[3]) : 0U);
    v3f_trace_set(V3F_TRACE_RIGHT_DOWN0,
                  (right != 0 && right->valid != 0U) ?
                  pack4(right->frame.down_bits[0],
                        right->frame.down_bits[1],
                        right->frame.down_bits[2],
                        right->frame.down_bits[3]) : 0U);
}

static void v3f_report_diag_trace(const v3f_global_key_state_t *keys,
                                  const uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    if((keys == 0) || (nkro16 == 0))
    {
        return;
    }

    v3f_trace_set(V3F_TRACE_KEYS_DOWN01,
                  pack4(keys->down[0],
                        keys->down[1],
                        keys->down[2],
                        keys->down[3]));
    v3f_trace_set(V3F_TRACE_NKRO_0205,
                  pack4(nkro16[2], nkro16[3], nkro16[4], nkro16[5]));
    v3f_trace_set(V3F_TRACE_NKRO_0609,
                  pack4(nkro16[6], nkro16[7], nkro16[8], nkro16[9]));
}

static void v3f_prepare_spi_poll_tx(aik_spi_host_cmd_v1_t *cmd,
                                    uint16_t host_seq,
                                    const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                    uint8_t output_mode)
{
    if(cmd == 0)
    {
        return;
    }

#if V3F_ENABLE_SPI_HOST_CMD
    v3f_rf_report_bridge_prepare_cmd(cmd, host_seq, nkro16, output_mode);
#else
    (void)host_seq;
    (void)nkro16;
    (void)output_mode;
    memset(cmd, 0, sizeof(*cmd));
#endif
}

static void v3f_prepare_right_state_push(aik_spi_host_cmd_v1_t *cmd,
                                         uint16_t host_seq,
                                         const aik_spi_half_state_v1_t *right,
                                         uint8_t output_mode)
{
#if V3F_ENABLE_SPI_HOST_CMD && V3F_ENABLE_RF_BRIDGE
    aik_spi_half_state_v1_t empty_right;

    if(right != 0)
    {
        v3f_rf_report_bridge_prepare_right_state_cmd(cmd,
                                                     host_seq,
                                                     right,
                                                     output_mode);
        return;
    }

    memset(&empty_right, 0, sizeof(empty_right));
    empty_right.half_seq = host_seq;
    aik_spi_half_state_finish(&empty_right, AIK_KEY_COUNT_RIGHT);
    v3f_rf_report_bridge_prepare_right_state_cmd(cmd,
                                                 host_seq,
                                                 &empty_right,
                                                 output_mode);
#else
    v3f_prepare_spi_poll_tx(cmd, host_seq, 0, output_mode);
    (void)right;
#endif
}

static void v3f_cdc_debug_poll(uint16_t tick,
                               const v3f_half_cache_t *left,
                               const v3f_half_cache_t *right,
                               const v3f_global_key_state_t *keys,
                               const uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
#if V3F_ENABLE_USBFS_CDC_DEBUG
    static uint16_t last_tick;
    static uint8_t phase;
    v3f_ch585_link_stats_t left_stats;
    v3f_ch585_link_stats_t right_stats;
    char line[96];
    int len;

    if((uint16_t)(tick - last_tick) < V3F_CDC_DEBUG_PERIOD_TICKS)
    {
        return;
    }
    last_tick = tick;

    memset(&left_stats, 0, sizeof(left_stats));
    memset(&right_stats, 0, sizeof(right_stats));
    v3f_ch585_link_stats(AIK_HALF_ID_LEFT, &left_stats);
    v3f_ch585_link_stats(AIK_HALF_ID_RIGHT, &right_stats);

    switch(phase)
    {
        case 0:
            len = snprintf(line, sizeof(line),
                           "L ok=%u st=%u err=%lu inv=%lu h=%02x%02x s=%u\r\n",
                           (unsigned int)((left != 0) ? left->valid : 0U),
                           (unsigned int)((left != 0) ? left->stale_ticks : 0U),
                           (unsigned long)left_stats.link_errors,
                           (unsigned long)left_stats.invalid_frames,
                           (unsigned int)left_stats.last_magic,
                           (unsigned int)left_stats.last_type,
                           (unsigned int)left_stats.last_seq);
            break;

        case 1:
            len = snprintf(line, sizeof(line),
                           "L raw=%02x%02x%02x%02x bits=%02x%02x%02x%02x%02x%02x crc=%04x/%04x diag=%08lx\r\n",
                           (unsigned int)left_stats.last_rx_head[0],
                           (unsigned int)left_stats.last_rx_head[1],
                           (unsigned int)left_stats.last_rx_head[2],
                           (unsigned int)left_stats.last_rx_head[3],
                           (unsigned int)left_stats.last_rx_down[0],
                           (unsigned int)left_stats.last_rx_down[1],
                           (unsigned int)left_stats.last_rx_down[2],
                           (unsigned int)left_stats.last_rx_down[3],
                           (unsigned int)left_stats.last_rx_down[4],
                           (unsigned int)left_stats.last_rx_down[5],
                           (unsigned int)left_stats.last_crc,
                           (unsigned int)left_stats.last_calc_crc,
                           (unsigned long)left_stats.last_diag);
            break;

        case 2:
            len = snprintf(line, sizeof(line),
                           "L d=%02x%02x%02x%02x\r\n",
                           (unsigned int)((left != 0 && left->valid) ? left->frame.down_bits[0] : 0U),
                           (unsigned int)((left != 0 && left->valid) ? left->frame.down_bits[1] : 0U),
                           (unsigned int)((left != 0 && left->valid) ? left->frame.down_bits[2] : 0U),
                           (unsigned int)((left != 0 && left->valid) ? left->frame.down_bits[3] : 0U));
            break;

        case 3:
            len = snprintf(line, sizeof(line),
                           "R ok=%u st=%u err=%lu inv=%lu h=%02x%02x s=%u\r\n",
                           (unsigned int)((right != 0) ? right->valid : 0U),
                           (unsigned int)((right != 0) ? right->stale_ticks : 0U),
                           (unsigned long)right_stats.link_errors,
                           (unsigned long)right_stats.invalid_frames,
                           (unsigned int)right_stats.last_magic,
                           (unsigned int)right_stats.last_type,
                           (unsigned int)right_stats.last_seq);
            break;

        case 4:
            len = snprintf(line, sizeof(line),
                           "R raw=%02x%02x%02x%02x bits=%02x%02x%02x%02x%02x%02x crc=%04x/%04x diag=%08lx\r\n",
                           (unsigned int)right_stats.last_rx_head[0],
                           (unsigned int)right_stats.last_rx_head[1],
                           (unsigned int)right_stats.last_rx_head[2],
                           (unsigned int)right_stats.last_rx_head[3],
                           (unsigned int)right_stats.last_rx_down[0],
                           (unsigned int)right_stats.last_rx_down[1],
                           (unsigned int)right_stats.last_rx_down[2],
                           (unsigned int)right_stats.last_rx_down[3],
                           (unsigned int)right_stats.last_rx_down[4],
                           (unsigned int)right_stats.last_rx_down[5],
                           (unsigned int)right_stats.last_crc,
                           (unsigned int)right_stats.last_calc_crc,
                           (unsigned long)right_stats.last_diag);
            break;

        case 5:
            len = snprintf(line, sizeof(line),
                           "R d=%02x%02x%02x%02x\r\n",
                           (unsigned int)((right != 0 && right->valid) ? right->frame.down_bits[0] : 0U),
                           (unsigned int)((right != 0 && right->valid) ? right->frame.down_bits[1] : 0U),
                           (unsigned int)((right != 0 && right->valid) ? right->frame.down_bits[2] : 0U),
                           (unsigned int)((right != 0 && right->valid) ? right->frame.down_bits[3] : 0U));
            break;

        default:
            len = snprintf(line, sizeof(line),
                           "K d=%02x%02x%02x%02x n=%02x%02x%02x%02x rpt=%lu\r\n",
                           (unsigned int)((keys != 0) ? keys->down[0] : 0U),
                           (unsigned int)((keys != 0) ? keys->down[1] : 0U),
                           (unsigned int)((keys != 0) ? keys->down[2] : 0U),
                           (unsigned int)((keys != 0) ? keys->down[3] : 0U),
                           (unsigned int)((nkro16 != 0) ? nkro16[2] : 0U),
                           (unsigned int)((nkro16 != 0) ? nkro16[3] : 0U),
                           (unsigned int)((nkro16 != 0) ? nkro16[4] : 0U),
                           (unsigned int)((nkro16 != 0) ? nkro16[5] : 0U),
                           (unsigned long)v3f_usb_hid_nkro_reports());
            break;
    }

    phase = (uint8_t)((phase + 1U) % 7U);
    if(len > 0)
    {
        (void)v3f_usb_hid_nkro_debug_write(line);
    }
#else
    (void)tick;
    (void)left;
    (void)right;
    (void)keys;
    (void)nkro16;
#endif
}

int main(void)
{
    v3f_half_cache_t left;
    v3f_half_cache_t right;
    aik_spi_half_state_v1_t rx;
    aik_spi_host_cmd_v1_t left_cmd;
    aik_spi_host_cmd_v1_t right_cmd;
    v3f_global_key_state_t keys;
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES];
    uint8_t zero_nkro16[AIK_NKRO_REPORT_BYTES];
    uint8_t output_mode = v3f_output_mode_sanitize(V3F_OUTPUT_MODE_DEFAULT);
    uint16_t host_seq = 0U;

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(nkro16, 0, sizeof(nkro16));
    memset(zero_nkro16, 0, sizeof(zero_nkro16));

    v3f_board_init();
    v3f_usb_hid_nkro_init();
    v3f_ch585_link_init();
    v3f_rgb_status_init();
    v3f_rgb_status_set_enabled(1U);

    while(1)
    {
        uint8_t got_left;
        uint8_t got_right;
        uint8_t previous_output_mode = output_mode;

#if !V3F_ENABLE_RF_BRIDGE
        if(v3f_usb_hid_nkro_pending_empty() == 0U)
        {
            continue;
        }
#endif

#if V3F_ENABLE_RF_BRIDGE
        v3f_prepare_spi_poll_tx(&right_cmd,
                                host_seq,
                                0,
                                output_mode);
        got_right = v3f_ch585_link_poll(AIK_HALF_ID_RIGHT, &right_cmd, &rx);
        update_half_cache(&right, got_right, &rx);
        age_half_cache_on_usb_report(&right, got_right);

        if(v3f_output_mode_is_wireless(output_mode) != 0U)
        {
            v3f_prepare_right_state_push(&left_cmd,
                                         host_seq,
                                         right.valid ? &right.frame : 0,
                                         output_mode);
        }
        else
        {
            v3f_prepare_spi_poll_tx(&left_cmd,
                                    host_seq,
                                    0,
                                    output_mode);
        }
        got_left = v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &left_cmd, &rx);
        update_half_cache(&left, got_left, &rx);
        age_half_cache_on_usb_report(&left, got_left);
#else
        v3f_prepare_spi_poll_tx(&left_cmd,
                                host_seq,
                                nkro16,
                                output_mode);
        v3f_prepare_spi_poll_tx(&right_cmd,
                                host_seq,
                                nkro16,
                                output_mode);

        got_left = v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &left_cmd, &rx);
        update_half_cache(&left, got_left, &rx);

        got_right = v3f_ch585_link_poll(AIK_HALF_ID_RIGHT, &right_cmd, &rx);
        update_half_cache(&right, got_right, &rx);

        age_half_cache_on_usb_report(&left, got_left);
        age_half_cache_on_usb_report(&right, got_right);
#endif

        v3f_half_state_merge(left.valid ? &left.frame : 0,
                             right.valid ? &right.frame : 0,
                             &keys);
        output_mode = v3f_output_mode_update_from_keys(&keys, output_mode);
        v3f_lighting_update_from_keys(&keys);
        v3f_default_profile_build_nkro16(&keys, nkro16);
        if((previous_output_mode == AIK_OUTPUT_MODE_USBHS) &&
           (output_mode != AIK_OUTPUT_MODE_USBHS) &&
           (v3f_usb_hid_nkro_pending_empty() != 0U))
        {
            (void)v3f_usb_hid_nkro_submit(zero_nkro16);
        }
        else if((output_mode == AIK_OUTPUT_MODE_USBHS) &&
                (v3f_usb_hid_nkro_pending_empty() != 0U))
        {
            (void)v3f_usb_hid_nkro_submit(nkro16);
        }

        v3f_trace_inc(V3F_TRACE_TICK);
        v3f_trace_set(V3F_TRACE_LEFT_OK, left.valid);
        v3f_trace_set(V3F_TRACE_RIGHT_OK, right.valid);
        v3f_trace_set(V3F_TRACE_LEFT_STALE, left.stale_ticks);
        v3f_trace_set(V3F_TRACE_RIGHT_STALE, right.stale_ticks);
        v3f_trace_set(V3F_TRACE_OUTPUT_MODE, output_mode);
        v3f_link_diag_trace(&left, &right);
        v3f_report_diag_trace(&keys, nkro16);
        v3f_usb_diag_trace();
        v3f_cdc_debug_poll(host_seq, &left, &right, &keys, nkro16);
        v3f_rgb_status_task(host_seq);

        host_seq++;
#if V3F_ENABLE_RF_BRIDGE
        v3f_board_delay_us(V3F_USB_REPORT_INTERVAL_US);
#endif
    }
}
