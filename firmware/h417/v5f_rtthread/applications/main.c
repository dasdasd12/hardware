/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-05-13     AI Assistant First version for CH32H417 V5F
 * 2025-01-15     AI Assistant Added USB3.0 HID+CDC composite device init
 * 2025-05-14     AI Assistant Added eval board HID test thread
 * 2026-06-09     AI Assistant Switched to USBSS + USBFS dual CDC bring-up
 */

#include <rtthread.h>
#include <rtdevice.h>
#include "board.h"
#include "ch585_ble_bridge.h"
#include "ch585_spi_scan.h"
#include "keyboard_engine.h"
#include "usb_hs_hid_keyboard.h"

#ifndef APP_ENABLE_USB_TEST
#define APP_ENABLE_USB_TEST 1
#endif

#ifndef APP_ENABLE_USB2_HS_CDC
#define APP_ENABLE_USB2_HS_CDC 0
#endif

#ifndef APP_ENABLE_USB2_FS_CDC
#define APP_ENABLE_USB2_FS_CDC 1
#endif

#ifndef APP_ENABLE_USB2_HS_HID
#define APP_ENABLE_USB2_HS_HID 0
#endif

#ifndef APP_ENABLE_CH585_SPI_SCAN
#define APP_ENABLE_CH585_SPI_SCAN 1
#endif

#ifndef APP_CH585_SPI_SCAN_POLLS_PER_LOOP
#define APP_CH585_SPI_SCAN_POLLS_PER_LOOP 1
#endif

#ifndef APP_ENABLE_USB_SCAN_REPORT
#define APP_ENABLE_USB_SCAN_REPORT 1
#endif

#ifndef APP_ENABLE_USB_SCAN_STATUS_REPORT
#define APP_ENABLE_USB_SCAN_STATUS_REPORT 1
#endif

#ifndef APP_ENABLE_USB_SPI_TRAIN_REPORT
#define APP_ENABLE_USB_SPI_TRAIN_REPORT 1
#endif

#ifndef APP_ENABLE_KEYBOARD_ENGINE
#define APP_ENABLE_KEYBOARD_ENGINE 0
#endif

#ifndef APP_ENABLE_CH585_BLE_BRIDGE
#define APP_ENABLE_CH585_BLE_BRIDGE 0
#endif

#ifndef APP_ENABLE_USB_KEY_ENGINE_REPORT
#define APP_ENABLE_USB_KEY_ENGINE_REPORT 0
#endif

#ifndef APP_USB_SCAN_REPORT_VALUES_PER_LINE
#define APP_USB_SCAN_REPORT_VALUES_PER_LINE 8
#endif

#ifndef APP_USB_SCAN_REPORT_PERIOD_LOOPS
#define APP_USB_SCAN_REPORT_PERIOD_LOOPS 2
#endif

#ifndef APP_USB_SCAN_STATUS_REPORT_PERIOD_LOOPS
#define APP_USB_SCAN_STATUS_REPORT_PERIOD_LOOPS 8
#endif

#ifndef APP_USB_SPI_TRAIN_REPORT_PERIOD_LOOPS
#define APP_USB_SPI_TRAIN_REPORT_PERIOD_LOOPS 4
#endif

#ifndef APP_CH585_DEBUG_REQUEST_PERIOD_LOOPS
#define APP_CH585_DEBUG_REQUEST_PERIOD_LOOPS 8
#endif

#ifndef APP_CH585_DEBUG_REQUEST_KEY
#define APP_CH585_DEBUG_REQUEST_KEY 0
#endif

#ifndef APP_CH585_CONFIG_TEST_ENABLE
#define APP_CH585_CONFIG_TEST_ENABLE 1
#endif

#ifndef APP_CH585_CONFIG_TEST_LOOP
#define APP_CH585_CONFIG_TEST_LOOP 6
#endif

#ifndef APP_CH585_CONFIG_TEST_KEY
#define APP_CH585_CONFIG_TEST_KEY 0
#endif

#ifndef APP_CH585_CONFIG_TEST_PARAM
#define APP_CH585_CONFIG_TEST_PARAM CH585_SCAN_CFG_RT_ENABLE
#endif

#ifndef APP_CH585_CONFIG_TEST_VALUE
#define APP_CH585_CONFIG_TEST_VALUE 0
#endif

#ifndef APP_CH585_CALIBRATE_TEST_ENABLE
#define APP_CH585_CALIBRATE_TEST_ENABLE 1
#endif

