#include "CONFIG.h"
#include "HAL.h"

#include <string.h>

#include "aik_approval_control.h"
#include "aik_host_shortcut.h"
#include "aik_spi_protocol.h"
#include "ch585_ads7948_mux_acq.h"
#include "ch585_board_config.h"
#include "ch585_half_report.h"
#include "ch585_profile.h"

#ifndef CH585_BLE_PAIRING_EXT_EEPROM
#define CH585_BLE_PAIRING_EXT_EEPROM 0
#endif
#if CH585_BLE_PAIRING_EXT_EEPROM
#include "ch585_eeprom_i2c.h"
#endif
#if CH585_BLE_HID_ENABLE
#include "ble_hid.h"
#endif
#include "ch585_rf_nkro_tx.h"
#include "ch585_spi0_slave_link.h"
#include "magnetic_key_engine.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#ifndef CH585_HALF_ID
#define CH585_HALF_ID AIK_HALF_ID_LEFT
#endif

#ifndef CH585_RF_TX_ENABLE
#define CH585_RF_TX_ENABLE 0
#endif

#ifndef CH585_BLE_HID_ENABLE
#define CH585_BLE_HID_ENABLE 0
#endif

#ifndef CH585_WIRELESS_MODE_SWITCH_ENABLE
#define CH585_WIRELESS_MODE_SWITCH_ENABLE \
    ((CH585_BOARD_HAS_WIRELESS_MODE_SWITCH != 0U) && \
     (CH585_HALF_ID == AIK_HALF_ID_LEFT) && \
     CH585_RF_TX_ENABLE && CH585_BLE_HID_ENABLE)
#endif

#ifndef CH585_SPI_ACCEPT_HOST_CMD
#define CH585_SPI_ACCEPT_HOST_CMD (CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE)
#endif

#ifndef CH585_HALF_SCAN_DEBUG_UART
#define CH585_HALF_SCAN_DEBUG_UART 0
#endif

#ifndef CH585_HALF_SCAN_DEBUG_UART_BAUD
#define CH585_HALF_SCAN_DEBUG_UART_BAUD 921600U
#endif

#ifndef CH585_HALF_SCAN_DEBUG_PERIOD_FRAMES
#define CH585_HALF_SCAN_DEBUG_PERIOD_FRAMES 100U
#endif

#ifndef CH585_LOCAL_ROTARY_PULSE_FRAMES
#define CH585_LOCAL_ROTARY_PULSE_FRAMES 6U
#endif

#ifndef CH585_LOCAL_WHEEL_PULSE_FRAMES
#define CH585_LOCAL_WHEEL_PULSE_FRAMES 32U
#endif

#ifndef CH585_LOCAL_ROTARY_QUAD_STEPS_PER_EVENT
#define CH585_LOCAL_ROTARY_QUAD_STEPS_PER_EVENT 2
#endif

#ifndef CH585_LOCAL_ROTARY_QUAD_STEPS_PER_EVENT_RIGHT
#define CH585_LOCAL_ROTARY_QUAD_STEPS_PER_EVENT_RIGHT 2
#endif

#ifndef CH585_LOCAL_SWITCH_DEBOUNCE_FRAMES
#define CH585_LOCAL_SWITCH_DEBOUNCE_FRAMES 3U
#endif

#ifndef CH585_LOCAL_SWITCH_ENTER_GUARD_FRAMES
#define CH585_LOCAL_SWITCH_ENTER_GUARD_FRAMES 50U
#endif

#ifndef CH585_LOCAL_SWITCH_ENTER_PULSE_FRAMES
#define CH585_LOCAL_SWITCH_ENTER_PULSE_FRAMES 6U
#endif

#ifndef CH585_LOCAL_BUTTON_DEBOUNCE_FRAMES
#define CH585_LOCAL_BUTTON_DEBOUNCE_FRAMES 12U
#endif

#ifndef CH585_LOCAL_BUTTON_PULSE_FRAMES
#define CH585_LOCAL_BUTTON_PULSE_FRAMES 6U
#endif

#ifndef CH585_LOCAL_BUTTON_COOLDOWN_FRAMES
#define CH585_LOCAL_BUTTON_COOLDOWN_FRAMES 100U
#endif

#ifndef CH585_WIRELESS_MODE_SWITCH_DEBOUNCE_FRAMES
/* The normal H417 poll cadence is 125 us, so 160 frames is about 20 ms. */
#define CH585_WIRELESS_MODE_SWITCH_DEBOUNCE_FRAMES 160U
#endif

#ifndef BLE_HID_KBD_REPORT_LEN
#define BLE_HID_KBD_REPORT_LEN 8U
#endif

#define CH585_LOCAL_SWITCH_CENTER_MASK 0x01U
#define CH585_LOCAL_SWITCH_UP_MASK     0x02U
#define CH585_LOCAL_SWITCH_DOWN_MASK   0x04U
#define CH585_LOCAL_SWITCH_RIGHT_MASK  0x08U
#define CH585_LOCAL_SWITCH_LEFT_MASK   0x10U
#define CH585_LOCAL_SWITCH_DIR_MASK    0x1EU

#define CH585_LOCAL_SWITCH_GESTURE_NONE    0U
#define CH585_LOCAL_SWITCH_GESTURE_PENDING 1U
#define CH585_LOCAL_SWITCH_GESTURE_ENTER   2U
#define CH585_LOCAL_SWITCH_GESTURE_DIR     3U

#define CH585_WIRELESS_MODE_BLE_PB11       GPIO_Pin_11
#define CH585_WIRELESS_MODE_RF24_PB10      GPIO_Pin_10
#define CH585_WIRELESS_MODE_BLE_ACTIVE     0x01U
#define CH585_WIRELESS_MODE_RF24_ACTIVE    0x02U
#define CH585_WIRELESS_MODE_INVALID        0xFFU

#if CH585_HALF_SCAN_DEBUG_UART && !defined(DEBUG)
#error CH585_HALF_SCAN_DEBUG_UART requires DEBUG=Debug_UART1 or another WCH DEBUG UART.
#endif

