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
#include "ch585_spi_scan.h"

#ifndef APP_ENABLE_V5F_HW_TEST
#define APP_ENABLE_V5F_HW_TEST 0
#endif

#if APP_ENABLE_V5F_HW_TEST
#include "v5f_hw_test.h"
#endif

#ifndef APP_ENABLE_USB_TEST
#define APP_ENABLE_USB_TEST 1
#endif

#ifndef APP_ENABLE_USB2_HS_CDC
#define APP_ENABLE_USB2_HS_CDC 0
#endif

#ifndef APP_ENABLE_USB2_FS_CDC
#define APP_ENABLE_USB2_FS_CDC 1
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


#ifndef APP_ENABLE_SERIAL_HEARTBEAT
#define APP_ENABLE_SERIAL_HEARTBEAT 1
#endif

#if APP_ENABLE_USB_TEST
extern int ch32h417_dual_cdc_init(void);
extern void ch32h417_dual_cdc_poll(void);
extern void usb_dc_ch32h417_dump_diag(void);
extern int ch32h417_usb_cdc_write(const void *data, rt_uint32_t len);
#endif

/* Eval board: PB1 = LED */
#define LED_PIN  rt_pin_get("PB.1")

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

    if (ch32h417_usb_cdc_write(line, (rt_uint32_t)used) > 0) {
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
    char line[224];
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
                       "SS hb=%u s0ok=%u s0fetch=%u s0crc=%u s0seq=%u sck=%u.%u p=%04x h=%u c=%u tr=%u/%u/%u s1ok=%u raw0=%u raw1=%u raw63=%u raw64=%u\r\n",
                       (unsigned int)heartbeat,
                       (unsigned int)src0->frames_ok,
                       (unsigned int)src0->fetch_errors,
                       (unsigned int)src0->crc_errors,
                       (unsigned int)src0->seq_drops,
                       (unsigned int)(sck_x10 / 10U),
                       (unsigned int)(sck_x10 % 10U),
                       (unsigned int)ch585_spi_scan_source0_prescaler(),
                       (unsigned int)ch585_spi_scan_source0_hsrx(),
                       (unsigned int)ch585_spi_scan_source0_cpha_edges(),
                       (unsigned int)ch585_spi_scan_source0_train_done(),
                       (unsigned int)ch585_spi_scan_source0_train_errors(),
                       (unsigned int)ch585_spi_scan_source0_train_frames(),
                       (unsigned int)src1->frames_ok,
                       (unsigned int)raw[0],
                       (unsigned int)raw[1],
                       (unsigned int)raw[63],
                       (unsigned int)raw[64]);
    if ((used > 0) && ((rt_size_t)used < sizeof(line))) {
        (void)ch32h417_usb_cdc_write(line, (rt_uint32_t)used);
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
        if (ch32h417_usb_cdc_write(line, (rt_uint32_t)used) > 0) {
            report_index++;
        }
    }
}
#endif

int main(void)
{
    rt_base_t led_pin = LED_PIN;
    rt_uint32_t heartbeat = 0;

    rt_kprintf("Hello, RT-Thread on CH32H417 V5F!\n");
    rt_pin_mode(led_pin, PIN_MODE_OUTPUT);

#if APP_ENABLE_V5F_HW_TEST
    v5f_hw_test_start();
#endif

#if APP_ENABLE_CH585_SPI_SCAN
    ch585_spi_scan_init();
#endif

#if APP_ENABLE_USB_TEST
#if APP_ENABLE_USB2_FS_CDC && !APP_ENABLE_USB2_HS_CDC
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
#else
    rt_kprintf("USB test disabled; RT-Thread heartbeat is running.\n");
#endif

    while (1)
    {
        rt_pin_write(led_pin, (heartbeat & 1U) ? PIN_HIGH : PIN_LOW);
#if APP_ENABLE_CH585_SPI_SCAN
        rt_uint32_t scan_poll;

        for (scan_poll = 0; scan_poll < APP_CH585_SPI_SCAN_POLLS_PER_LOOP; scan_poll++)
        {
            ch585_spi_scan_poll_once();
        }

#endif
#if APP_ENABLE_USB_TEST
        ch32h417_dual_cdc_poll();
#if APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SCAN_STATUS_REPORT
        usb_scan_status_report_poll(heartbeat);
#endif
#if APP_ENABLE_CH585_SPI_SCAN && APP_ENABLE_USB_SPI_TRAIN_REPORT
        usb_spi_train_report_poll(heartbeat);
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
        rt_thread_mdelay(500);
    }

    return 0;
}
