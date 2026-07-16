#include <string.h>
#include <stdio.h>

#include "aik_spi_protocol.h"
#include "board_init.h"
#include "ch585_link.h"
#include "default_profile.h"
#include "half_state.h"
#include "pc_link.h"
#include "profile_runtime.h"
#include "profile_sync.h"
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
#define v3f_usb_hid_nkro_submit_consumer ch32h417_usbhs_hid_nkro_submit_consumer
#define v3f_usb_hid_nkro_submit_mouse_wheel ch32h417_usbhs_hid_nkro_submit_mouse_wheel
#define v3f_usb_hid_nkro_reports ch32h417_usbhs_hid_nkro_reports
#define v3f_usb_hid_nkro_debug_write ch32h417_usbhs_hid_nkro_debug_write
#define v3f_usb_hid_nkro_diag_snapshot ch32h417_usbhs_hid_nkro_diag_snapshot
#if V3F_ENABLE_USBFS_CDC
/* Dual-controller build: USBHS keeps HID, USBFS carries the CDC
 * config/debug channel, so CDC writes go through the USBFS driver. */
#include "ch32h417.h"
#include "ch32h417_usbfs_hid_nkro.h"
#undef v3f_usb_hid_nkro_debug_write
#define v3f_usb_hid_nkro_debug_write ch32h417_usbfs_hid_nkro_debug_write
#endif
#else
#include "ch32h417_usbfs_hid_nkro.h"
typedef ch32h417_usbfs_hid_nkro_diag_t v3f_usb_hid_nkro_diag_t;
#define v3f_usb_hid_nkro_init ch32h417_usbfs_hid_nkro_init
#define v3f_usb_hid_nkro_send ch32h417_usbfs_hid_nkro_send
#define v3f_usb_hid_nkro_pending_empty ch32h417_usbfs_hid_nkro_pending_empty
#define v3f_usb_hid_nkro_submit ch32h417_usbfs_hid_nkro_submit
#define v3f_usb_hid_nkro_submit_consumer ch32h417_usbfs_hid_nkro_submit_consumer
#define v3f_usb_hid_nkro_submit_mouse_wheel ch32h417_usbfs_hid_nkro_submit_mouse_wheel
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

#ifndef V3F_ENABLE_USBFS_CDC
#define V3F_ENABLE_USBFS_CDC 0
#endif

#ifndef V3F_OUTPUT_MODE_DEFAULT
#define V3F_OUTPUT_MODE_DEFAULT AIK_OUTPUT_MODE_USBHS
#endif

#ifndef V3F_CDC_DEBUG_PERIOD_TICKS
#define V3F_CDC_DEBUG_PERIOD_TICKS 25U
#endif

#ifndef V3F_ENABLE_PROFILE_STATUS_SYNC
#define V3F_ENABLE_PROFILE_STATUS_SYNC 1
#endif

#ifndef V3F_PROFILE_STATUS_PERIOD_TICKS
#define V3F_PROFILE_STATUS_PERIOD_TICKS 512U
#endif

#ifndef V3F_CONSUMER_EVENT_INTERVAL_TICKS
#define V3F_CONSUMER_EVENT_INTERVAL_TICKS 320U
#endif

#ifndef V3F_CONSUMER_PENDING_LIMIT
#define V3F_CONSUMER_PENDING_LIMIT 8
#endif

#define V3F_FN_LAYER_KEY   38U
#define V3F_SWITCH_KEY_F1  45U
#define V3F_SWITCH_KEY_F2  44U
#define V3F_SWITCH_KEY_F3  43U
#define V3F_SWITCH_KEY_F5  41U
#define V3F_SWITCH_KEY_F6  6U

#ifndef V3F_LIGHTING_COMBO_PRESS_FRAMES
#define V3F_LIGHTING_COMBO_PRESS_FRAMES 4U
#endif

#ifndef V3F_LIGHTING_COMBO_RELEASE_FRAMES
#define V3F_LIGHTING_COMBO_RELEASE_FRAMES 8U
#endif