static ch585_ads7948_mux_acq_t s_acq;
static mag_key_engine_t s_engine;
static aik_spi_half_state_v1_t s_tx_frame __attribute__((aligned(4)));
static aik_spi_host_cmd_v1_t s_rx_cmd __attribute__((aligned(4)));
static aik_spi_profile_status_v1_t s_profile_status __attribute__((aligned(4)));
static aik_spi_profile_xfer_v1_t s_profile_xfer __attribute__((aligned(4)));
static aik_spi_half_state_v1_t s_right_frame __attribute__((aligned(4)));
static uint8_t s_rf_nkro16[AIK_NKRO_REPORT_BYTES];
#if CH585_BLE_HID_ENABLE
static uint8_t s_ble_boot8[BLE_HID_KBD_REPORT_LEN];
#endif
static uint16_t s_compact_raw[AIK_KEY_COUNT_RIGHT];
static int s_last_spi_result;
static uint8_t s_profile_status_pending;
static uint8_t s_profile_xfer_pending;
static uint8_t s_right_frame_valid;
static uint8_t s_local_rotary_last_ab;
static int8_t s_local_rotary_accum;
static uint8_t s_local_rotary_cw_frames;
static uint8_t s_local_rotary_ccw_frames;
static uint32_t s_local_rotary_cw_total;
static uint32_t s_local_rotary_ccw_total;
static uint8_t s_local_rotary_debug_last_raw_ab;
static uint32_t s_local_rotary_debug_edge_count;
static volatile uint32_t s_local_rotary_debug_irq_count;
static volatile uint16_t s_local_rotary_debug_irq_flags;
static int16_t s_local_mouse_wheel_pending;
static uint8_t s_local_button_last_raw;
static uint8_t s_local_button_stable;
static uint8_t s_local_button_last_stable;
static uint8_t s_local_button_raw_count;
static uint8_t s_local_button_mute_frames;
static uint8_t s_local_button_mute_cooldown;
static uint8_t s_local_button_press_latched;
static uint8_t s_local_switch_last_raw;
static uint8_t s_local_switch_stable;
static uint8_t s_local_switch_raw_count;
static uint8_t s_local_switch_gesture;
static uint8_t s_local_switch_guard_frames;
static uint8_t s_local_switch_enter_frames;
#if CH585_BLE_HID_ENABLE
static uint32_t s_ble_sent_count;
static uint32_t s_ble_drop_count;
static uint16_t s_ble_last_consumer_usage;
#endif
#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
static uint8_t s_output_mode;
static int8_t s_last_output_mouse_wheel;
#endif
#if CH585_WIRELESS_MODE_SWITCH_ENABLE
static uint8_t s_wireless_mode_switch_last_raw;
static uint8_t s_wireless_mode_switch_raw_count;
static uint8_t s_wireless_mode_switch_mode;
#endif

static ch585_ads7948_mux_side_t half_scan_side(void)
{
    return (CH585_HALF_ID == AIK_HALF_ID_RIGHT) ?
        CH585_ADS7948_MUX_SIDE_RIGHT :
        CH585_ADS7948_MUX_SIDE_LEFT;
}

static void half_scan_build_down_bits(void)
{
    uint8_t key;
    uint8_t key_count = aik_spi_half_key_count((uint8_t)CH585_HALF_ID);

    memset(s_tx_frame.down_bits, 0, sizeof(s_tx_frame.down_bits));
    for(key = 0U; key < key_count; key++)
    {
        const mag_key_state_t *state = mag_key_engine_state(&s_engine, key);

        if((state != 0) && (state->is_down != 0U))
        {
            s_tx_frame.down_bits[key >> 3] |= (uint8_t)(1U << (key & 7U));
        }
    }
}

static uint8_t half_scan_gpioa_pressed(uint32_t pin)
{
    return (GPIOA_ReadPortPin(pin) == 0U) ? 1U : 0U;
}

static uint8_t half_scan_gpiob_pressed(uint32_t pin)
{
    return (GPIOB_ReadPortPin(pin) == 0U) ? 1U : 0U;
}

#if CH585_WIRELESS_MODE_SWITCH_ENABLE
static uint8_t half_scan_wireless_mode_switch_read_raw(void)
{
    uint8_t raw = 0U;

    if(half_scan_gpiob_pressed(CH585_WIRELESS_MODE_BLE_PB11) != 0U)
    {
        raw |= CH585_WIRELESS_MODE_BLE_ACTIVE;
    }
    if(half_scan_gpiob_pressed(CH585_WIRELESS_MODE_RF24_PB10) != 0U)
    {
        raw |= CH585_WIRELESS_MODE_RF24_ACTIVE;
    }
    return raw;
}

static uint8_t half_scan_wireless_mode_switch_decode(uint8_t raw)
{
    if(raw == CH585_WIRELESS_MODE_BLE_ACTIVE)
    {
        return AIK_OUTPUT_MODE_BLE;
    }
    if(raw == CH585_WIRELESS_MODE_RF24_ACTIVE)
    {
        return AIK_OUTPUT_MODE_RF24;
    }
    if(raw == 0U)
    {
        /* Center detent: neither externally pulled-up input is grounded. */
        return AIK_OUTPUT_MODE_USBHS;
    }
    /* Both low is not a valid position for this single-pole switch. */
    return CH585_WIRELESS_MODE_INVALID;
}

static void half_scan_wireless_mode_switch_state_init(void)
{
    uint8_t raw = half_scan_wireless_mode_switch_read_raw();
    uint8_t mode = half_scan_wireless_mode_switch_decode(raw);

    s_wireless_mode_switch_last_raw = raw;
    s_wireless_mode_switch_raw_count = 0U;
    /* Fail safely to wired mode if both contacts are unexpectedly low at
     * boot; later invalid samples retain the last valid position. */
    s_wireless_mode_switch_mode =
        (mode == CH585_WIRELESS_MODE_INVALID) ?
        AIK_OUTPUT_MODE_USBHS : mode;
}

static void half_scan_wireless_mode_switch_poll(void)
{
    uint8_t raw = half_scan_wireless_mode_switch_read_raw();
    uint8_t mode;

    if(raw != s_wireless_mode_switch_last_raw)
    {
        s_wireless_mode_switch_last_raw = raw;
        s_wireless_mode_switch_raw_count = 1U;
        return;
    }

    if(s_wireless_mode_switch_raw_count <
       CH585_WIRELESS_MODE_SWITCH_DEBOUNCE_FRAMES)
    {
        s_wireless_mode_switch_raw_count++;
    }
    if(s_wireless_mode_switch_raw_count <
       CH585_WIRELESS_MODE_SWITCH_DEBOUNCE_FRAMES)
    {
        return;
    }

    mode = half_scan_wireless_mode_switch_decode(raw);
    if(mode != CH585_WIRELESS_MODE_INVALID)
    {
        s_wireless_mode_switch_mode = mode;
    }
}
#endif

static uint8_t half_scan_read_rotary_ab(void)
{
    uint8_t a = half_scan_gpioa_pressed(GPIO_Pin_10);
    uint8_t b = half_scan_gpioa_pressed(GPIO_Pin_11);

    return (uint8_t)(a | (uint8_t)(b << 1));
}

static uint8_t half_scan_read_rotary_raw_ab(void)
{
    uint8_t a = (GPIOA_ReadPortPin(GPIO_Pin_10) != 0U) ? 1U : 0U;
    uint8_t b = (GPIOA_ReadPortPin(GPIO_Pin_11) != 0U) ? 1U : 0U;

    return (uint8_t)(a | (uint8_t)(b << 1));
}

