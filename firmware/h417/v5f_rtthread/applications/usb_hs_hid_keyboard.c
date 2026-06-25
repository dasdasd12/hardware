/*
 * USBHS HID keyboard + vendor HID bring-up for CH32H417.
 *
 * This is a minimal application-layer skeleton: USBFS CDC can stay as the
 * debug console while USBHS enumerates as a keyboard plus a vendor HID pipe.
 */

#include <rtthread.h>
#include <string.h>

#include "usbd_core.h"
#include "usbd_hid.h"
#include "usb_dc_ch32h417.h"
#include "usb_hs_hid_keyboard.h"

#define USBHS_HID_VID        0x1A86
#define USBHS_HID_PID        0xFE32
#define USBHS_HID_MAX_POWER  100

#define USBHS_KBD_IN_EP      0x81
#define USBHS_VENDOR_IN_EP   0x82
#define USBHS_VENDOR_OUT_EP  0x02

#define USBHS_KBD_REPORT_LEN     8U
#define USBHS_VENDOR_REPORT_LEN  64U

#define USBHS_KBD_INTERVAL       1U
#define USBHS_VENDOR_INTERVAL    1U

#define USBHS_KBD_REPORT_DESC_SIZE     63U
#define USBHS_VENDOR_REPORT_DESC_SIZE  38U

#define USBHS_HID_CONFIG_SIZE \
    (9U + HID_KEYBOARD_DESCRIPTOR_LEN + HID_CUSTOM_INOUT_DESCRIPTOR_LEN)

#ifndef APP_USBHS_HID_DOWN_ADC
#define APP_USBHS_HID_DOWN_ADC 2000U
#endif

static const uint8_t usbhs_hid_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00,
                               USBHS_HID_VID, USBHS_HID_PID, 0x0100, 0x01)
};

static const uint8_t usbhs_hid_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USBHS_HID_CONFIG_SIZE, 0x02, 0x01,
                               USB_CONFIG_BUS_POWERED, USBHS_HID_MAX_POWER),
    HID_KEYBOARD_DESCRIPTOR_INIT(0x00, 0x01, USBHS_KBD_REPORT_DESC_SIZE,
                                 USBHS_KBD_IN_EP, USBHS_KBD_REPORT_LEN,
                                 USBHS_KBD_INTERVAL),
    HID_CUSTOM_INOUT_DESCRIPTOR_INIT(0x01, 0x00, USBHS_VENDOR_REPORT_DESC_SIZE,
                                     USBHS_VENDOR_OUT_EP, USBHS_VENDOR_IN_EP,
                                     USBHS_VENDOR_REPORT_LEN,
                                     USBHS_VENDOR_INTERVAL),
};

static const uint8_t usbhs_hid_device_quality_descriptor[] = {
    0x0A,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *usbhs_hid_string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },
    "WCH",
    "AI Keyboard USBHS HID",
    "2026062201",
};

static const uint8_t *usbhs_hid_device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return usbhs_hid_device_descriptor;
}

static const uint8_t *usbhs_hid_config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return usbhs_hid_config_descriptor;
}

static const uint8_t *usbhs_hid_device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return usbhs_hid_device_quality_descriptor;
}

static const char *usbhs_hid_string_descriptor_callback(uint8_t speed,
                                                       uint8_t index)
{
    (void)speed;

    if (index >= (sizeof(usbhs_hid_string_descriptors) /
                  sizeof(usbhs_hid_string_descriptors[0])))
    {
        return RT_NULL;
    }

    return usbhs_hid_string_descriptors[index];
}

static const struct usb_descriptor usbhs_hid_descriptor = {
    .device_descriptor_callback = usbhs_hid_device_descriptor_callback,
    .config_descriptor_callback = usbhs_hid_config_descriptor_callback,
    .device_quality_descriptor_callback =
        usbhs_hid_device_quality_descriptor_callback,
    .string_descriptor_callback = usbhs_hid_string_descriptor_callback,
};