#define V3F_LIGHTING_COMBO_NONE   0U
#define V3F_LIGHTING_COMBO_TOGGLE 1U
#define V3F_LIGHTING_COMBO_EFFECT 2U

#define V3F_HID_USAGE_A           0x04U
#define V3F_HID_USAGE_RIGHT_ARROW 0x4FU
#define V3F_HID_USAGE_LEFT_ARROW  0x50U
#define V3F_HID_USAGE_DOWN_ARROW  0x51U
#define V3F_HID_USAGE_UP_ARROW    0x52U
#define V3F_HID_USAGE_ENTER       0x28U

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
    V3F_TRACE_RGB_RENDER_COUNT = 43,
    V3F_TRACE_RGB_ERROR_COUNT = 44,
    V3F_TRACE_RGB_LAST_RESULT = 45,
    V3F_TRACE_RGB_EFFECT = 46,
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

static void v3f_nkro_set_usage(uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                               uint8_t usage)
{
    if((nkro16 != 0) && (usage >= V3F_HID_USAGE_A))
    {
        uint8_t bit_index = (uint8_t)(usage - V3F_HID_USAGE_A);
        uint8_t byte_index = (uint8_t)(2U + (bit_index >> 3));
        uint8_t bit_mask = (uint8_t)(1U << (bit_index & 7U));

        if(byte_index < AIK_NKRO_REPORT_BYTES)
        {
            nkro16[byte_index] |= bit_mask;
        }
    }
}

static void v3f_apply_local_keyboard_controls(
    const aik_spi_half_state_v1_t *left,
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    if(left == 0)
    {
        return;
    }

    if(aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_UP) != 0U)
    {
        v3f_nkro_set_usage(nkro16, V3F_HID_USAGE_UP_ARROW);
    }
    if(aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_DOWN) != 0U)
    {
        v3f_nkro_set_usage(nkro16, V3F_HID_USAGE_DOWN_ARROW);
    }
    if(aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_RIGHT) != 0U)
    {
        v3f_nkro_set_usage(nkro16, V3F_HID_USAGE_RIGHT_ARROW);
    }
    if(aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_LEFT) != 0U)
    {
        v3f_nkro_set_usage(nkro16, V3F_HID_USAGE_LEFT_ARROW);
    }
    if(aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_CENTER) != 0U)
    {
        v3f_nkro_set_usage(nkro16, V3F_HID_USAGE_ENTER);
    }
}

static uint16_t v3f_consumer_usage_from_local_controls(
    const aik_spi_half_state_v1_t *right)
{
    if(right == 0)
    {
        return AIK_CONSUMER_USAGE_NONE;
    }
    if(aik_spi_half_bit_down(right, AIK_RIGHT_LOCAL_BIT_EC11_MUTE) != 0U)
    {
        return AIK_CONSUMER_USAGE_MUTE;
    }
    if(aik_spi_half_bit_down(right, AIK_RIGHT_LOCAL_BIT_EC11_CW) != 0U)
    {
        return AIK_CONSUMER_USAGE_VOLUME_UP;
    }
    if(aik_spi_half_bit_down(right, AIK_RIGHT_LOCAL_BIT_EC11_CCW) != 0U)
    {
        return AIK_CONSUMER_USAGE_VOLUME_DOWN;
    }
    return AIK_CONSUMER_USAGE_NONE;
}

static int8_t v3f_rotary_count_delta_from_right(
    const aik_spi_half_state_v1_t *right)
{
    static uint8_t valid;
    static uint8_t last_cw_count;
    static uint8_t last_ccw_count;
    uint8_t cw_count;
    uint8_t ccw_count;
    uint8_t cw_delta;
    uint8_t ccw_delta;

    if(right == 0)
    {
        valid = 0U;
        return 0;
    }

    cw_count = aik_spi_half_get_2bit(
        right,
        AIK_RIGHT_LOCAL_ROTARY_CW_COUNT_SHIFT);
    ccw_count = aik_spi_half_get_2bit(
        right,
        AIK_RIGHT_LOCAL_ROTARY_CCW_COUNT_SHIFT);

    if(valid == 0U)
    {
        last_cw_count = cw_count;
        last_ccw_count = ccw_count;
        valid = 1U;
        return 0;
    }

    cw_delta = (uint8_t)((cw_count - last_cw_count) &
                         AIK_RIGHT_LOCAL_ROTARY_COUNT_MASK);
    ccw_delta = (uint8_t)((ccw_count - last_ccw_count) &
                          AIK_RIGHT_LOCAL_ROTARY_COUNT_MASK);
    last_cw_count = cw_count;
    last_ccw_count = ccw_count;

    return (int8_t)((int8_t)cw_delta - (int8_t)ccw_delta);
}