static void half_scan_local_gpio_init(void)
{
    GPIOADigitalCfg(ENABLE, GPIO_Pin_7 | GPIO_Pin_10 | GPIO_Pin_11);
    GPIOBDigitalCfg(ENABLE, GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);
    GPIOA_ModeCfg(GPIO_Pin_10 | GPIO_Pin_11, GPIO_ModeIN_PU);
    s_local_rotary_debug_last_raw_ab = half_scan_read_rotary_raw_ab();
    s_local_rotary_debug_edge_count = 0U;
    s_local_rotary_debug_irq_count = 0U;
    s_local_rotary_debug_irq_flags = 0U;
#if CH585_HALF_SCAN_DEBUG_UART
    GPIOA_ClearITFlagBit(GPIO_Pin_10 | GPIO_Pin_11);
    GPIOA_ITModeCfg(GPIO_Pin_10 | GPIO_Pin_11, GPIO_ITMode_FallEdge);
    PFIC_EnableIRQ(GPIO_A_IRQn);
#endif
    if(CH585_HALF_ID == AIK_HALF_ID_LEFT)
    {
        GPIOA_ModeCfg(GPIO_Pin_7, GPIO_ModeIN_PU);
        GPIOB_ModeCfg(GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7,
                      GPIO_ModeIN_PU);
    }
    else
    {
        GPIOA_ModeCfg(GPIO_Pin_7, GPIO_ModeIN_PU);
    }

#if CH585_WIRELESS_MODE_SWITCH_ENABLE
    /* PB10/PB11 are USBFS D-/D+ when RB_PIN_USB_EN is set. Keep that analog
     * function disabled and use floating digital inputs: R31/R50 provide the
     * external pull-ups and SW1 grounds the selected contact. PB11 is also an
     * optional boot pin, so production option bytes must retain default PB22. */
    GPIOAGPPCfg(DISABLE, RB_PIN_USB_EN);
    GPIOBDigitalCfg(ENABLE,
                    CH585_WIRELESS_MODE_BLE_PB11 |
                        CH585_WIRELESS_MODE_RF24_PB10);
    GPIOB_ModeCfg(CH585_WIRELESS_MODE_BLE_PB11 |
                      CH585_WIRELESS_MODE_RF24_PB10,
                  GPIO_ModeIN_Floating);
    half_scan_wireless_mode_switch_state_init();
#endif

    s_local_rotary_last_ab = half_scan_read_rotary_ab();
    s_local_rotary_accum = 0;
    s_local_rotary_cw_frames = 0U;
    s_local_rotary_ccw_frames = 0U;
    s_local_rotary_cw_total = 0U;
    s_local_rotary_ccw_total = 0U;
    s_local_mouse_wheel_pending = 0;
    s_local_button_last_raw = 0U;
    s_local_button_stable = 0U;
    s_local_button_last_stable = 0U;
    s_local_button_raw_count = 0U;
    s_local_button_mute_frames = 0U;
    s_local_button_mute_cooldown = 0U;
    s_local_button_press_latched = 0U;
    s_local_switch_last_raw = 0U;
    s_local_switch_stable = 0U;
    s_local_switch_raw_count = 0U;
    s_local_switch_gesture = CH585_LOCAL_SWITCH_GESTURE_NONE;
    s_local_switch_guard_frames = 0U;
    s_local_switch_enter_frames = 0U;
}

static void half_scan_queue_local_mouse_wheel(int8_t delta)
{
    if(CH585_HALF_ID != AIK_HALF_ID_LEFT)
    {
        return;
    }

    if((delta > 0) && (s_local_mouse_wheel_pending < 32))
    {
        s_local_mouse_wheel_pending++;
    }
    else if((delta < 0) && (s_local_mouse_wheel_pending > -32))
    {
        s_local_mouse_wheel_pending--;
    }
}

#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
static int8_t half_scan_take_local_mouse_wheel(void)
{
    if(CH585_HALF_ID != AIK_HALF_ID_LEFT)
    {
        return 0;
    }

    if(s_local_mouse_wheel_pending > 0)
    {
        s_local_mouse_wheel_pending--;
        return ch585_half_report_mouse_wheel_step(
            AIK_HP_SIGNAL_WHEEL_UP);
    }
    if(s_local_mouse_wheel_pending < 0)
    {
        s_local_mouse_wheel_pending++;
        return ch585_half_report_mouse_wheel_step(
            AIK_HP_SIGNAL_WHEEL_DOWN);
    }
    return 0;
}
#endif

static uint8_t half_scan_debounce_u8(uint8_t raw,
                                     uint8_t mask,
                                     uint8_t threshold,
                                     uint8_t *last_raw,
                                     uint8_t *raw_count,
                                     uint8_t *stable)
{
    raw &= mask;
    if(raw == *last_raw)
    {
        if(*raw_count < threshold)
        {
            (*raw_count)++;
        }
    }
    else
    {
        *last_raw = raw;
        *raw_count = 1U;
    }

    if(*raw_count >= threshold)
    {
        *stable = raw;
    }
    return *stable;
}

static void half_scan_local_rotary_poll(void)
{
    static const int8_t s_quad_delta[16] =
    {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };
    uint8_t raw_ab = half_scan_read_rotary_raw_ab();
    uint8_t next_ab = half_scan_read_rotary_ab();
    uint8_t index = (uint8_t)((s_local_rotary_last_ab << 2) | next_ab);
    uint8_t pulse_frames =
        (CH585_HALF_ID == AIK_HALF_ID_LEFT) ?
        CH585_LOCAL_WHEEL_PULSE_FRAMES :
        CH585_LOCAL_ROTARY_PULSE_FRAMES;
    int8_t steps_per_event =
        (CH585_HALF_ID == AIK_HALF_ID_RIGHT) ?
        (int8_t)CH585_LOCAL_ROTARY_QUAD_STEPS_PER_EVENT_RIGHT :
        (int8_t)CH585_LOCAL_ROTARY_QUAD_STEPS_PER_EVENT;

    if(raw_ab != s_local_rotary_debug_last_raw_ab)
    {
        s_local_rotary_debug_last_raw_ab = raw_ab;
        s_local_rotary_debug_edge_count++;
    }

    s_local_rotary_last_ab = next_ab;
    s_local_rotary_accum =
        (int8_t)(s_local_rotary_accum + s_quad_delta[index & 0x0FU]);

    if(s_local_rotary_accum >= steps_per_event)
    {
        s_local_rotary_accum = 0;
        s_local_rotary_cw_frames = pulse_frames;
        s_local_rotary_ccw_frames = 0U;
        s_local_rotary_cw_total++;
        half_scan_queue_local_mouse_wheel(1);
    }
    else if(s_local_rotary_accum <= -steps_per_event)
    {
        s_local_rotary_accum = 0;
        s_local_rotary_ccw_frames = pulse_frames;
        s_local_rotary_cw_frames = 0U;
        s_local_rotary_ccw_total++;
        half_scan_queue_local_mouse_wheel(-1);
    }
}

static uint8_t half_scan_local_switch_stable_mask(void)
{
    uint8_t raw = 0U;

    if(half_scan_gpioa_pressed(GPIO_Pin_7) != 0U)
    {
        raw |= CH585_LOCAL_SWITCH_CENTER_MASK;
    }
    if(half_scan_gpiob_pressed(GPIO_Pin_7) != 0U)
    {
        raw |= CH585_LOCAL_SWITCH_UP_MASK;
    }
    if(half_scan_gpiob_pressed(GPIO_Pin_6) != 0U)
    {
        raw |= CH585_LOCAL_SWITCH_DOWN_MASK;
    }
    if(half_scan_gpiob_pressed(GPIO_Pin_5) != 0U)
    {
        raw |= CH585_LOCAL_SWITCH_RIGHT_MASK;
    }
    if(half_scan_gpiob_pressed(GPIO_Pin_4) != 0U)
    {
        raw |= CH585_LOCAL_SWITCH_LEFT_MASK;
    }

    return half_scan_debounce_u8(raw,
                                 CH585_LOCAL_SWITCH_CENTER_MASK |
                                     CH585_LOCAL_SWITCH_DIR_MASK,
                                 CH585_LOCAL_SWITCH_DEBOUNCE_FRAMES,
                                 &s_local_switch_last_raw,
                                 &s_local_switch_raw_count,
                                 &s_local_switch_stable);
}