static const uint8_t usbhs_hid_keyboard_report_desc[USBHS_KBD_REPORT_DESC_SIZE] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xA1, 0x01, /* Collection (Application) */
    0x05, 0x07, /* Usage Page (Keyboard) */
    0x19, 0xE0, /* Usage Minimum (Keyboard LeftControl) */
    0x29, 0xE7, /* Usage Maximum (Keyboard Right GUI) */
    0x15, 0x00, /* Logical Minimum (0) */
    0x25, 0x01, /* Logical Maximum (1) */
    0x75, 0x01, /* Report Size (1) */
    0x95, 0x08, /* Report Count (8) */
    0x81, 0x02, /* Input (Data,Var,Abs) */
    0x95, 0x01, /* Report Count (1) */
    0x75, 0x08, /* Report Size (8) */
    0x81, 0x03, /* Input (Const,Var,Abs) */
    0x95, 0x05, /* Report Count (5) */
    0x75, 0x01, /* Report Size (1) */
    0x05, 0x08, /* Usage Page (LEDs) */
    0x19, 0x01, /* Usage Minimum (Num Lock) */
    0x29, 0x05, /* Usage Maximum (Kana) */
    0x91, 0x02, /* Output (Data,Var,Abs) */
    0x95, 0x01, /* Report Count (1) */
    0x75, 0x03, /* Report Size (3) */
    0x91, 0x03, /* Output (Const,Var,Abs) */
    0x95, 0x06, /* Report Count (6) */
    0x75, 0x08, /* Report Size (8) */
    0x15, 0x00, /* Logical Minimum (0) */
    0x25, 0xFF, /* Logical Maximum (255) */
    0x05, 0x07, /* Usage Page (Keyboard) */
    0x19, 0x00, /* Usage Minimum (Reserved) */
    0x29, 0x65, /* Usage Maximum (Keyboard Application) */
    0x81, 0x00, /* Input (Data,Ary,Abs) */
    0xC0        /* End Collection */
};

static const uint8_t usbhs_hid_vendor_report_desc[USBHS_VENDOR_REPORT_DESC_SIZE] = {
    0x06, 0x00, 0xFF, /* Usage Page (Vendor Defined) */
    0x09, 0x01,       /* Usage (Vendor Usage 1) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x02,       /* Report ID 2: device-to-host status */
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, 0x3F,
    0x81, 0x02,
    0x85, 0x01,       /* Report ID 1: host-to-device command */
    0x09, 0x03,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, 0x3F,
    0x91, 0x02,
    0xC0
};

static struct usbd_interface usbhs_kbd_intf;
static struct usbd_interface usbhs_vendor_intf;
static struct usbd_endpoint usbhs_kbd_in_ep;
static struct usbd_endpoint usbhs_vendor_in_ep;
static struct usbd_endpoint usbhs_vendor_out_ep;

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
uint8_t usbhs_kbd_report[USBHS_KBD_REPORT_LEN];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
uint8_t usbhs_vendor_rx[USBHS_VENDOR_REPORT_LEN];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
uint8_t usbhs_vendor_tx[USBHS_VENDOR_REPORT_LEN];

static uint8_t usbhs_hid_registered;
static volatile uint8_t usbhs_hid_configured_flag;
static volatile uint8_t usbhs_kbd_busy;
static volatile uint8_t usbhs_vendor_busy;
static uint8_t usbhs_kbd_idle;
static uint8_t usbhs_kbd_protocol = 1U;
static uint8_t usbhs_kbd_leds;
static uint8_t usbhs_last_vendor_cmd;
static uint32_t usbhs_kbd_reports;
static uint32_t usbhs_vendor_rx_reports;
static uint32_t usbhs_vendor_tx_reports;

static uint8_t key_id_to_hid_usage(uint16_t key_id)
{
    static const uint8_t digit_usage[10] = {
        0x27, 0x1E, 0x1F, 0x20, 0x21,
        0x22, 0x23, 0x24, 0x25, 0x26
    };

    if (key_id < 26U)
    {
        return (uint8_t)(0x04U + key_id);
    }

    if ((key_id >= 26U) && (key_id < 36U))
    {
        return digit_usage[key_id - 26U];
    }

    return 0U;
}

static void usbhs_vendor_build_status_report(void)
{
    memset(usbhs_vendor_tx, 0, sizeof(usbhs_vendor_tx));
    usbhs_vendor_tx[0] = 0x02U;
    usbhs_vendor_tx[1] = 'A';
    usbhs_vendor_tx[2] = 'I';
    usbhs_vendor_tx[3] = 'K';
    usbhs_vendor_tx[4] = 'H';
    usbhs_vendor_tx[5] = usbhs_last_vendor_cmd;
    usbhs_vendor_tx[6] = usbhs_kbd_leds;
    usbhs_vendor_tx[7] = usbhs_hid_configured_flag;
    usbhs_vendor_tx[8] = (uint8_t)usbhs_kbd_reports;
    usbhs_vendor_tx[9] = (uint8_t)(usbhs_kbd_reports >> 8);
    usbhs_vendor_tx[10] = (uint8_t)usbhs_vendor_rx_reports;
    usbhs_vendor_tx[11] = (uint8_t)(usbhs_vendor_rx_reports >> 8);
}