static void v3f_consumer_delta_accumulate(int16_t *pending, int8_t delta)
{
    int16_t next;

    if((pending == 0) || (delta == 0))
    {
        return;
    }

    next = (int16_t)(*pending + delta);
    if(next > V3F_CONSUMER_PENDING_LIMIT)
    {
        next = V3F_CONSUMER_PENDING_LIMIT;
    }
    else if(next < -V3F_CONSUMER_PENDING_LIMIT)
    {
        next = -V3F_CONSUMER_PENDING_LIMIT;
    }
    *pending = next;
}

static int8_t v3f_consumer_delta_take_limited(int16_t *pending)
{
    int16_t value;

    if((pending == 0) || (*pending == 0))
    {
        return 0;
    }

    value = *pending;
    if(value > 8)
    {
        value = 8;
    }
    else if(value < -8)
    {
        value = -8;
    }
    return (int8_t)value;
}

static int8_t v3f_mouse_wheel_from_local_controls(
    const aik_spi_half_state_v1_t *left)
{
    if(left == 0)
    {
        return 0;
    }
    if(aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_UP) != 0U)
    {
        return 1;
    }
    if(aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_DOWN) != 0U)
    {
        return -1;
    }
    return 0;
}

static uint8_t v3f_output_mode_update_from_keys(v3f_global_key_state_t *keys,
                                                uint8_t current_mode)
{
    uint8_t next_mode = current_mode;

    if((keys != 0) &&
       (v3f_global_key_is_down(keys, V3F_FN_LAYER_KEY) != 0U))
    {
        if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F1) != 0U)
        {
            next_mode = AIK_OUTPUT_MODE_USBHS;
        }
        else if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F2) != 0U)
        {
            next_mode = AIK_OUTPUT_MODE_RF24;
        }
        else if(v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F3) != 0U)
        {
            next_mode = AIK_OUTPUT_MODE_BLE;
        }
    }

    return v3f_output_mode_sanitize(next_mode);
}

static uint8_t v3f_lighting_combo_from_keys(v3f_global_key_state_t *keys)
{
    uint8_t f5_down;
    uint8_t f6_down;

    if((keys == 0) ||
       (v3f_global_key_is_down(keys, V3F_FN_LAYER_KEY) == 0U))
    {
        return V3F_LIGHTING_COMBO_NONE;
    }

    f5_down = v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F5);
    f6_down = v3f_global_key_is_down(keys, V3F_SWITCH_KEY_F6);

    if((f5_down != 0U) && (f6_down == 0U))
    {
        return V3F_LIGHTING_COMBO_TOGGLE;
    }
    if((f6_down != 0U) && (f5_down == 0U))
    {
        return V3F_LIGHTING_COMBO_EFFECT;
    }
    return V3F_LIGHTING_COMBO_NONE;
}

static void v3f_lighting_update_from_keys(v3f_global_key_state_t *keys)
{
    static uint8_t armed = 1U;
    static uint8_t last_combo = V3F_LIGHTING_COMBO_NONE;
    static uint8_t stable_frames;
    uint8_t combo = v3f_lighting_combo_from_keys(keys);

    if(combo == last_combo)
    {
        if(stable_frames < 0xFFU)
        {
            stable_frames++;
        }
    }
    else
    {
        last_combo = combo;
        stable_frames = 1U;
    }

    if(combo == V3F_LIGHTING_COMBO_NONE)
    {
        if(stable_frames >= V3F_LIGHTING_COMBO_RELEASE_FRAMES)
        {
            armed = 1U;
        }
        return;
    }

    if((armed == 0U) ||
       (stable_frames < V3F_LIGHTING_COMBO_PRESS_FRAMES))
    {
        return;
    }

    if(combo == V3F_LIGHTING_COMBO_TOGGLE)
    {
        v3f_rgb_status_toggle_enabled();
    }
    else if(combo == V3F_LIGHTING_COMBO_EFFECT)
    {
        v3f_rgb_status_next_effect();
    }
    armed = 0U;
}