static void half_scan_apply_left_five_way(void)
{
    uint8_t stable = half_scan_local_switch_stable_mask();
    uint8_t center = (uint8_t)(stable & CH585_LOCAL_SWITCH_CENTER_MASK);
    uint8_t dirs = (uint8_t)(stable & CH585_LOCAL_SWITCH_DIR_MASK);

    if(center == 0U)
    {
        if((s_local_switch_gesture ==
                CH585_LOCAL_SWITCH_GESTURE_PENDING) &&
           (dirs == 0U))
        {
            /* Preserve a fast center-only tap that released during the
             * direction-classification window. */
            s_local_switch_enter_frames =
                CH585_LOCAL_SWITCH_ENTER_PULSE_FRAMES;
        }
        else if(s_local_switch_gesture !=
                CH585_LOCAL_SWITCH_GESTURE_NONE)
        {
            s_local_switch_enter_frames = 0U;
        }
        s_local_switch_gesture = CH585_LOCAL_SWITCH_GESTURE_NONE;
        s_local_switch_guard_frames = 0U;
        if(s_local_switch_enter_frames != 0U)
        {
            aik_spi_half_set_bit(
                &s_tx_frame,
                AIK_LEFT_LOCAL_BIT_SCR_CENTER_QUALIFIED);
            aik_spi_half_set_bit(
                &s_tx_frame,
                AIK_LEFT_LOCAL_BIT_SCR_CENTER);
            s_local_switch_enter_frames--;
        }
        return;
    }

    if(s_local_switch_gesture == CH585_LOCAL_SWITCH_GESTURE_NONE)
    {
        s_local_switch_enter_frames = 0U;
        s_local_switch_gesture = CH585_LOCAL_SWITCH_GESTURE_PENDING;
        s_local_switch_guard_frames = CH585_LOCAL_SWITCH_ENTER_GUARD_FRAMES;
    }

    if(s_local_switch_gesture == CH585_LOCAL_SWITCH_GESTURE_PENDING)
    {
        if(dirs != 0U)
        {
            s_local_switch_gesture = CH585_LOCAL_SWITCH_GESTURE_DIR;
        }
        else if(s_local_switch_guard_frames != 0U)
        {
            s_local_switch_guard_frames--;
            return;
        }
        else
        {
            s_local_switch_gesture = CH585_LOCAL_SWITCH_GESTURE_ENTER;
            s_local_switch_enter_frames = CH585_LOCAL_SWITCH_ENTER_PULSE_FRAMES;
        }
    }

    if(s_local_switch_gesture == CH585_LOCAL_SWITCH_GESTURE_ENTER)
    {
        aik_spi_half_set_bit(
            &s_tx_frame,
            AIK_LEFT_LOCAL_BIT_SCR_CENTER_QUALIFIED);
        if(s_local_switch_enter_frames != 0U)
        {
            aik_spi_half_set_bit(&s_tx_frame, AIK_LEFT_LOCAL_BIT_SCR_CENTER);
            s_local_switch_enter_frames--;
        }
        return;
    }

    if(s_local_switch_gesture != CH585_LOCAL_SWITCH_GESTURE_DIR)
    {
        return;
    }

    if((dirs & CH585_LOCAL_SWITCH_UP_MASK) != 0U)
    {
        aik_spi_half_set_bit(&s_tx_frame, AIK_LEFT_LOCAL_BIT_SCR_UP);
    }
    if((dirs & CH585_LOCAL_SWITCH_DOWN_MASK) != 0U)
    {
        aik_spi_half_set_bit(&s_tx_frame, AIK_LEFT_LOCAL_BIT_SCR_DOWN);
    }
    if((dirs & CH585_LOCAL_SWITCH_RIGHT_MASK) != 0U)
    {
        aik_spi_half_set_bit(&s_tx_frame, AIK_LEFT_LOCAL_BIT_SCR_RIGHT);
    }
    if((dirs & CH585_LOCAL_SWITCH_LEFT_MASK) != 0U)
    {
        aik_spi_half_set_bit(&s_tx_frame, AIK_LEFT_LOCAL_BIT_SCR_LEFT);
    }
}

static void half_scan_local_button_poll(void)
{
    uint8_t stable = half_scan_debounce_u8(half_scan_gpioa_pressed(GPIO_Pin_7),
                                          1U,
                                          CH585_LOCAL_BUTTON_DEBOUNCE_FRAMES,
                                          &s_local_button_last_raw,
                                          &s_local_button_raw_count,
                                          &s_local_button_stable);

    if(s_local_button_mute_cooldown != 0U)
    {
        s_local_button_mute_cooldown--;
    }

    if(stable == 0U)
    {
        s_local_button_press_latched = 0U;
    }
    else if((s_local_button_press_latched == 0U) &&
            (s_local_button_last_stable == 0U) &&
            (s_local_button_mute_cooldown == 0U))
    {
        s_local_button_mute_frames = CH585_LOCAL_BUTTON_PULSE_FRAMES;
        s_local_button_mute_cooldown = CH585_LOCAL_BUTTON_COOLDOWN_FRAMES;
        s_local_button_press_latched = 1U;
    }
    s_local_button_last_stable = stable;
}

static void half_scan_encode_right_rotary_counts(void)
{
    aik_spi_half_set_2bit(
        &s_tx_frame,
        AIK_RIGHT_LOCAL_ROTARY_CW_COUNT_SHIFT,
        (uint8_t)(s_local_rotary_cw_total &
                  AIK_RIGHT_LOCAL_ROTARY_COUNT_MASK));
    aik_spi_half_set_2bit(
        &s_tx_frame,
        AIK_RIGHT_LOCAL_ROTARY_CCW_COUNT_SHIFT,
        (uint8_t)(s_local_rotary_ccw_total &
                  AIK_RIGHT_LOCAL_ROTARY_COUNT_MASK));
}

static void half_scan_apply_local_inputs(void)
{
    if(CH585_HALF_ID == AIK_HALF_ID_LEFT)
    {
        half_scan_local_rotary_poll();
        half_scan_apply_left_five_way();
        if(s_local_rotary_cw_frames != 0U)
        {
            aik_spi_half_set_bit(&s_tx_frame, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_UP);
            s_local_rotary_cw_frames--;
        }
        if(s_local_rotary_ccw_frames != 0U)
        {
            aik_spi_half_set_bit(&s_tx_frame, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_DOWN);
            s_local_rotary_ccw_frames--;
        }
#if CH585_WIRELESS_MODE_SWITCH_ENABLE
        half_scan_wireless_mode_switch_poll();
        aik_spi_left_set_output_mode(&s_tx_frame,
                                     s_wireless_mode_switch_mode);
#endif
    }
    else
    {
        half_scan_local_rotary_poll();
        half_scan_local_button_poll();
        half_scan_encode_right_rotary_counts();
        if(s_local_rotary_cw_frames != 0U)
        {
            aik_spi_half_set_bit(&s_tx_frame, AIK_RIGHT_LOCAL_BIT_EC11_CW);
            s_local_rotary_cw_frames--;
        }
        if(s_local_rotary_ccw_frames != 0U)
        {
            aik_spi_half_set_bit(&s_tx_frame, AIK_RIGHT_LOCAL_BIT_EC11_CCW);
            s_local_rotary_ccw_frames--;
        }
        if(s_local_button_mute_frames != 0U)
        {
            aik_spi_half_set_bit(&s_tx_frame, AIK_RIGHT_LOCAL_BIT_EC11_MUTE);
            s_local_button_mute_frames--;
        }
    }
}

