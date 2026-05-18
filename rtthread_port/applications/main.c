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
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "board.h"

#ifndef APP_ENABLE_USB_TEST
#define APP_ENABLE_USB_TEST 1
#endif

#ifndef APP_ENABLE_SERIAL_HEARTBEAT
#define APP_ENABLE_SERIAL_HEARTBEAT 1
#endif

#if APP_ENABLE_USB_TEST
extern int hid_cdc_composite_init(void);
extern int usbd_ep_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len);
#endif

/* Eval board: PB1 = LED, PB0 = KEY (active low with pull-up) */
#define LED_PIN  rt_pin_get("PB.1")
#define KEY_PIN  rt_pin_get("PB.0")

#if APP_ENABLE_USB_TEST
#define HID_INT_EP 0x84

static void usb_test_thread(void *parameter)
{
    uint8_t report[8];

    (void)parameter;

    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(KEY_PIN, PIN_MODE_INPUT_PULLUP);

    rt_kprintf("USB test thread started. Press PB0 to send HID 'A'.\n");

    while (1)
    {
        if (rt_pin_read(KEY_PIN) == PIN_LOW)
        {
            /* Send HID keyboard report: press 'a' (0x04) */
            memset(report, 0, sizeof(report));
            report[2] = 0x04;
            usbd_ep_start_write(0, HID_INT_EP, report, sizeof(report));
            rt_pin_write(LED_PIN, PIN_HIGH);

            rt_thread_mdelay(50);

            /* Release key */
            memset(report, 0, sizeof(report));
            usbd_ep_start_write(0, HID_INT_EP, report, sizeof(report));
            rt_pin_write(LED_PIN, PIN_LOW);

            rt_thread_mdelay(200); /* debounce */
        }

        rt_thread_mdelay(10);
    }
}
#endif

int main(void)
{
#if APP_ENABLE_USB_TEST
    rt_thread_t tid;
#endif
    rt_base_t led_pin = LED_PIN;
    rt_uint32_t heartbeat = 0;

    rt_kprintf("Hello, RT-Thread on CH32H417 V5F!\n");
    rt_pin_mode(led_pin, PIN_MODE_OUTPUT);

#if APP_ENABLE_USB_TEST
    rt_kprintf("Initializing USB3.0 HID+CDC Composite Device...\n");

    /* Initialize CherryUSB composite device */
    if (hid_cdc_composite_init() != 0)
    {
        rt_kprintf("USB device init failed; keep RT-Thread running.\n");
    }
    else
    {
        rt_kprintf("USB device initialized.\n");

        /* Create eval board test thread */
        tid = rt_thread_create("usb_test", usb_test_thread, RT_NULL, 1024, 10, 10);
        if (tid != RT_NULL)
        {
            rt_thread_startup(tid);
        }
        else
        {
            rt_kprintf("Failed to create usb_test thread!\n");
        }
    }
#else
    rt_kprintf("USB test disabled; RT-Thread heartbeat is running.\n");
#endif

    while (1)
    {
        rt_pin_write(led_pin, (heartbeat & 1U) ? PIN_HIGH : PIN_LOW);
#if APP_ENABLE_SERIAL_HEARTBEAT
        if ((heartbeat % 4U) == 0U)
        {
            rt_kprintf("rtthread heartbeat %u\n", heartbeat);
        }
#endif
        heartbeat++;
        rt_thread_mdelay(500);
    }

    return 0;
}
