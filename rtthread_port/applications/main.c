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

#ifndef APP_ENABLE_USB_TEST
#define APP_ENABLE_USB_TEST 1
#endif

#ifndef APP_ENABLE_SERIAL_HEARTBEAT
#define APP_ENABLE_SERIAL_HEARTBEAT 1
#endif

#if APP_ENABLE_USB_TEST
extern int ch32h417_dual_cdc_init(void);
extern void ch32h417_dual_cdc_poll(void);
extern void usb_dc_ch32h417_dump_diag(void);
#endif

/* Eval board: PB1 = LED */
#define LED_PIN  rt_pin_get("PB.1")

int main(void)
{
    rt_base_t led_pin = LED_PIN;
    rt_uint32_t heartbeat = 0;

    rt_kprintf("Hello, RT-Thread on CH32H417 V5F!\n");
    rt_pin_mode(led_pin, PIN_MODE_OUTPUT);

#if APP_ENABLE_USB_TEST
#if defined(APP_USBSS_SKIP_FOR_V3F_OFFICIAL) && (APP_USBSS_SKIP_FOR_V3F_OFFICIAL != 0)
    rt_kprintf("Initializing USBFS CDC loopback; USBSS owned by V3F official stack...\n");
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
#if APP_ENABLE_USB_TEST
        ch32h417_dual_cdc_poll();
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
            }
        }
#endif
        heartbeat++;
        rt_thread_mdelay(500);
    }

    return 0;
}