static void half_scan_compact_raw(const ch585_ads7948_mux_acq_t *acq,
                                  uint16_t *compact,
                                  uint8_t key_count)
{
    const uint16_t *raw;
    const ch585_ads7948_mux_profile_t *profile;
    uint8_t key = 0U;
    uint8_t lane_index;

    if((acq == 0) || (compact == 0) || (acq->profile == 0))
    {
        return;
    }

    raw = ch585_ads7948_mux_acq_raw(acq);
    profile = acq->profile;
    for(lane_index = 0U;
        (lane_index < CH585_ADS7948_MUX_LANE_COUNT) && (key < key_count);
        lane_index++)
    {
        const ch585_ads7948_mux_lane_t *lane = &profile->lanes[lane_index];
        uint8_t mux;
        uint8_t mux_end = (uint8_t)(lane->mux_first + lane->mux_count);

        for(mux = lane->mux_first; (mux < mux_end) && (key < key_count); mux++)
        {
            uint16_t raw_index =
                ((uint16_t)lane_index * CH585_ADS7948_MUX_MUX_CHANNEL_COUNT) +
                (uint16_t)mux;

            compact[key++] = raw[raw_index];
        }
    }

    while(key < key_count)
    {
        compact[key++] = 0U;
    }
}

static void half_scan_build_frame(void)
{
    s_tx_frame.half_seq++;
    half_scan_build_down_bits();
    half_scan_apply_local_inputs();
    aik_spi_half_state_finish(&s_tx_frame,
                              aik_spi_half_frame_bits((uint8_t)CH585_HALF_ID));
}

static void half_scan_prepare_profile_status(uint16_t ack_seq)
{
    memset(&s_profile_status, 0, sizeof(s_profile_status));
    s_profile_status.ack_seq = ack_seq;
    s_profile_status.half_id = (uint8_t)CH585_HALF_ID;
    ch585_profile_fill_status(&s_profile_status);
    aik_spi_profile_status_finish(&s_profile_status);
    s_profile_status_pending = 1U;
}

static void half_scan_prepare_profile_xfer(void)
{
    ch585_profile_fill_xfer(&s_profile_xfer);
    s_profile_xfer_pending = 1U;
}

#if CH585_BLE_HID_ENABLE
static uint8_t half_scan_nkro_usage_down(
    const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
    uint8_t usage)
{
    uint8_t bit_index;
    uint8_t byte_index;

    if((usage < 0x04U) || (usage > 0x73U))
    {
        return 0U;
    }
    bit_index = (uint8_t)(usage - 0x04U);
    byte_index = (uint8_t)(2U + (bit_index >> 3));
    if(byte_index >= AIK_NKRO_REPORT_BYTES)
    {
        return 0U;
    }
    return (uint8_t)(
        (nkro16[byte_index] >> (bit_index & 7U)) & 1U);
}

static void half_scan_nkro16_to_boot8(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                      uint8_t boot8[BLE_HID_KBD_REPORT_LEN])
{
    static const uint8_t priority_usages[3] = {
        AIK_APPROVAL_CONTROL_HID_USAGE_YES,
        AIK_APPROVAL_CONTROL_HID_USAGE_NO,
        AIK_HOST_SHORTCUT_HID_USAGE_F24
    };
    uint8_t byte_index;
    uint8_t out_index = 2U;
    uint8_t priority_index;

    memset(boot8, 0, BLE_HID_KBD_REPORT_LEN);
    boot8[0] = nkro16[0];
    for(priority_index = 0U;
        (priority_index < 3U) &&
        (out_index < BLE_HID_KBD_REPORT_LEN);
        priority_index++)
    {
        if(half_scan_nkro_usage_down(
               nkro16, priority_usages[priority_index]) != 0U)
        {
            boot8[out_index++] = priority_usages[priority_index];
        }
    }

    for(byte_index = 2U;
        (byte_index < AIK_NKRO_REPORT_BYTES) && (out_index < BLE_HID_KBD_REPORT_LEN);
        byte_index++)
    {
        uint8_t bits = nkro16[byte_index];
        uint8_t bit;

        for(bit = 0U; (bit < 8U) && (out_index < BLE_HID_KBD_REPORT_LEN); bit++)
        {
            if((bits & (uint8_t)(1U << bit)) != 0U)
            {
                uint8_t usage =
                    (uint8_t)(0x04U + ((byte_index - 2U) * 8U) + bit);

                if((usage != AIK_APPROVAL_CONTROL_HID_USAGE_YES) &&
                   (usage != AIK_APPROVAL_CONTROL_HID_USAGE_NO) &&
                   (usage != AIK_HOST_SHORTCUT_HID_USAGE_F24))
                {
                    boot8[out_index++] = usage;
                }
            }
        }
    }
}

static void half_scan_ble_send_consumer_events(uint16_t consumer_usage,
                                               int8_t consumer_delta)
{
    int8_t delta = consumer_delta;

    while(delta > 0)
    {
        if(BLE_HID_TriggerConsumerTap(AIK_CONSUMER_USAGE_VOLUME_UP) != SUCCESS)
        {
            s_ble_drop_count++;
            break;
        }
        delta--;
    }

    while(delta < 0)
    {
        if(BLE_HID_TriggerConsumerTap(AIK_CONSUMER_USAGE_VOLUME_DOWN) != SUCCESS)
        {
            s_ble_drop_count++;
            break;
        }
        delta++;
    }

    if((consumer_delta == 0) &&
       (consumer_usage != AIK_CONSUMER_USAGE_NONE) &&
       (s_ble_last_consumer_usage == AIK_CONSUMER_USAGE_NONE))
    {
        if(BLE_HID_TriggerConsumerTap(consumer_usage) != SUCCESS)
        {
            s_ble_drop_count++;
        }
    }

    s_ble_last_consumer_usage = consumer_usage;
}
#endif

#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
static uint8_t half_scan_sanitize_output_mode(uint8_t mode)
{
    if((mode == AIK_OUTPUT_MODE_RF24) || (mode == AIK_OUTPUT_MODE_BLE))
    {
        return mode;
    }
    return AIK_OUTPUT_MODE_USBHS;
}

static uint8_t half_scan_host_output_mode(uint8_t cmd, uint8_t flags)
{
    uint8_t mode = half_scan_sanitize_output_mode(
        (uint8_t)(flags & AIK_SPI_FLAG_OUTPUT_MODE_MASK));

#if CH585_RF_TX_ENABLE && !CH585_BLE_HID_ENABLE
    if((cmd == AIK_SPI_CMD_POLL_WITH_RF) ||
       (cmd == AIK_SPI_CMD_PUSH_RIGHT_STATE))
    {
        mode = AIK_OUTPUT_MODE_RF24;
    }
#else
    (void)cmd;
#endif

    return mode;
}