static void v3f_global_key_clear_one(v3f_global_key_state_t *keys,
                                     uint8_t key_id)
{
    if((keys != 0) && (key_id < AIK_KEY_COUNT_TOTAL))
    {
        keys->down[key_id >> 3] &= (uint8_t)~(uint8_t)(1U << (key_id & 7U));
    }
}

static uint8_t v3f_fn_consumed_get(const uint8_t consumed[V3F_GLOBAL_DOWN_BYTES],
                                   uint8_t key_id)
{
    return (uint8_t)((consumed[key_id >> 3] >> (key_id & 7U)) & 1U);
}

static void v3f_fn_consumed_set(uint8_t consumed[V3F_GLOBAL_DOWN_BYTES],
                                uint8_t key_id)
{
    consumed[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
}

static void v3f_fn_consumed_clear(uint8_t consumed[V3F_GLOBAL_DOWN_BYTES],
                                  uint8_t key_id)
{
    consumed[key_id >> 3] &= (uint8_t)~(uint8_t)(1U << (key_id & 7U));
}

static void v3f_fn_layer_consume_keys(v3f_global_key_state_t *keys)
{
    static uint8_t consumed[V3F_GLOBAL_DOWN_BYTES];
    uint8_t key_id;
    uint8_t fn_down;

    if(keys == 0)
    {
        return;
    }

    fn_down = v3f_global_key_is_down(keys, V3F_FN_LAYER_KEY);

    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        if(v3f_global_key_is_down(keys, key_id) == 0U)
        {
            v3f_fn_consumed_clear(consumed, key_id);
        }
    }

    if(fn_down != 0U)
    {
        for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
        {
            if(v3f_global_key_is_down(keys, key_id) != 0U)
            {
                v3f_fn_consumed_set(consumed, key_id);
            }
        }
    }

    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        if(v3f_fn_consumed_get(consumed, key_id) != 0U)
        {
            v3f_global_key_clear_one(keys, key_id);
        }
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

static void v3f_profile_status_poll(uint16_t host_seq)
{
#if V3F_ENABLE_PROFILE_STATUS_SYNC
#if V3F_PROFILE_STATUS_PERIOD_TICKS == 0
    (void)host_seq;
    return;
#else
    if((host_seq % V3F_PROFILE_STATUS_PERIOD_TICKS) != 31U)
    {
        return;
    }
    v3f_profile_sync_status_poll(host_seq);
#endif
#else
    (void)host_seq;
#endif
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
                                         uint8_t output_mode,
                                         uint16_t consumer_usage,
                                         int8_t consumer_delta)
{
#if V3F_ENABLE_SPI_HOST_CMD && V3F_ENABLE_RF_BRIDGE
    aik_spi_half_state_v1_t empty_right;

    if(right != 0)
    {
        v3f_rf_report_bridge_prepare_right_state_cmd(cmd,
                                                     host_seq,
                                                     right,
                                                     output_mode);
        aik_spi_host_cmd_set_consumer_usage(cmd, consumer_usage);
        aik_spi_host_cmd_set_consumer_delta(cmd, consumer_delta);
        aik_spi_host_cmd_finish(cmd);
        return;
    }

    memset(&empty_right, 0, sizeof(empty_right));
    empty_right.half_seq = host_seq;
    aik_spi_half_state_finish(&empty_right,
                              aik_spi_half_frame_bits(AIK_HALF_ID_RIGHT));
    v3f_rf_report_bridge_prepare_right_state_cmd(cmd,
                                                 host_seq,
                                                 &empty_right,
                                                 output_mode);
    aik_spi_host_cmd_set_consumer_usage(cmd, consumer_usage);
    aik_spi_host_cmd_set_consumer_delta(cmd, consumer_delta);
    aik_spi_host_cmd_finish(cmd);
#else
    v3f_prepare_spi_poll_tx(cmd, host_seq, 0, output_mode);
    (void)right;
    (void)consumer_usage;
    (void)consumer_delta;
#endif
}

static void v3f_cdc_debug_poll(uint16_t tick,
                               const v3f_half_cache_t *left,
                               const v3f_half_cache_t *right,
                               const v3f_global_key_state_t *keys,
                               const uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
#if V3F_ENABLE_USBFS_CDC_DEBUG && !V3F_ENABLE_USBFS_CDC
    /* The configuration protocol owns CDC IN while USBFS carries profiles. */
    static uint16_t last_tick;
    static uint8_t phase;
    v3f_ch585_link_stats_t left_stats;
    v3f_ch585_link_stats_t right_stats;
    char line[160];
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

        case 6:
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

        default:
            len = snprintf(line, sizeof(line),
                           "P H=%04x/%u L ok=%lu bad=%lu m=%u id=%04x g=%u f=%02x R ok=%lu bad=%lu m=%u id=%04x g=%u f=%02x\r\n",
                           (unsigned int)AIK_PROFILE_DEBUG_ID16,
                           (unsigned int)AIK_PROFILE_DEBUG_GENERATION16,
                           (unsigned long)left_stats.profile_status_ok,
                           (unsigned long)left_stats.profile_status_invalid,
                           (unsigned int)aik_spi_profile_status_matches_debug_profile(
                               &left_stats.last_profile_status,
                               AIK_HALF_ID_LEFT),
                           (unsigned int)left_stats.last_profile_status.profile_id16,
                           (unsigned int)left_stats.last_profile_status.generation16,
                           (unsigned int)left_stats.last_profile_status.flags,
                           (unsigned long)right_stats.profile_status_ok,
                           (unsigned long)right_stats.profile_status_invalid,
                           (unsigned int)aik_spi_profile_status_matches_debug_profile(
                               &right_stats.last_profile_status,
                               AIK_HALF_ID_RIGHT),
                           (unsigned int)right_stats.last_profile_status.profile_id16,
                           (unsigned int)right_stats.last_profile_status.generation16,
                           (unsigned int)right_stats.last_profile_status.flags);
            break;
    }

    phase = (uint8_t)((phase + 1U) % 8U);
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
    uint16_t last_usb_consumer_usage = AIK_CONSUMER_USAGE_NONE;
    uint16_t last_usb_consumer_tick = 0U;
    int8_t last_usb_mouse_wheel = 0;
    int16_t usb_consumer_delta_pending = 0;
    int16_t usb_mouse_wheel_pending = 0;
    int16_t wireless_consumer_delta_pending = 0;
    uint16_t wireless_consumer_usage_pending = AIK_CONSUMER_USAGE_NONE;
    uint16_t last_wireless_right_consumer_usage = AIK_CONSUMER_USAGE_NONE;
    uint8_t usb_nkro_release_pending = 0U;
    uint8_t usb_consumer_release_pending = 0U;

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(nkro16, 0, sizeof(nkro16));
    memset(zero_nkro16, 0, sizeof(zero_nkro16));

    v3f_board_init();
    /* Bring USB up before profile parsing so enumeration can isolate early boot faults. */
    v3f_usb_hid_nkro_init();
#if V3F_ENABLE_USBHS_8K && V3F_ENABLE_USBFS_CDC
    ch32h417_usbfs_hid_nkro_init();
    /* Keep the 8K HID interrupt ahead of CDC bulk traffic. */
    NVIC_SetPriority(USBHS_IRQn, 0x00U);
    NVIC_SetPriority(USBFS_IRQn, 0x80U);
#endif
    (void)v3f_profile_runtime_init();
    v3f_profile_sync_init();
    v3f_profile_sync_mark_all_dirty();
    v3f_ch585_link_init();
    v3f_rgb_status_init();
    v3f_rgb_status_set_enabled(0U);
    v3f_board_start_v5f();

    while(1)
    {
        uint8_t got_left;
        uint8_t got_right;
        uint8_t sync_mask = v3f_profile_sync_poll(host_seq);
        uint8_t previous_output_mode = output_mode;
        uint16_t consumer_usage;
        uint16_t right_consumer_usage = AIK_CONSUMER_USAGE_NONE;
        uint16_t left_push_consumer_usage = AIK_CONSUMER_USAGE_NONE;
        int8_t consumer_delta = 0;
        int8_t left_push_consumer_delta = 0;
        int8_t mouse_wheel;

#if V3F_ENABLE_RF_BRIDGE
        v3f_prepare_spi_poll_tx(&right_cmd,
                                host_seq,
                                0,
                                output_mode);
        got_right = ((sync_mask & (1U << AIK_HALF_ID_RIGHT)) == 0U) ?
                    v3f_ch585_link_poll(AIK_HALF_ID_RIGHT, &right_cmd, &rx) :
                    0U;
        update_half_cache(&right, got_right, &rx);
        age_half_cache_on_usb_report(&right, got_right);
        (void)v3f_rotary_count_delta_from_right(
            right.valid ? &right.frame : 0);
        consumer_delta = 0;
        right_consumer_usage = v3f_consumer_usage_from_local_controls(
            right.valid ? &right.frame : 0);

        if(v3f_output_mode_is_wireless(output_mode) != 0U)
        {
            v3f_consumer_delta_accumulate(&wireless_consumer_delta_pending,
                                          consumer_delta);
            if((right_consumer_usage != AIK_CONSUMER_USAGE_NONE) &&
               (last_wireless_right_consumer_usage == AIK_CONSUMER_USAGE_NONE) &&
               (wireless_consumer_usage_pending == AIK_CONSUMER_USAGE_NONE))
            {
                wireless_consumer_usage_pending = right_consumer_usage;
            }
            left_push_consumer_usage = wireless_consumer_usage_pending;
            left_push_consumer_delta =
                v3f_consumer_delta_take_limited(
                    &wireless_consumer_delta_pending);
            v3f_prepare_right_state_push(&left_cmd,
                                         host_seq,
                                         right.valid ? &right.frame : 0,
                                         output_mode,
                                         left_push_consumer_usage,
                                         left_push_consumer_delta);
            last_wireless_right_consumer_usage = right_consumer_usage;
        }
        else
        {
            v3f_prepare_spi_poll_tx(&left_cmd,
                                    host_seq,
                                    0,
                                    output_mode);
        }
        got_left = ((sync_mask & (1U << AIK_HALF_ID_LEFT)) == 0U) ?
                   v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &left_cmd, &rx) :
                   0U;
        update_half_cache(&left, got_left, &rx);
        age_half_cache_on_usb_report(&left, got_left);
        if((v3f_output_mode_is_wireless(output_mode) != 0U) &&
           (got_left != 0U))
        {
            wireless_consumer_delta_pending =
                (int16_t)(wireless_consumer_delta_pending -
                          left_push_consumer_delta);
            if(left_push_consumer_usage != AIK_CONSUMER_USAGE_NONE)
            {
                wireless_consumer_usage_pending = AIK_CONSUMER_USAGE_NONE;
            }
        }
#else
        v3f_prepare_spi_poll_tx(&left_cmd,
                                host_seq,
                                nkro16,
                                output_mode);
        v3f_prepare_spi_poll_tx(&right_cmd,
                                host_seq,
                                nkro16,
                                output_mode);

        got_left = ((sync_mask & (1U << AIK_HALF_ID_LEFT)) == 0U) ?
                   v3f_ch585_link_poll(AIK_HALF_ID_LEFT, &left_cmd, &rx) :
                   0U;
        update_half_cache(&left, got_left, &rx);

        got_right = ((sync_mask & (1U << AIK_HALF_ID_RIGHT)) == 0U) ?
                    v3f_ch585_link_poll(AIK_HALF_ID_RIGHT, &right_cmd, &rx) :
                    0U;
        update_half_cache(&right, got_right, &rx);
        (void)v3f_rotary_count_delta_from_right(
            right.valid ? &right.frame : 0);
        consumer_delta = 0;

        age_half_cache_on_usb_report(&left, got_left);
        age_half_cache_on_usb_report(&right, got_right);
#endif

        v3f_half_state_merge(left.valid ? &left.frame : 0,
                             right.valid ? &right.frame : 0,
                             &keys);
        if(v3f_profile_runtime_rearm_take() != 0U)
        {
            /* Table swap: neutralise held keys until re-pressed and
             * force one empty report so nothing sticks. */
            v3f_profile_runtime_rearm_latch(&keys);
            usb_nkro_release_pending = 1U;
            usb_consumer_release_pending = 1U;
        }
        output_mode = v3f_output_mode_update_from_keys(&keys, output_mode);
        if(v3f_output_mode_is_wireless(output_mode) == 0U)
        {
            wireless_consumer_delta_pending = 0;
            wireless_consumer_usage_pending = AIK_CONSUMER_USAGE_NONE;
            last_wireless_right_consumer_usage = AIK_CONSUMER_USAGE_NONE;
        }
        v3f_lighting_update_from_keys(&keys);
        if(v3f_profile_runtime_valid() != 0U)
        {
            if(v3f_profile_runtime_get()->has_fn_overlay == 0U)
            {
                v3f_fn_layer_consume_keys(&keys);
            }
            v3f_profile_runtime_build_nkro16(&keys, nkro16);
            v3f_profile_runtime_apply_local_keyboard(
                left.valid ? &left.frame : 0, nkro16);
            consumer_usage = v3f_profile_runtime_consumer_usage(
                right.valid ? &right.frame : 0);
            mouse_wheel = v3f_profile_runtime_mouse_wheel(
                left.valid ? &left.frame : 0);
        }
        else
        {
            v3f_fn_layer_consume_keys(&keys);
            v3f_default_profile_build_nkro16(&keys, nkro16);
            v3f_apply_local_keyboard_controls(left.valid ? &left.frame : 0,
                                              nkro16);
            consumer_usage = v3f_consumer_usage_from_local_controls(
                right.valid ? &right.frame : 0);
            mouse_wheel = v3f_mouse_wheel_from_local_controls(
                left.valid ? &left.frame : 0);
        }
        if(output_mode == AIK_OUTPUT_MODE_USBHS)
        {
            v3f_consumer_delta_accumulate(&usb_consumer_delta_pending,
                                          consumer_delta);
        }
        if((output_mode == AIK_OUTPUT_MODE_USBHS) &&
           (mouse_wheel != 0) &&
           (last_usb_mouse_wheel == 0))
        {
            if((mouse_wheel > 0) && (usb_mouse_wheel_pending < 127))
            {
                usb_mouse_wheel_pending++;
            }
            else if((mouse_wheel < 0) && (usb_mouse_wheel_pending > -127))
            {
                usb_mouse_wheel_pending--;
            }
        }
        if((previous_output_mode == AIK_OUTPUT_MODE_USBHS) &&
           (output_mode != AIK_OUTPUT_MODE_USBHS))
        {
            usb_nkro_release_pending = 1U;
            usb_consumer_release_pending = 1U;
            usb_consumer_delta_pending = 0;
            usb_mouse_wheel_pending = 0;
        }
        if(v3f_usb_hid_nkro_pending_empty() != 0U)
        {
            uint8_t consumer_rate_ready =
                ((uint16_t)(host_seq - last_usb_consumer_tick) >=
                 V3F_CONSUMER_EVENT_INTERVAL_TICKS) ? 1U : 0U;

            if(usb_nkro_release_pending != 0U)
            {
                if(v3f_usb_hid_nkro_submit(zero_nkro16) != 0U)
                {
                    usb_nkro_release_pending = 0U;
                }
            }
            else if(usb_consumer_release_pending != 0U)
            {
                if(v3f_usb_hid_nkro_submit_consumer(
                       AIK_CONSUMER_USAGE_NONE) != 0U)
                {
                    usb_consumer_release_pending = 0U;
                }
            }
            else if(output_mode == AIK_OUTPUT_MODE_USBHS)
            {
                if((consumer_usage != AIK_CONSUMER_USAGE_NONE) &&
                   (last_usb_consumer_usage == AIK_CONSUMER_USAGE_NONE))
                {
                    if(v3f_usb_hid_nkro_submit_consumer(consumer_usage) != 0U)
                    {
                        usb_consumer_release_pending = 1U;
                        last_usb_consumer_tick = host_seq;
                    }
                }
                else if((usb_consumer_delta_pending > 0) &&
                        (consumer_rate_ready != 0U))
                {
                    if(v3f_usb_hid_nkro_submit_consumer(
                           AIK_CONSUMER_USAGE_VOLUME_UP) != 0U)
                    {
                        usb_consumer_delta_pending--;
                        usb_consumer_release_pending = 1U;
                        last_usb_consumer_tick = host_seq;
                    }
                }
                else if((usb_consumer_delta_pending < 0) &&
                        (consumer_rate_ready != 0U))
                {
                    if(v3f_usb_hid_nkro_submit_consumer(
                           AIK_CONSUMER_USAGE_VOLUME_DOWN) != 0U)
                    {
                        usb_consumer_delta_pending++;
                        usb_consumer_release_pending = 1U;
                        last_usb_consumer_tick = host_seq;
                    }
                }
                else if(usb_mouse_wheel_pending > 0)
                {
                    if(v3f_usb_hid_nkro_submit_mouse_wheel(1) != 0U)
                    {
                        usb_mouse_wheel_pending--;
                    }
                }
                else if(usb_mouse_wheel_pending < 0)
                {
                    if(v3f_usb_hid_nkro_submit_mouse_wheel(-1) != 0U)
                    {
                        usb_mouse_wheel_pending++;
                    }
                }
                else
                {
                    (void)v3f_usb_hid_nkro_submit(nkro16);
                }
            }
        }
        if(output_mode == AIK_OUTPUT_MODE_USBHS)
        {
            last_usb_consumer_usage = consumer_usage;
            last_usb_mouse_wheel = mouse_wheel;
        }
        else
        {
            last_usb_consumer_usage = AIK_CONSUMER_USAGE_NONE;
            last_usb_mouse_wheel = 0;
        }

        v3f_trace_inc(V3F_TRACE_TICK);
        v3f_trace_set(V3F_TRACE_LEFT_OK, left.valid);
        v3f_trace_set(V3F_TRACE_RIGHT_OK, right.valid);
        v3f_trace_set(V3F_TRACE_LEFT_STALE, left.stale_ticks);
        v3f_trace_set(V3F_TRACE_RIGHT_STALE, right.stale_ticks);
        v3f_trace_set(V3F_TRACE_OUTPUT_MODE, output_mode);
        v3f_trace_set(V3F_TRACE_RGB_RENDER_COUNT, v3f_rgb_status_render_count());
        v3f_trace_set(V3F_TRACE_RGB_ERROR_COUNT, v3f_rgb_status_error_count());
        v3f_trace_set(V3F_TRACE_RGB_LAST_RESULT, v3f_rgb_status_last_result());
        v3f_trace_set(V3F_TRACE_RGB_EFFECT, v3f_rgb_status_effect());
        v3f_link_diag_trace(&left, &right);
        v3f_report_diag_trace(&keys, nkro16);
        v3f_usb_diag_trace();
        v3f_profile_status_poll(host_seq);
        v3f_pc_link_poll();
        v3f_cdc_debug_poll(host_seq, &left, &right, &keys, nkro16);
        v3f_rgb_status_task(host_seq);

        host_seq++;
#if V3F_ENABLE_RF_BRIDGE
        v3f_board_delay_us(V3F_USB_REPORT_INTERVAL_US);
#endif
    }
}