#ifndef APP_CH585_CALIBRATE_TEST_LOOP
#define APP_CH585_CALIBRATE_TEST_LOOP 18
#endif

#ifndef APP_CH585_CALIBRATE_TEST_KEY
#define APP_CH585_CALIBRATE_TEST_KEY 0
#endif

#ifndef APP_USB_KEY_ENGINE_REPORT_PERIOD_LOOPS
#define APP_USB_KEY_ENGINE_REPORT_PERIOD_LOOPS 1
#endif

#ifndef APP_USB_KEY_ENGINE_TRACKED_KEYS
#define APP_USB_KEY_ENGINE_TRACKED_KEYS 4
#endif

#ifndef APP_USB_CDC_WRITE_WAIT_RETRIES
#define APP_USB_CDC_WRITE_WAIT_RETRIES 20
#endif

#ifndef APP_USB_CDC_WRITE_RETRY_DELAY_MS
#define APP_USB_CDC_WRITE_RETRY_DELAY_MS 1
#endif

#ifndef APP_ENABLE_SERIAL_HEARTBEAT
#define APP_ENABLE_SERIAL_HEARTBEAT 1
#endif

#ifndef APP_MAIN_LOOP_DELAY_MS
#if APP_ENABLE_CH585_BLE_BRIDGE
#define APP_MAIN_LOOP_DELAY_MS 5
#else
#define APP_MAIN_LOOP_DELAY_MS 500
#endif
#endif

#if APP_ENABLE_USB_TEST
extern int ch32h417_dual_cdc_init(void);
extern void ch32h417_dual_cdc_poll(void);
extern void usb_dc_ch32h417_dump_diag(void);
extern int ch32h417_usb_cdc_write(const void *data, rt_uint32_t len);
#endif

/* Eval board: PB1 = LED */
#define LED_PIN  rt_pin_get("PB.1")

#if APP_ENABLE_USB_TEST
static int usb_cdc_write_full(const char *data, rt_size_t len)
{
    rt_size_t offset = 0U;
    rt_uint32_t retries = 0U;

    if (data == RT_NULL)
    {
        return -1;
    }

    while (offset < len)
    {
        int wrote = ch32h417_usb_cdc_write(&data[offset], (rt_uint32_t)(len - offset));

        if (wrote > 0)
        {
            offset += (rt_size_t)wrote;
            retries = 0U;
            continue;
        }

        if ((wrote == -2) || (retries >= APP_USB_CDC_WRITE_WAIT_RETRIES))
        {
            return (offset > 0U) ? (int)offset : wrote;
        }

        retries++;
        rt_thread_mdelay(APP_USB_CDC_WRITE_RETRY_DELAY_MS);
    }

    return (int)offset;
}

static int usb_cdc_write_line(const char *line, int len)
{
    if (len <= 0)
    {
        return len;
    }

    return usb_cdc_write_full(line, (rt_size_t)len);
}
#endif

#if APP_ENABLE_CH585_SPI_SCAN
static void ch585_config_test_poll(rt_uint32_t heartbeat)
{
#if APP_CH585_CONFIG_TEST_ENABLE
    static rt_uint8_t queued;

    if (queued != 0U)
    {
        return;
    }

    if (heartbeat < APP_CH585_CONFIG_TEST_LOOP)
    {
        return;
    }

    if (ch585_spi_scan_source0_queue_set_config(APP_CH585_CONFIG_TEST_KEY,
                                                APP_CH585_CONFIG_TEST_PARAM,
                                                APP_CH585_CONFIG_TEST_VALUE) == 0)
    {
        queued = 1U;
    }
#else
    (void)heartbeat;
#endif
}

static void ch585_debug_request_poll(rt_uint32_t heartbeat)
{
    if (APP_CH585_DEBUG_REQUEST_PERIOD_LOOPS == 0U)
    {
        return;
    }

    if ((heartbeat % APP_CH585_DEBUG_REQUEST_PERIOD_LOOPS) != 2U)
    {
        return;
    }

    (void)ch585_spi_scan_source0_queue_get_debug(APP_CH585_DEBUG_REQUEST_KEY);
}

static void ch585_calibrate_test_poll(rt_uint32_t heartbeat)
{
#if APP_CH585_CALIBRATE_TEST_ENABLE
    static rt_uint8_t queued;

    if (queued != 0U)
    {
        return;
    }

    if (heartbeat < APP_CH585_CALIBRATE_TEST_LOOP)
    {
        return;
    }

    if (ch585_spi_scan_source0_queue_calibrate_key(APP_CH585_CALIBRATE_TEST_KEY, 0U) == 0)
    {
        queued = 1U;
    }
#else
    (void)heartbeat;
#endif
}
#endif