static void half_scan_radio_handoff_delay(void)
{
    uint8_t i;

    for(i = 0U; i < 4U; i++)
    {
        TMOS_SystemProcess();
        mDelaymS(1);
    }
}

static void half_scan_set_output_mode(uint8_t mode)
{
    mode = half_scan_sanitize_output_mode(mode);
    if(s_output_mode == mode)
    {
        return;
    }

#if CH585_BLE_HID_ENABLE
    /* Pause calibration before every handoff; wireless resumes below. */
    HAL_RadioCalibrationSetEnabled(0U);
#endif

    if(mode == AIK_OUTPUT_MODE_RF24)
    {
#if CH585_BLE_HID_ENABLE
        BLE_HID_DisableForRadio(150U);
        half_scan_radio_handoff_delay();
#endif
#if CH585_RF_TX_ENABLE
        ch585_rf_nkro_tx_set_enabled(1U);
#endif
    }
    else if(mode == AIK_OUTPUT_MODE_BLE)
    {
#if CH585_RF_TX_ENABLE
        ch585_rf_nkro_tx_set_enabled(0U);
#endif
#if CH585_BLE_HID_ENABLE
        BLE_HID_SetEnabled(1U);
#endif
    }
    else
    {
#if CH585_RF_TX_ENABLE
        /*
         * Do not spend the RF release burst between the SPI command and
         * its response. TMR0 sends those zero reports in the background
         * after this command has returned to the link loop.
         */
        ch585_rf_nkro_tx_disable_async();
#endif
#if CH585_BLE_HID_ENABLE
        BLE_HID_SetEnabled(0U);
#endif
    }

#if CH585_BLE_HID_ENABLE
    if(mode != AIK_OUTPUT_MODE_USBHS)
    {
        /* Resume later; never run calibration inside this SPI command. */
        HAL_RadioCalibrationSetEnabled(1U);
    }
#endif

#if CH585_BLE_HID_ENABLE
    if(mode != AIK_OUTPUT_MODE_BLE)
    {
        memset(s_ble_boot8, 0, sizeof(s_ble_boot8));
        s_ble_last_consumer_usage = AIK_CONSUMER_USAGE_NONE;
    }
#endif
    s_output_mode = mode;
}

static void half_scan_output_nkro16(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                    uint16_t consumer_usage,
                                    int8_t consumer_delta,
                                    int8_t mouse_wheel,
                                    uint16_t host_seq,
                                    uint8_t output_mode)
{
    int8_t mouse_wheel_event = mouse_wheel;

    half_scan_set_output_mode(output_mode);

#if CH585_RF_TX_ENABLE
    if(s_output_mode == AIK_OUTPUT_MODE_RF24)
    {
        ch585_rf_nkro_tx_set_report(nkro16,
                                    consumer_usage,
                                    consumer_delta,
                                    mouse_wheel_event,
                                    host_seq,
                                    s_output_mode);
    }
#else
    (void)host_seq;
    (void)consumer_usage;
    (void)consumer_delta;
    (void)mouse_wheel_event;
#endif

#if CH585_BLE_HID_ENABLE
    if(s_output_mode == AIK_OUTPUT_MODE_BLE)
    {
        uint8_t next_boot8[BLE_HID_KBD_REPORT_LEN];

        half_scan_nkro16_to_boot8(nkro16, next_boot8);
        half_scan_ble_send_consumer_events(consumer_usage, consumer_delta);

        if(memcmp(s_ble_boot8, next_boot8, sizeof(s_ble_boot8)) != 0)
        {
            uint8_t status = BLE_HID_SendKeyboard(next_boot8);

            if(status == SUCCESS)
            {
                memcpy(s_ble_boot8, next_boot8, sizeof(s_ble_boot8));
                s_ble_sent_count++;
            }
            else
            {
                s_ble_drop_count++;
            }
        }
    }
#else
    (void)output_mode;
    (void)consumer_delta;
    (void)mouse_wheel;
#endif

    s_last_output_mouse_wheel = mouse_wheel;
}
#endif

static void half_scan_apply_host_cmd(void)
{
    if(aik_spi_host_cmd_valid(&s_rx_cmd) == 0U)
    {
        return;
    }

    if(s_rx_cmd.cmd == AIK_SPI_CMD_GET_PROFILE_STATUS)
    {
        half_scan_prepare_profile_status(s_rx_cmd.host_seq);
        return;
    }

    if((s_rx_cmd.cmd >= AIK_SPI_CMD_PROFILE_BEGIN) &&
       (s_rx_cmd.cmd <= AIK_SPI_CMD_PROFILE_GET_XFER))
    {
        uint8_t activates = (uint8_t)(
            s_rx_cmd.cmd == AIK_SPI_CMD_PROFILE_SET_ACTIVE);

        if(s_rx_cmd.cmd == AIK_SPI_CMD_PROFILE_COMMIT)
        {
            aik_spi_profile_commit_v1_t commit;

            aik_spi_host_cmd_get_payload(&s_rx_cmd, &commit,
                                         sizeof(commit));
            activates = (uint8_t)(
                (commit.patch_flags &
                 AIK_SPI_PROFILE_FLAG_ACTIVATE) != 0U);
        }
        ch585_profile_handle_transfer_cmd(&s_rx_cmd, &s_engine);
        half_scan_prepare_profile_xfer();
        if((activates != 0U) &&
           (s_profile_xfer.state == AIK_SPI_XFER_STATE_DONE))
        {
            s_local_mouse_wheel_pending = 0;
            ch585_half_report_arm_release_gate(
                &s_tx_frame,
                s_right_frame_valid ? &s_right_frame : 0);
        }
        return;
    }

#if CH585_SPI_ACCEPT_HOST_CMD
#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
    if(s_rx_cmd.cmd == AIK_SPI_CMD_POLL)
    {
        uint8_t output_mode = half_scan_host_output_mode(s_rx_cmd.cmd,
                                                         s_rx_cmd.flags);
        half_scan_set_output_mode(output_mode);
    }
    else if(s_rx_cmd.cmd == AIK_SPI_CMD_POLL_WITH_RF)
    {
        uint8_t output_mode = half_scan_host_output_mode(s_rx_cmd.cmd,
                                                         s_rx_cmd.flags);
        memcpy(s_rf_nkro16, s_rx_cmd.nkro16, sizeof(s_rf_nkro16));
        half_scan_output_nkro16(s_rf_nkro16,
                                aik_spi_host_cmd_consumer_usage(&s_rx_cmd),
                                aik_spi_host_cmd_consumer_delta(&s_rx_cmd),
                                half_scan_take_local_mouse_wheel(),
                                s_rx_cmd.host_seq,
                                output_mode);
    }
    else if(s_rx_cmd.cmd == AIK_SPI_CMD_PUSH_RIGHT_STATE)
    {
        aik_spi_half_state_v1_t next_right;
        uint8_t right_state_valid;
        uint8_t output_mode = half_scan_host_output_mode(s_rx_cmd.cmd,
                                                         s_rx_cmd.flags);

        memcpy(&next_right, s_rx_cmd.nkro16, sizeof(next_right));
        if(aik_spi_half_state_valid(&next_right) == 0U)
        {
            ch585_half_report_set_approval_context(
                (uint8_t)(
                    (s_rx_cmd.flags &
                     AIK_SPI_FLAG_APPROVAL_ACTIVE) != 0U),
                (uint8_t)(
                    (s_rx_cmd.flags &
                     AIK_SPI_FLAG_APPROVAL_SELECTED_YES) != 0U),
                0U);
            s_right_frame_valid = 0U;
            ch585_half_report_build_nkro16(
                &s_tx_frame, 0, s_rf_nkro16);
            half_scan_output_nkro16(
                s_rf_nkro16,
                AIK_CONSUMER_USAGE_NONE,
                0,
                0,
                s_rx_cmd.host_seq,
                output_mode);
            return;
        }

        right_state_valid = (uint8_t)(
            (s_rx_cmd.flags & AIK_SPI_FLAG_RIGHT_STATE_VALID) != 0U);
        ch585_half_report_set_approval_context(
            (uint8_t)(
                (s_rx_cmd.flags & AIK_SPI_FLAG_APPROVAL_ACTIVE) != 0U),
            (uint8_t)(
                (s_rx_cmd.flags &
                 AIK_SPI_FLAG_APPROVAL_SELECTED_YES) != 0U),
            right_state_valid);
        s_right_frame = next_right;
        s_right_frame_valid = right_state_valid;
        ch585_half_report_build_nkro16(&s_tx_frame,
                                       s_right_frame_valid ? &s_right_frame : 0,
                                       s_rf_nkro16);
        half_scan_output_nkro16(s_rf_nkro16,
                                aik_spi_host_cmd_consumer_usage(&s_rx_cmd),
                                aik_spi_host_cmd_consumer_delta(&s_rx_cmd),
                                half_scan_take_local_mouse_wheel(),
                                s_rx_cmd.host_seq,
                                output_mode);
    }
#endif
#else
    (void)s_rx_cmd;
#endif
}