static void usbhs_vendor_submit_read(uint8_t busid)
{
    usbd_ep_start_read(busid, USBHS_VENDOR_OUT_EP,
                       usbhs_vendor_rx, sizeof(usbhs_vendor_rx));
}

static void usbhs_hid_event_handler(uint8_t busid, uint8_t event)
{
    if (busid != USB_CH32H417_BUS_HS)
    {
        return;
    }

    switch (event)
    {
    case USBD_EVENT_CONFIGURED:
        usbhs_hid_configured_flag = 1U;
        usbhs_kbd_busy = 0U;
        usbhs_vendor_busy = 0U;
        usbhs_vendor_submit_read(busid);
        rt_kprintf("USBHS HID configured\r\n");
        break;
    case USBD_EVENT_RESET:
    case USBD_EVENT_DISCONNECTED:
        usbhs_hid_configured_flag = 0U;
        usbhs_kbd_busy = 0U;
        usbhs_vendor_busy = 0U;
        break;
    default:
        break;
    }
}

static void usbhs_kbd_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
    usbhs_kbd_busy = 0U;
    usbhs_kbd_reports++;
}

static void usbhs_vendor_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
    usbhs_vendor_busy = 0U;
    usbhs_vendor_tx_reports++;
}

static void usbhs_vendor_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    if (nbytes > sizeof(usbhs_vendor_rx))
    {
        nbytes = sizeof(usbhs_vendor_rx);
    }

    usbhs_vendor_rx_reports++;
    usbhs_last_vendor_cmd = (nbytes > 1U) ? usbhs_vendor_rx[1] : 0U;
    usbhs_vendor_submit_read(busid);

    if ((usbhs_hid_configured_flag != 0U) && (usbhs_vendor_busy == 0U))
    {
        usbhs_vendor_build_status_report();
        usbhs_vendor_busy = 1U;
        if (usbd_ep_start_write(busid, USBHS_VENDOR_IN_EP,
                                usbhs_vendor_tx,
                                sizeof(usbhs_vendor_tx)) != 0)
        {
            usbhs_vendor_busy = 0U;
        }
    }
}

static void usbhs_hid_register_bus(void)
{
    if (usbhs_hid_registered != 0U)
    {
        return;
    }

    usbhs_kbd_in_ep.ep_addr = USBHS_KBD_IN_EP;
    usbhs_kbd_in_ep.ep_cb = usbhs_kbd_in_callback;
    usbhs_vendor_in_ep.ep_addr = USBHS_VENDOR_IN_EP;
    usbhs_vendor_in_ep.ep_cb = usbhs_vendor_in_callback;
    usbhs_vendor_out_ep.ep_addr = USBHS_VENDOR_OUT_EP;
    usbhs_vendor_out_ep.ep_cb = usbhs_vendor_out_callback;

    usbd_desc_register(USB_CH32H417_BUS_HS, &usbhs_hid_descriptor);
    usbd_add_interface(USB_CH32H417_BUS_HS,
                       usbd_hid_init_intf(USB_CH32H417_BUS_HS,
                                          &usbhs_kbd_intf,
                                          usbhs_hid_keyboard_report_desc,
                                          sizeof(usbhs_hid_keyboard_report_desc)));
    usbd_add_interface(USB_CH32H417_BUS_HS,
                       usbd_hid_init_intf(USB_CH32H417_BUS_HS,
                                          &usbhs_vendor_intf,
                                          usbhs_hid_vendor_report_desc,
                                          sizeof(usbhs_hid_vendor_report_desc)));
    usbd_add_endpoint(USB_CH32H417_BUS_HS, &usbhs_kbd_in_ep);
    usbd_add_endpoint(USB_CH32H417_BUS_HS, &usbhs_vendor_in_ep);
    usbd_add_endpoint(USB_CH32H417_BUS_HS, &usbhs_vendor_out_ep);
    usbhs_hid_registered = 1U;
}

int ch32h417_usbhs_hid_init(void)
{
    int ret;

    if (usbhs_hid_registered == 0U)
    {
        usbhs_hid_register_bus();
    }

    ret = usbd_initialize(USB_CH32H417_BUS_HS,
                          USBHS_BASE,
                          usbhs_hid_event_handler);
    rt_kprintf("USBHS HID init %s\r\n", (ret == 0) ? "ok" : "failed");
    return ret;
}