#if APP_ENABLE_USB_TEST && APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SCAN_REPORT
static void usb_scan_report_poll(rt_uint32_t heartbeat)
{
    static rt_uint32_t report_frame;
    static rt_uint16_t report_index;
    const uint16_t *raw = ch585_spi_scan_raw();
    char line[96];
    int used;
    rt_uint16_t i;
    rt_uint16_t sent_values = 0;

    if ((heartbeat % APP_USB_SCAN_REPORT_PERIOD_LOOPS) != 0U) {
        return;
    }

    used = rt_snprintf(line, sizeof(line), "KS f=%u i=%03u", report_frame, report_index);
    if ((used < 0) || ((rt_size_t)used >= sizeof(line))) {
        return;
    }

    for (i = 0; (i < APP_USB_SCAN_REPORT_VALUES_PER_LINE) &&
         ((report_index + i) < CH585_SCAN_TOTAL_KEYS); i++)
    {
        int wrote;

        wrote = rt_snprintf(&line[used], sizeof(line) - (rt_size_t)used,
                            " %u", raw[report_index + i]);
        if ((wrote < 0) || ((rt_size_t)wrote >= (sizeof(line) - (rt_size_t)used))) {
            break;
        }
        used += wrote;
        sent_values++;
    }

    if ((sent_values == 0U) || ((rt_size_t)used > (sizeof(line) - 3U))) {
        return;
    }

    line[used++] = '\r';
    line[used++] = '\n';
    line[used] = '\0';

    if (usb_cdc_write_line(line, used) == used) {
        report_index += sent_values;
        if (report_index >= CH585_SCAN_TOTAL_KEYS) {
            report_index = 0;
            report_frame++;
        }
    }
}
#endif