#if CH585_HALF_SCAN_DEBUG_UART
static void half_scan_debug_uart_init(void)
{
    GPIOPinRemap(DISABLE, RB_PIN_UART1);
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    UART1_BaudRateCfg(CH585_HALF_SCAN_DEBUG_UART_BAUD);
}

static uint8_t half_scan_debug_first_down(uint8_t key_count)
{
    uint8_t key;

    for(key = 0U; key < key_count; key++)
    {
        if((s_tx_frame.down_bits[key >> 3] & (uint8_t)(1U << (key & 7U))) != 0U)
        {
            return key;
        }
    }
    return 0xFFU;
}

static void half_scan_debug_poll(uint8_t key_count)
{
    ch585_spi0_slave_link_stats_t spi_stats;
    uint16_t host_crc;
    uint16_t host_calc_crc;
    int8_t mouse_wheel_now;
#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
    int8_t last_output_mouse_wheel_debug = s_last_output_mouse_wheel;
#else
    int8_t last_output_mouse_wheel_debug = 0;
#endif
    uint8_t key;
    uint8_t rotary_ab;
    uint8_t rotary_raw_ab;
    uint8_t min_raw_key = 0U;
    uint8_t max_pos_key = 0U;
    uint16_t min_raw = 0xFFFFU;
    uint16_t max_pos = 0U;
#if CH585_SPI_ACCEPT_HOST_CMD
    uint8_t host_ok = aik_spi_host_cmd_valid(&s_rx_cmd);
#else
    uint8_t host_ok = 0U;
#endif

    if((s_tx_frame.half_seq % CH585_HALF_SCAN_DEBUG_PERIOD_FRAMES) != 0U)
    {
        return;
    }

    for(key = 0U; key < key_count; key++)
    {
        const mag_key_state_t *state = mag_key_engine_state(&s_engine, key);

        if(state == 0)
        {
            continue;
        }

        if(state->raw_adc < min_raw)
        {
            min_raw = state->raw_adc;
            min_raw_key = key;
        }
        if(state->position_pm > max_pos)
        {
            max_pos = state->position_pm;
            max_pos_key = key;
        }
    }

    ch585_spi0_slave_link_get_stats(&spi_stats);
    host_crc = s_rx_cmd.crc16;
    host_calc_crc = aik_spi_host_cmd_crc(&s_rx_cmd);
    rotary_ab = half_scan_read_rotary_ab();
    rotary_raw_ab = half_scan_read_rotary_raw_ab();
    mouse_wheel_now =
        ch585_half_report_mouse_wheel(&s_tx_frame,
                                      s_right_frame_valid ? &s_right_frame : 0);
    PRINT("hs half=%u seq=%u scan=%lu raw_min=%u:%u pos_max=%u:%u down=%02x%02x%02x%02x%02x%02x first=%u out=%u spi=%lu abort=%lu last=%d host=%u cmd=%u hseq=%u rxcnt=%u rx=%02x%02x%02x%02x hcrc=%04x/%04x rawab=%u redge=%lu irq=%lu if=%04x rot=%u racc=%d cw=%u ccw=%u cwT=%lu ccwT=%lu wh=%d pend=%d lastwh=%d\r\n",
          (unsigned int)CH585_HALF_ID,
          (unsigned int)s_tx_frame.half_seq,
          (unsigned long)s_acq.frames,
          (unsigned int)min_raw_key,
          (unsigned int)min_raw,
          (unsigned int)max_pos_key,
          (unsigned int)max_pos,
          (unsigned int)s_tx_frame.down_bits[0],
          (unsigned int)s_tx_frame.down_bits[1],
          (unsigned int)s_tx_frame.down_bits[2],
          (unsigned int)s_tx_frame.down_bits[3],
          (unsigned int)s_tx_frame.down_bits[4],
          (unsigned int)s_tx_frame.down_bits[5],
          (unsigned int)half_scan_debug_first_down(key_count),
#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
          (unsigned int)s_output_mode,
#else
          (unsigned int)AIK_OUTPUT_MODE_USBHS,
#endif
          (unsigned long)spi_stats.frames,
          (unsigned long)spi_stats.aborts,
          s_last_spi_result,
          (unsigned int)host_ok,
          (unsigned int)s_rx_cmd.cmd,
          (unsigned int)s_rx_cmd.host_seq,
          (unsigned int)spi_stats.last_rx_count,
          (unsigned int)spi_stats.last_rx_head[0],
          (unsigned int)spi_stats.last_rx_head[1],
          (unsigned int)spi_stats.last_rx_head[2],
          (unsigned int)spi_stats.last_rx_head[3],
          (unsigned int)host_crc,
          (unsigned int)host_calc_crc,
          (unsigned int)rotary_raw_ab,
          (unsigned long)s_local_rotary_debug_edge_count,
          (unsigned long)s_local_rotary_debug_irq_count,
          (unsigned int)s_local_rotary_debug_irq_flags,
          (unsigned int)rotary_ab,
          (int)s_local_rotary_accum,
          (unsigned int)s_local_rotary_cw_frames,
          (unsigned int)s_local_rotary_ccw_frames,
          (unsigned long)s_local_rotary_cw_total,
          (unsigned long)s_local_rotary_ccw_total,
          (int)mouse_wheel_now,
          (int)s_local_mouse_wheel_pending,
          (int)last_output_mouse_wheel_debug);
#if CH585_BLE_HID_ENABLE
    PRINT("ble conn=%u sent=%lu drop=%lu boot=%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
          (unsigned int)BLE_HID_IsConnected(),
          (unsigned long)s_ble_sent_count,
          (unsigned long)s_ble_drop_count,
          (unsigned int)s_ble_boot8[0],
          (unsigned int)s_ble_boot8[1],
          (unsigned int)s_ble_boot8[2],
          (unsigned int)s_ble_boot8[3],
          (unsigned int)s_ble_boot8[4],
          (unsigned int)s_ble_boot8[5],
          (unsigned int)s_ble_boot8[6],
          (unsigned int)s_ble_boot8[7]);
#endif
}
#else
static void half_scan_debug_uart_init(void)
{
}