void ch32h417_usbhs_hid_poll_keyboard(const rt_uint16_t *raw_adc,
                                      rt_uint16_t key_count)
{
    uint8_t report[USBHS_KBD_REPORT_LEN] = {0};
    uint8_t slot = 2U;
    rt_uint16_t i;

    if ((usbhs_hid_configured_flag == 0U) || (usbhs_kbd_busy != 0U))
    {
        return;
    }

    if (raw_adc == RT_NULL)
    {
        return;
    }

    for (i = 0U; (i < key_count) && (slot < USBHS_KBD_REPORT_LEN); i++)
    {
        uint8_t usage;

        if (raw_adc[i] < APP_USBHS_HID_DOWN_ADC)
        {
            continue;
        }

        usage = key_id_to_hid_usage(i);
        if (usage == 0U)
        {
            continue;
        }

        report[slot] = usage;
        slot++;
    }

    memcpy(usbhs_kbd_report, report, sizeof(usbhs_kbd_report));
    usbhs_kbd_busy = 1U;
    if (usbd_ep_start_write(USB_CH32H417_BUS_HS,
                            USBHS_KBD_IN_EP,
                            usbhs_kbd_report,
                            sizeof(usbhs_kbd_report)) != 0)
    {
        usbhs_kbd_busy = 0U;
    }
}

rt_uint8_t ch32h417_usbhs_hid_configured(void)
{
    return usbhs_hid_configured_flag;
}

rt_uint8_t ch32h417_usbhs_hid_keyboard_busy(void)
{
    return usbhs_kbd_busy;
}

rt_uint8_t ch32h417_usbhs_hid_vendor_busy(void)
{
    return usbhs_vendor_busy;
}

rt_uint32_t ch32h417_usbhs_hid_keyboard_reports(void)
{
    return usbhs_kbd_reports;
}

rt_uint32_t ch32h417_usbhs_hid_vendor_rx_reports(void)
{
    return usbhs_vendor_rx_reports;
}

rt_uint32_t ch32h417_usbhs_hid_vendor_tx_reports(void)
{
    return usbhs_vendor_tx_reports;
}

rt_uint8_t ch32h417_usbhs_hid_last_vendor_cmd(void)
{
    return usbhs_last_vendor_cmd;
}

void usbd_hid_get_report(uint8_t busid,
                         uint8_t intf,
                         uint8_t report_id,
                         uint8_t report_type,
                         uint8_t **data,
                         uint32_t *len)
{
    (void)report_type;

    if (busid != USB_CH32H417_BUS_HS)
    {
        return;
    }

    if (intf == 0U)
    {
        *data = usbhs_kbd_report;
        *len = sizeof(usbhs_kbd_report);
        return;
    }

    if (intf == 1U)
    {
        usbhs_vendor_build_status_report();
        usbhs_vendor_tx[0] = (report_id == 0U) ? 0x02U : report_id;
        *data = usbhs_vendor_tx;
        *len = sizeof(usbhs_vendor_tx);
    }
}

uint8_t usbd_hid_get_idle(uint8_t busid, uint8_t intf, uint8_t report_id)
{
    (void)report_id;

    if ((busid == USB_CH32H417_BUS_HS) && (intf == 0U))
    {
        return usbhs_kbd_idle;
    }

    return 0U;
}

uint8_t usbd_hid_get_protocol(uint8_t busid, uint8_t intf)
{
    if ((busid == USB_CH32H417_BUS_HS) && (intf == 0U))
    {
        return usbhs_kbd_protocol;
    }

    return 0U;
}

void usbd_hid_set_report(uint8_t busid,
                         uint8_t intf,
                         uint8_t report_id,
                         uint8_t report_type,
                         uint8_t *report,
                         uint32_t report_len)
{
    (void)report_id;
    (void)report_type;

    if ((busid != USB_CH32H417_BUS_HS) || (report == RT_NULL) ||
        (report_len == 0U))
    {
        return;
    }

    if (intf == 0U)
    {
        usbhs_kbd_leds = report[0];
    }
    else if (intf == 1U)
    {
        usbhs_vendor_rx_reports++;
        usbhs_last_vendor_cmd = (report_len > 1U) ? report[1] : report[0];
    }
}

void usbd_hid_set_idle(uint8_t busid,
                       uint8_t intf,
                       uint8_t report_id,
                       uint8_t duration)
{
    (void)report_id;

    if ((busid == USB_CH32H417_BUS_HS) && (intf == 0U))
    {
        usbhs_kbd_idle = duration;
    }
}

void usbd_hid_set_protocol(uint8_t busid, uint8_t intf, uint8_t protocol)
{
    if ((busid == USB_CH32H417_BUS_HS) && (intf == 0U))
    {
        usbhs_kbd_protocol = protocol;
    }
}