#if APP_ENABLE_USB_TEST && APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SCAN_STATUS_REPORT
static void usb_scan_status_report_poll(rt_uint32_t heartbeat)
{
    const ch585_scan_source_stats_t *src0;
    const ch585_scan_source_stats_t *src1;
    const uint16_t *raw;
    uint32_t sck_x10;
    char line[96];
    int used;

    if ((heartbeat % APP_USB_SCAN_STATUS_REPORT_PERIOD_LOOPS) != 1U) {
        return;
    }

    src0 = ch585_spi_scan_source_stats(0U);
    src1 = ch585_spi_scan_source_stats(1U);
    raw = ch585_spi_scan_raw();
    sck_x10 = ch585_spi_scan_source0_sck_khz_x10();
    if ((src0 == RT_NULL) || (src1 == RT_NULL) || (raw == RT_NULL)) {
        return;
    }

    used = rt_snprintf(line, sizeof(line),
                       "SS hb=%u ok=%u fetch=%u crc=%u seq=%u s1ok=%u\r\n",
                       (unsigned int)heartbeat,
                       (unsigned int)src0->frames_ok,
                       (unsigned int)src0->fetch_errors,
                       (unsigned int)src0->crc_errors,
                       (unsigned int)src0->seq_drops,
                       (unsigned int)src1->frames_ok);
    if ((used > 0) && ((rt_size_t)used < sizeof(line))) {
        (void)usb_cdc_write_line(line, used);
    }

    used = rt_snprintf(line, sizeof(line),
                       "SP sck=%u.%u p=%04x h=%u c=%u tr=%u/%u/%u\r\n",
                       (unsigned int)(sck_x10 / 10U),
                       (unsigned int)(sck_x10 % 10U),
                       (unsigned int)ch585_spi_scan_source0_prescaler(),
                       (unsigned int)ch585_spi_scan_source0_hsrx(),
                       (unsigned int)ch585_spi_scan_source0_cpha_edges(),
                       (unsigned int)ch585_spi_scan_source0_train_done(),
                       (unsigned int)ch585_spi_scan_source0_train_errors(),
                       (unsigned int)ch585_spi_scan_source0_train_frames());
    if ((used > 0) && ((rt_size_t)used < sizeof(line))) {
        (void)usb_cdc_write_line(line, used);
    }

    {
        ch585_scan_debug_status_t debug;
        uint32_t debug_frames = 0U;

        if (ch585_spi_scan_source0_debug_status(&debug) == 0)
        {
            debug_frames = debug.frames;
        }

        used = rt_snprintf(line, sizeof(line),
                           "SC q=%u sent=%u dcmd=%u cal=%u ackerr=%u host=%u cmd=%02x type=%02x seq=%u rsn=%u dbg=%u\r\n",
                           (unsigned int)ch585_spi_scan_source0_cmd_queued(),
                           (unsigned int)ch585_spi_scan_source0_cmd_sent(),
                           (unsigned int)ch585_spi_scan_source0_debug_cmd_sent(),
                           (unsigned int)ch585_spi_scan_source0_calibrate_cmd_sent(),
                           (unsigned int)ch585_spi_scan_source0_ack_errors(),
                           (unsigned int)ch585_spi_scan_source0_host_seq(),
                           (unsigned int)ch585_spi_scan_source0_last_cmd(),
                           (unsigned int)ch585_spi_scan_source0_last_frame_type(),
                           (unsigned int)ch585_spi_scan_source0_last_frame_seq(),
                           (unsigned int)ch585_spi_scan_source0_last_resync_reason(),
                           (unsigned int)debug_frames);
        if ((used > 0) && ((rt_size_t)used < sizeof(line))) {
            (void)usb_cdc_write_line(line, used);
        }

        used = rt_snprintf(line, sizeof(line),
                           "SD rxcmd=%02x rxseq=%u rxv=%u rxe=%u rxbad=%u rxrsn=%u badm=%02x badc=%02x badhs=%u badack=%u\r\n",
                           (unsigned int)ch585_spi_scan_source0_slave_diag_cmd(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_host_seq(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_valid(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_cmd_error(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_invalid_count(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_invalid_reason(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_raw_magic(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_raw_cmd(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_raw_host_seq(),
                           (unsigned int)ch585_spi_scan_source0_slave_diag_raw_ack_seq());
        if ((used > 0) && ((rt_size_t)used < sizeof(line))) {
            (void)usb_cdc_write_line(line, used);
        }
    }

    used = rt_snprintf(line, sizeof(line),
                       "SR r0=%u r1=%u r63=%u r64=%u\r\n",
                       (unsigned int)raw[0],
                       (unsigned int)raw[1],
                       (unsigned int)raw[63],
                       (unsigned int)raw[64]);
    if ((used > 0) && ((rt_size_t)used < sizeof(line))) {
        (void)usb_cdc_write_line(line, used);
    }
}
#endif

#if APP_ENABLE_USB_TEST && APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SCAN_STATUS_REPORT
static void usb_scan_debug_report_poll(rt_uint32_t heartbeat)
{
    ch585_scan_debug_status_t debug;
    char line[128];
    int used;

    if ((heartbeat % APP_USB_SCAN_STATUS_REPORT_PERIOD_LOOPS) != 3U)
    {
        return;
    }

    if (ch585_spi_scan_source0_debug_status(&debug) != 0)
    {
        return;
    }

    used = rt_snprintf(line, sizeof(line),
                       "KD n=%u seq=%u k=%u down=%u rt=%u\r\n",
                       (unsigned int)debug.frames,
                       (unsigned int)debug.seq,
                       (unsigned int)debug.key_id,
                       (unsigned int)debug.is_down,
                       (unsigned int)debug.rt_armed);
    if ((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        (void)usb_cdc_write_line(line, used);
    }

    used = rt_snprintf(line, sizeof(line),
                       "KA raw=%u filt=%u pos=%u peak=%u\r\n",
                       (unsigned int)debug.raw_adc,
                       (unsigned int)debug.filtered_adc,
                       (unsigned int)debug.position_pm,
                       (unsigned int)debug.peak_pm);
    if ((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        (void)usb_cdc_write_line(line, used);
    }
}
#endif

#if APP_ENABLE_USB_TEST && APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SPI_TRAIN_REPORT
static void usb_spi_train_report_poll(rt_uint32_t heartbeat)
{
    static rt_uint8_t report_index;
    rt_uint8_t count;
    rt_uint16_t prescaler;
    rt_uint16_t bad_errors;
    rt_uint16_t seq_errors;
    rt_uint8_t hsrx;
    rt_uint8_t cpha_edges;
    char line[96];
    int used;

    if ((heartbeat % APP_USB_SPI_TRAIN_REPORT_PERIOD_LOOPS) != 3U) {
        return;
    }

    if (ch585_spi_scan_source0_train_done() == 0U) {
        return;
    }

    count = ch585_spi_scan_source0_train_candidate_count();
    if (count == 0U) {
        return;
    }

    if (report_index >= count) {
        report_index = 0U;
    }

    if (ch585_spi_scan_source0_train_candidate(report_index,
                                               &prescaler,
                                               &hsrx,
                                               &cpha_edges,
                                               &bad_errors,
                                               &seq_errors) != 0) {
        report_index = 0U;
        return;
    }

    used = rt_snprintf(line, sizeof(line),
                       "TR i=%u/%u p=%04x h=%u c=%u bad=%u seq=%u\r\n",
                       (unsigned int)report_index,
                       (unsigned int)count,
                       (unsigned int)prescaler,
                       (unsigned int)hsrx,
                       (unsigned int)cpha_edges,
                       (unsigned int)bad_errors,
                       (unsigned int)seq_errors);
    if ((used > 0) && ((rt_size_t)used < sizeof(line))) {
        if (usb_cdc_write_line(line, used) == used) {
            report_index++;
        }
    }
}
#endif

#if APP_ENABLE_USB_TEST && APP_ENABLE_KEYBOARD_ENGINE && APP_ENABLE_USB_KEY_ENGINE_REPORT
static void usb_keyboard_engine_report_poll(rt_uint32_t heartbeat)
{
    keyboard_engine_event_t events[KEYBOARD_ENGINE_EVENT_CAPACITY];
    char line[104];
    rt_uint16_t count;
    rt_uint16_t i;

    count = keyboard_engine_drain_events(events, KEYBOARD_ENGINE_EVENT_CAPACITY);
    for (i = 0U; i < count; i++)
    {
        int used = rt_snprintf(line, sizeof(line),
                               "EV f=%u k=%u d=%u p=%u r=%u fl=%u\r\n",
                               (unsigned int)keyboard_engine_frame_count(),
                               (unsigned int)events[i].key_id,
                               (unsigned int)events[i].is_down,
                               (unsigned int)events[i].position_pm,
                               (unsigned int)events[i].raw_adc,
                               (unsigned int)events[i].filtered_adc);
        if ((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            (void)usb_cdc_write_line(line, used);
        }
    }

    if ((heartbeat % APP_USB_KEY_ENGINE_REPORT_PERIOD_LOOPS) != 0U)
    {
        return;
    }

    for (i = 0U; i < APP_USB_KEY_ENGINE_TRACKED_KEYS; i++)
    {
        const keyboard_engine_key_state_t *state = keyboard_engine_key_state(i);
        int used;

        if (state == RT_NULL)
        {
            continue;
        }

        used = rt_snprintf(line, sizeof(line),
                           "KE f=%u k=%u r=%u fl=%u p=%u d=%u\r\n",
                           (unsigned int)keyboard_engine_frame_count(),
                           (unsigned int)i,
                           (unsigned int)state->raw_adc,
                           (unsigned int)state->filtered_adc,
                           (unsigned int)state->position_pm,
                           (unsigned int)state->is_down);
        if ((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            (void)usb_cdc_write_line(line, used);
        }
    }
}
#endif

#if APP_ENABLE_USB_TEST && APP_ENABLE_USB2_HS_HID
static void usb_hs_hid_status_report_poll(rt_uint32_t heartbeat)
{
    char line[96];
    int used;

    if ((heartbeat % APP_USB_SCAN_STATUS_REPORT_PERIOD_LOOPS) != 5U)
    {
        return;
    }

    used = rt_snprintf(line, sizeof(line),
                       "UH cfg=%u kb=%u vb=%u kr=%u vrx=%u vtx=%u vcmd=%02x\r\n",
                       (unsigned int)ch32h417_usbhs_hid_configured(),
                       (unsigned int)ch32h417_usbhs_hid_keyboard_busy(),
                       (unsigned int)ch32h417_usbhs_hid_vendor_busy(),
                       (unsigned int)ch32h417_usbhs_hid_keyboard_reports(),
                       (unsigned int)ch32h417_usbhs_hid_vendor_rx_reports(),
                       (unsigned int)ch32h417_usbhs_hid_vendor_tx_reports(),
                       (unsigned int)ch32h417_usbhs_hid_last_vendor_cmd());
    if ((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        (void)usb_cdc_write_line(line, used);
    }
}
#endif

int main(void)
{
    rt_base_t led_pin = LED_PIN;
    rt_uint32_t heartbeat = 0;

    rt_kprintf("Hello, RT-Thread on CH32H417 V5F!\n");
    rt_pin_mode(led_pin, PIN_MODE_OUTPUT);

#if APP_ENABLE_CH585_SPI_SCAN
    ch585_spi_scan_init();
#endif
#if APP_ENABLE_CH585_BLE_BRIDGE
    ch585_ble_bridge_init();
#endif
#if APP_ENABLE_KEYBOARD_ENGINE
    keyboard_engine_init();
#endif

#if APP_ENABLE_USB_TEST
#if APP_ENABLE_USB2_HS_HID && APP_ENABLE_USB2_FS_CDC
    rt_kprintf("Initializing USBFS CDC debug and USBHS HID keyboard/vendor...\n");
#elif APP_ENABLE_USB2_FS_CDC && !APP_ENABLE_USB2_HS_CDC
    rt_kprintf("Initializing USB2.0 FS CDC loopback on USBFS/OTG port...\n");
#elif APP_ENABLE_USB2_HS_CDC && !APP_ENABLE_USB2_FS_CDC
    rt_kprintf("Initializing USB2.0 HS CDC loopback on USBHS port...\n");
#elif defined(APP_USBSS_SKIP_FOR_V3F_OFFICIAL) && (APP_USBSS_SKIP_FOR_V3F_OFFICIAL != 0)
    rt_kprintf("Initializing USB2.0 CDC loopback; USBSS owned by V3F official stack...\n");
#else
    rt_kprintf("Initializing USBSS + USBFS CDC loopback devices; USBHS fallback disabled for SS debug...\n");
#endif

    if (ch32h417_dual_cdc_init() != 0)
    {
        rt_kprintf("Dual CDC init failed on both buses; keep RT-Thread running.\n");
    }
    else
    {
        rt_kprintf("Dual CDC init completed.\n");
    }

#if APP_ENABLE_USB2_HS_HID
    if (ch32h417_usbhs_hid_init() != 0)
    {
        rt_kprintf("USBHS HID init failed; FS CDC debug may still work.\n");
    }
#endif
#else
    rt_kprintf("USB test disabled; RT-Thread heartbeat is running.\n");
#endif

    while (1)
    {
        rt_pin_write(led_pin, (heartbeat & 1U) ? PIN_HIGH : PIN_LOW);
#if APP_ENABLE_CH585_SPI_SCAN
        rt_uint32_t scan_poll;

        ch585_config_test_poll(heartbeat);
        ch585_calibrate_test_poll(heartbeat);
        ch585_debug_request_poll(heartbeat);
        for (scan_poll = 0; scan_poll < APP_CH585_SPI_SCAN_POLLS_PER_LOOP; scan_poll++)
        {
            ch585_spi_scan_poll_once();
        }
#if APP_ENABLE_KEYBOARD_ENGINE
        keyboard_engine_update(ch585_spi_scan_raw());
#endif
#if APP_ENABLE_CH585_BLE_BRIDGE
        ch585_ble_bridge_poll_from_raw(ch585_spi_scan_raw(), CH585_SCAN_TOTAL_KEYS);
#endif
#endif
#if APP_ENABLE_USB_TEST
        ch32h417_dual_cdc_poll();
#if APP_ENABLE_USB2_HS_HID && APP_ENABLE_CH585_SPI_SCAN
        ch32h417_usbhs_hid_poll_keyboard(ch585_spi_scan_raw(),
                                         CH585_SCAN_TOTAL_KEYS);
#endif
#if APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SCAN_STATUS_REPORT
        usb_scan_status_report_poll(heartbeat);
        usb_scan_debug_report_poll(heartbeat);
#endif
#if APP_ENABLE_USB2_HS_HID
        usb_hs_hid_status_report_poll(heartbeat);
#endif
#if APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SPI_TRAIN_REPORT
        usb_spi_train_report_poll(heartbeat);
#endif
#if APP_ENABLE_KEYBOARD_ENGINE && APP_ENABLE_USB_KEY_ENGINE_REPORT
        usb_keyboard_engine_report_poll(heartbeat);
#endif
#if APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SCAN_REPORT
        usb_scan_report_poll(heartbeat);
#endif
#endif
#if APP_ENABLE_SERIAL_HEARTBEAT
        if ((heartbeat % 4U) == 0U)
        {
            rt_kprintf("rtthread heartbeat %u\n", heartbeat);
            if ((heartbeat % 8U) == 0U)
            {
#if APP_ENABLE_USB_TEST
                usb_dc_ch32h417_dump_diag();
#endif
#if APP_ENABLE_CH585_SPI_SCAN
                ch585_spi_scan_dump_stats();
#endif
            }
        }
#endif
        heartbeat++;
        rt_thread_mdelay(APP_MAIN_LOOP_DELAY_MS);
    }

    return 0;
}