static void half_scan_debug_poll(uint8_t key_count)
{
    (void)key_count;
}
#endif

static void half_scan_init(void)
{
    const ch585_ads7948_mux_profile_t *profile = ch585_ads7948_mux_profile(half_scan_side());
    mag_key_config_t cfg;

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);
    half_scan_debug_uart_init();
    PRINT("half_scan start half=%u sys=%lu keys=%u rf=%u ble=%u uart_dbg=%u\r\n",
          (unsigned int)CH585_HALF_ID,
          (unsigned long)GetSysClock(),
          (unsigned int)aik_spi_half_key_count((uint8_t)CH585_HALF_ID),
          (unsigned int)CH585_RF_TX_ENABLE,
          (unsigned int)CH585_BLE_HID_ENABLE,
          (unsigned int)CH585_HALF_SCAN_DEBUG_UART);
#if CH585_BLE_PAIRING_EXT_EEPROM
    /* SNV callbacks hit the external EEPROM as soon as the BLE stack
     * starts; bring the I2C bus up first. */
    ch585_eeprom_i2c_init();
#endif
#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
    CH58x_BLEInit();
#endif
    HAL_Init();

    memset(&s_acq, 0, sizeof(s_acq));
    memset(&s_engine, 0, sizeof(s_engine));
    memset(&s_tx_frame, 0, sizeof(s_tx_frame));
    memset(&s_rx_cmd, 0, sizeof(s_rx_cmd));
    memset(&s_profile_status, 0, sizeof(s_profile_status));
    memset(&s_right_frame, 0, sizeof(s_right_frame));
    memset(s_rf_nkro16, 0, sizeof(s_rf_nkro16));
    memset(s_compact_raw, 0, sizeof(s_compact_raw));
    s_last_spi_result = 0;
    s_profile_status_pending = 0U;
    s_right_frame_valid = 0U;
#if CH585_BLE_HID_ENABLE
    memset(s_ble_boot8, 0, sizeof(s_ble_boot8));
    s_ble_sent_count = 0U;
    s_ble_drop_count = 0U;
    s_ble_last_consumer_usage = AIK_CONSUMER_USAGE_NONE;
#endif
#if CH585_RF_TX_ENABLE || CH585_BLE_HID_ENABLE
    s_output_mode = 0xFFU;
    s_last_output_mouse_wheel = 0;
#endif

    ch585_ads7948_mux_gpio_init();
    half_scan_local_gpio_init();
    (void)ch585_ads7948_mux_acq_init(&s_acq, profile);

    mag_key_default_config(&cfg);
    cfg.mode = MAG_KEY_MODE_RAPID_TRIGGER;
    (void)mag_key_engine_init(&s_engine,
                              aik_spi_half_key_count((uint8_t)CH585_HALF_ID),
                              &cfg);

    ch585_half_report_reset_factory();
    ch585_profile_boot_load(&s_engine);

    ch585_spi0_slave_link_init();
#if CH585_RF_TX_ENABLE
    ch585_rf_nkro_tx_init();
#endif
#if CH585_BLE_HID_ENABLE
    BLE_HID_Init();
#endif
#if CH585_RF_TX_ENABLE && CH585_BLE_HID_ENABLE
    half_scan_set_output_mode(AIK_OUTPUT_MODE_USBHS);
#elif CH585_RF_TX_ENABLE
    half_scan_set_output_mode(AIK_OUTPUT_MODE_RF24);
#elif CH585_BLE_HID_ENABLE
    half_scan_set_output_mode(AIK_OUTPUT_MODE_BLE);
#endif

    ch585_ads7948_mux_acq_poll(&s_acq);
    half_scan_compact_raw(&s_acq,
                          s_compact_raw,
                          aik_spi_half_key_count((uint8_t)CH585_HALF_ID));
    (void)mag_key_engine_update(&s_engine, s_compact_raw);
    half_scan_build_frame();
}

#if CH585_HALF_SCAN_DEBUG_UART
__INTERRUPT
void GPIOA_IRQHandler(void)
{
    uint16_t flags = GPIOA_ReadITFlagPort();
    uint16_t rotary_flags = (uint16_t)(flags & (GPIO_Pin_10 | GPIO_Pin_11));

    if(rotary_flags != 0U)
    {
        s_local_rotary_debug_irq_count++;
        s_local_rotary_debug_irq_flags |= rotary_flags;
    }

    if(flags != 0U)
    {
        GPIOA_ClearITFlagBit(flags);
    }
}
#endif

int main(void)
{
    half_scan_init();

    while(1)
    {
        uint8_t key_count = aik_spi_half_key_count((uint8_t)CH585_HALF_ID);

        s_last_spi_result =
            ch585_spi0_slave_link_receive_frame((uint8_t *)&s_rx_cmd,
                                                AIK_SPI_HOST_CMD_SIZE);
        if(s_last_spi_result == CH585_SPI0_SLAVE_LINK_OK)
        {
            half_scan_apply_host_cmd();
        }
        if(s_profile_xfer_pending != 0U)
        {
            s_last_spi_result =
                ch585_spi0_slave_link_serve_tx_frame(
                    (const uint8_t *)&s_profile_xfer,
                    AIK_SPI_HALF_STATE_SIZE);
            s_profile_xfer_pending = 0U;
        }
        else if(s_profile_status_pending != 0U)
        {
            s_last_spi_result =
                ch585_spi0_slave_link_serve_tx_frame(
                    (const uint8_t *)&s_profile_status,
                    AIK_SPI_HALF_STATE_SIZE);
            s_profile_status_pending = 0U;
        }
        else
        {
            s_last_spi_result =
                ch585_spi0_slave_link_serve_tx_frame(
                    (const uint8_t *)&s_tx_frame,
                    AIK_SPI_HALF_STATE_SIZE);
        }
        ch585_ads7948_mux_acq_poll(&s_acq);
        half_scan_compact_raw(&s_acq, s_compact_raw, key_count);
        (void)mag_key_engine_update(&s_engine, s_compact_raw);
        half_scan_build_frame();
        half_scan_debug_poll(key_count);
#if CH585_RF_TX_ENABLE
        ch585_rf_nkro_tx_poll();
#elif CH585_BLE_HID_ENABLE
        TMOS_SystemProcess();
#endif
    }
}
