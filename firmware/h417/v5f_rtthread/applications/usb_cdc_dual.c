/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <rtthread.h>
#include <string.h>

#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "usbd_hid.h"
#include "usb_dc_ch32h417.h"
#include "usb_dc_ch32h417_usbss.h"
#include "usb_hs_hid_keyboard.h"

#define USBD_VID           0x1A86
#define USBD_PID           0xFE31
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define CDC_FS_MPS 64U
#define CDC_HS_MPS 512U
#define CDC_SS_MPS 1024U

#define FS_KBD_IN_EP         0x84
#define FS_KBD_REPORT_LEN    8U
#define FS_KBD_INTERVAL      1U
#define FS_KBD_REPORT_DESC_SIZE 63U

#ifndef APP_ENABLE_USB2_HS_CDC
#define APP_ENABLE_USB2_HS_CDC 1
#endif

#ifndef APP_ENABLE_USB2_FS_CDC
#define APP_ENABLE_USB2_FS_CDC 0
#endif

#ifndef APP_ENABLE_USBSS_CDC
#define APP_ENABLE_USBSS_CDC 0
#endif

#ifndef APP_ENABLE_V5F_HW_TEST
#define APP_ENABLE_V5F_HW_TEST 0
#endif

#ifndef APP_USB_CDC_RX_ECHO
#if APP_ENABLE_V5F_HW_TEST
#define APP_USB_CDC_RX_ECHO 0
#else
#define APP_USB_CDC_RX_ECHO 1
#endif
#endif

#ifndef APP_ENABLE_USBFS_CDC_HID
#define APP_ENABLE_USBFS_CDC_HID 0
#endif

#if APP_ENABLE_USBFS_CDC_HID
#define USB_CONFIG_SIZE_FS (9 + CDC_ACM_DESCRIPTOR_LEN + HID_KEYBOARD_DESCRIPTOR_LEN)
#define USB_CONFIG_INTERFACE_COUNT_FS 0x03
#else
#define USB_CONFIG_SIZE_FS (9 + CDC_ACM_DESCRIPTOR_LEN)
#define USB_CONFIG_INTERFACE_COUNT_FS 0x02
#endif
#define USB_CONFIG_SIZE_HS (9 + CDC_ACM_DESCRIPTOR_LEN)
#define USB_CONFIG_SIZE_SS (9 + 8 + 9 + 5 + 5 + 4 + 5 + 7 + 6 + 9 + 7 + 6 + 7 + 6)

#define USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION 0x30U

#define CDC_RX_LINE_BYTES 64U
#define CDC_RX_LINE_COUNT 4U

static const uint8_t device_descriptor_fs[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t device_descriptor_ss[] = {
    0x12,                       /* bLength */
    USB_DESCRIPTOR_TYPE_DEVICE, /* bDescriptorType */
    WBVAL(USB_3_0),             /* bcdUSB */
    0xEF,                       /* bDeviceClass */
    0x02,                       /* bDeviceSubClass */
    0x01,                       /* bDeviceProtocol */
    0x09,                       /* bMaxPacketSize0: 512 bytes for USB 3.x */
    WBVAL(USBD_VID),            /* idVendor */
    WBVAL(USBD_PID),            /* idProduct */
    WBVAL(0x0100),              /* bcdDevice */
    USB_STRING_MFC_INDEX,       /* iManufacturer */
    USB_STRING_PRODUCT_INDEX,   /* iProduct */
    USB_STRING_SERIAL_INDEX,    /* iSerial */
    0x01                        /* bNumConfigurations */
};

static const uint8_t config_descriptor_fs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE_FS, USB_CONFIG_INTERFACE_COUNT_FS, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_FS_MPS, 0x02),
#if APP_ENABLE_USBFS_CDC_HID
    HID_KEYBOARD_DESCRIPTOR_INIT(0x02, 0x01, FS_KBD_REPORT_DESC_SIZE,
                                 FS_KBD_IN_EP, FS_KBD_REPORT_LEN,
                                 FS_KBD_INTERVAL),
#endif
};

static const uint8_t config_descriptor_hs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE_HS, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_HS_MPS, 0x02),
};

static const uint8_t config_descriptor_ss[] = {
    0x09,                              /* bLength */
    USB_DESCRIPTOR_TYPE_CONFIGURATION, /* bDescriptorType */
    WBVAL(USB_CONFIG_SIZE_SS),         /* wTotalLength */
    0x02,                              /* bNumInterfaces */
    0x01,                              /* bConfigurationValue */
    0x00,                              /* iConfiguration */
    USB_CONFIG_BUS_POWERED,            /* bmAttributes */
    USB_CONFIG_POWER_MA(USBD_MAX_POWER),

    0x08,                                      /* bLength */
    USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION, /* bDescriptorType */
    0x00,                                      /* bFirstInterface */
    0x02,                                      /* bInterfaceCount */
    USB_DEVICE_CLASS_CDC,                      /* bFunctionClass */
    CDC_ABSTRACT_CONTROL_MODEL,                /* bFunctionSubClass */
    CDC_COMMON_PROTOCOL_NONE,                  /* bFunctionProtocol */
    0x00,                                      /* iFunction */

    0x09,                         /* bLength */
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType */
    0x00,                         /* bInterfaceNumber */
    0x00,                         /* bAlternateSetting */
    0x01,                         /* bNumEndpoints */
    USB_DEVICE_CLASS_CDC,         /* bInterfaceClass */
    CDC_ABSTRACT_CONTROL_MODEL,   /* bInterfaceSubClass */
    CDC_COMMON_PROTOCOL_NONE,     /* bInterfaceProtocol */
    0x02,                         /* iInterface */

    0x05,             /* bLength */
    CDC_CS_INTERFACE, /* bDescriptorType */
    CDC_FUNC_DESC_HEADER,
    WBVAL(CDC_V1_10),

    0x05,             /* bLength */
    CDC_CS_INTERFACE, /* bDescriptorType */
    CDC_FUNC_DESC_CALL_MANAGEMENT,
    0x00, /* bmCapabilities */
    0x01, /* bDataInterface */

    0x04,             /* bLength */
    CDC_CS_INTERFACE, /* bDescriptorType */
    CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT,
    0x02, /* bmCapabilities */

    0x05,             /* bLength */
    CDC_CS_INTERFACE, /* bDescriptorType */
    CDC_FUNC_DESC_UNION,
    0x00, /* bMasterInterface */
    0x01, /* bSlaveInterface0 */

    0x07,                         /* bLength */
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */
    CDC_INT_EP,                   /* bEndpointAddress */
    USB_ENDPOINT_TYPE_INTERRUPT,  /* bmAttributes */
    WBVAL(CDC_SS_MPS),            /* wMaxPacketSize */
    0x0A,                         /* bInterval */

    0x06,                                    /* bLength */
    USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION,
    0x00, /* bMaxBurst */
    0x00, /* bmAttributes */
    0x00, 0x00, /* wBytesPerInterval */

    0x09,                         /* bLength */
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType */
    0x01,                         /* bInterfaceNumber */
    0x00,                         /* bAlternateSetting */
    0x02,                         /* bNumEndpoints */
    CDC_DATA_INTERFACE_CLASS,     /* bInterfaceClass */
    0x00,                         /* bInterfaceSubClass */
    0x00,                         /* bInterfaceProtocol */
    0x00,                         /* iInterface */

    0x07,                        /* bLength */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType */
    CDC_OUT_EP,                  /* bEndpointAddress */
    USB_ENDPOINT_TYPE_BULK,      /* bmAttributes */
    WBVAL(CDC_SS_MPS),           /* wMaxPacketSize */
    0x00,                        /* bInterval */

    0x06,                                    /* bLength */
    USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION,
    0x00, /* bMaxBurst */
    0x00, /* bmAttributes */
    0x00, 0x00, /* wBytesPerInterval */

    0x07,                        /* bLength */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType */
    CDC_IN_EP,                   /* bEndpointAddress */
    USB_ENDPOINT_TYPE_BULK,      /* bmAttributes */
    WBVAL(CDC_SS_MPS),           /* wMaxPacketSize */
    0x00,                        /* bInterval */

    0x06,                                    /* bLength */
    USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION,
    0x00, /* bMaxBurst */
    0x00, /* bmAttributes */
    0x00, 0x00, /* wBytesPerInterval */
};

static const uint8_t bos_descriptor[] = {
    USB_BOS_HEADER_DESCRIPTOR_INIT(0x16, 0x02),
    0x07,
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY,
    USB_DEVICE_CAPABILITY_USB_2_0_EXTENSION,
    0x02, 0x00, 0x00, 0x00,
    0x0A,
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY,
    USB_DEVICE_CAPABILITY_SUPERSPEED_USB,
    0x00,
    0x0E, 0x00, /* SS | HS | FS */
    0x01,       /* lowest fully functional speed: FS */
    0x0A, 0x00, /* bU1DevExitLat */
    0x00, 0x00, /* wU2DevExitLat */
};

static const struct usb_bos_descriptor bos_desc = {
    .string = bos_descriptor,
    .string_len = sizeof(bos_descriptor),
};

static const char *string_descriptors_fs[] = {
    (const char[]){ 0x09, 0x04 },
    "WCH",
    "CH32H417 USBFS CDC",
    "2026060902",
};

static const char *string_descriptors_ss[] = {
    (const char[]){ 0x09, 0x04 },
    "WCH",
    "CH32H417 USBSS CDC",
    "2026060901",
};

static const char *string_descriptors_hs[] = {
    (const char[]){ 0x09, 0x04 },
    "WCH",
    "CH32H417 USBHS CDC",
    "2026060903",
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return (speed == USB_SPEED_SUPER) ? device_descriptor_ss : device_descriptor_fs;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_SUPER) {
        return config_descriptor_ss;
    }
    if (speed == USB_SPEED_HIGH) {
        return config_descriptor_hs;
    }
    return config_descriptor_fs;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    const char **table;
    size_t table_count;

    if (speed == USB_SPEED_SUPER) {
        table = string_descriptors_ss;
        table_count = sizeof(string_descriptors_ss) / sizeof(string_descriptors_ss[0]);
    } else if (speed == USB_SPEED_HIGH) {
        table = string_descriptors_hs;
        table_count = sizeof(string_descriptors_hs) / sizeof(string_descriptors_hs[0]);
    } else {
        table = string_descriptors_fs;
        table_count = sizeof(string_descriptors_fs) / sizeof(string_descriptors_fs[0]);
    }

    if (index >= table_count) {
        return RT_NULL;
    }

    return table[index];
}

static const struct usb_descriptor dual_cdc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = RT_NULL,
    .string_descriptor_callback = string_descriptor_callback,
    .bos_descriptor = &bos_desc,
};

static struct usbd_interface cdc_intf[CONFIG_USBDEV_MAX_BUS];
static struct usbd_interface cdc_data_intf[CONFIG_USBDEV_MAX_BUS];
static struct usbd_endpoint cdc_out_ep[CONFIG_USBDEV_MAX_BUS];
static struct usbd_endpoint cdc_in_ep[CONFIG_USBDEV_MAX_BUS];
static struct usbd_endpoint cdc_int_ep[CONFIG_USBDEV_MAX_BUS];

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t cdc_rx_buffer[CONFIG_USBDEV_MAX_BUS][CDC_SS_MPS];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t cdc_tx_buffer[CONFIG_USBDEV_MAX_BUS][CDC_SS_MPS];
static uint8_t cdc_tx_busy[CONFIG_USBDEV_MAX_BUS];
static uint8_t cdc_bus_registered[CONFIG_USBDEV_MAX_BUS];
static uint8_t cdc_bus_initialized[CONFIG_USBDEV_MAX_BUS];
static uint8_t cdc_bus_configured[CONFIG_USBDEV_MAX_BUS];
static char cdc_rx_current[CONFIG_USBDEV_MAX_BUS][CDC_RX_LINE_BYTES];
static uint8_t cdc_rx_current_len[CONFIG_USBDEV_MAX_BUS];
static char cdc_rx_line[CONFIG_USBDEV_MAX_BUS][CDC_RX_LINE_COUNT][CDC_RX_LINE_BYTES];
static uint8_t cdc_rx_line_len[CONFIG_USBDEV_MAX_BUS][CDC_RX_LINE_COUNT];
static volatile uint8_t cdc_rx_line_head[CONFIG_USBDEV_MAX_BUS];
static volatile uint8_t cdc_rx_line_tail[CONFIG_USBDEV_MAX_BUS];
static volatile uint8_t cdc_rx_line_overflow[CONFIG_USBDEV_MAX_BUS];

static const char *cdc_bus_name(uint8_t busid)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return "USBSS";
    case USB_CH32H417_BUS_FS:
        return "USBFS";
    case USB_CH32H417_BUS_HS:
        return "USBHS";
    default:
        return "USB?";
    }
}

static uint32_t cdc_bus_mps(uint8_t busid)
{
    if (busid == USB_CH32H417_BUS_SS) {
        return CDC_SS_MPS;
    }
    if (busid == USB_CH32H417_BUS_HS) {
        return CDC_HS_MPS;
    }
    return CDC_FS_MPS;
}

static void cdc_submit_read(uint8_t busid)
{
    if (busid >= CONFIG_USBDEV_MAX_BUS) {
        return;
    }

    usbd_ep_start_read(busid, CDC_OUT_EP, cdc_rx_buffer[busid], cdc_bus_mps(busid));
}

static void cdc_reset_rx(uint8_t busid)
{
    if (busid >= CONFIG_USBDEV_MAX_BUS) {
        return;
    }

    cdc_rx_current_len[busid] = 0U;
    cdc_rx_line_head[busid] = 0U;
    cdc_rx_line_tail[busid] = 0U;
    cdc_rx_line_overflow[busid] = 0U;
}

static void cdc_finish_rx_line(uint8_t busid)
{
    uint8_t head;
    uint8_t next;
    uint8_t len;

    if (busid >= CONFIG_USBDEV_MAX_BUS) {
        return;
    }

    len = cdc_rx_current_len[busid];
    if (len == 0U) {
        return;
    }

    head = cdc_rx_line_head[busid];
    next = (uint8_t)((head + 1U) % CDC_RX_LINE_COUNT);
    if (next == cdc_rx_line_tail[busid]) {
        cdc_rx_line_overflow[busid] = 1U;
        cdc_rx_current_len[busid] = 0U;
        return;
    }

    memcpy(cdc_rx_line[busid][head], cdc_rx_current[busid], len);
    cdc_rx_line[busid][head][len] = '\0';
    cdc_rx_line_len[busid][head] = len;
    cdc_rx_line_head[busid] = next;
    cdc_rx_current_len[busid] = 0U;
}

static void cdc_queue_rx_byte(uint8_t busid, uint8_t value)
{
    uint8_t len;

    if (busid >= CONFIG_USBDEV_MAX_BUS) {
        return;
    }

    if ((value == '\r') || (value == '\n')) {
        cdc_finish_rx_line(busid);
        return;
    }

    if ((value == '\b') || (value == 0x7FU)) {
        if (cdc_rx_current_len[busid] > 0U) {
            cdc_rx_current_len[busid]--;
        }
        return;
    }

    if ((value < 0x20U) || (value > 0x7EU)) {
        return;
    }

    len = cdc_rx_current_len[busid];
    if (len >= (CDC_RX_LINE_BYTES - 1U)) {
        cdc_rx_line_overflow[busid] = 1U;
        return;
    }

    cdc_rx_current[busid][len] = (char)value;
    cdc_rx_current_len[busid] = (uint8_t)(len + 1U);
}

static void cdc_acm_data_recv(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    uint32_t i;

    (void)ep;

    if (busid >= CONFIG_USBDEV_MAX_BUS) {
        return;
    }

    if (nbytes == 0U) {
        cdc_submit_read(busid);
        return;
    }

    if (nbytes > cdc_bus_mps(busid)) {
        nbytes = cdc_bus_mps(busid);
    }

    for (i = 0U; i < nbytes; i++) {
        cdc_queue_rx_byte(busid, cdc_rx_buffer[busid][i]);
    }

#if APP_USB_CDC_RX_ECHO
    if (cdc_tx_busy[busid] != 0U) {
        cdc_submit_read(busid);
        return;
    }

    memcpy(cdc_tx_buffer[busid], cdc_rx_buffer[busid], nbytes);
    cdc_tx_busy[busid] = 1U;
    if (usbd_ep_start_write(busid, CDC_IN_EP, cdc_tx_buffer[busid], nbytes) != 0) {
        cdc_tx_busy[busid] = 0U;
        cdc_submit_read(busid);
    }
#else
    cdc_submit_read(busid);
#endif
}

static void cdc_acm_data_sent(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    (void)nbytes;

    if (busid >= CONFIG_USBDEV_MAX_BUS) {
        return;
    }

    cdc_tx_busy[busid] = 0U;
    cdc_submit_read(busid);
}

static void usb_event_handler(uint8_t busid, uint8_t event)
{
    if (busid >= CONFIG_USBDEV_MAX_BUS) {
        return;
    }

    switch (event) {
    case USBD_EVENT_CONFIGURED:
        cdc_tx_busy[busid] = 0U;
        cdc_bus_configured[busid] = 1U;
        cdc_submit_read(busid);
#if APP_ENABLE_USBFS_CDC_HID
        if (busid == USB_CH32H417_BUS_FS) {
            ch32h417_usbfs_hid_event(event);
        }
#endif
        rt_kprintf("%s CDC configured\r\n", cdc_bus_name(busid));
        break;
    case USBD_EVENT_RESET:
    case USBD_EVENT_DISCONNECTED:
        cdc_tx_busy[busid] = 0U;
        cdc_bus_configured[busid] = 0U;
        cdc_reset_rx(busid);
#if APP_ENABLE_USBFS_CDC_HID
        if (busid == USB_CH32H417_BUS_FS) {
            ch32h417_usbfs_hid_event(event);
        }
#endif
        break;
    default:
        break;
    }
}

static void dual_cdc_register_bus(uint8_t busid)
{
    if ((busid >= CONFIG_USBDEV_MAX_BUS) || (cdc_bus_registered[busid] != 0U)) {
        return;
    }

    cdc_out_ep[busid].ep_addr = CDC_OUT_EP;
    cdc_out_ep[busid].ep_cb = cdc_acm_data_recv;
    cdc_in_ep[busid].ep_addr = CDC_IN_EP;
    cdc_in_ep[busid].ep_cb = cdc_acm_data_sent;
    cdc_int_ep[busid].ep_addr = CDC_INT_EP;
    cdc_int_ep[busid].ep_cb = RT_NULL;

    usbd_desc_register(busid, &dual_cdc_descriptor);
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &cdc_intf[busid]));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &cdc_data_intf[busid]));
    usbd_add_endpoint(busid, &cdc_out_ep[busid]);
    usbd_add_endpoint(busid, &cdc_in_ep[busid]);
    usbd_add_endpoint(busid, &cdc_int_ep[busid]);
#if APP_ENABLE_USBFS_CDC_HID
    if (busid == USB_CH32H417_BUS_FS) {
        ch32h417_usbfs_hid_register_bus();
    }
#endif
    cdc_bus_registered[busid] = 1U;
}

int ch32h417_usbss_cdc_init(void)
{
    int ret;

    if (cdc_bus_initialized[USB_CH32H417_BUS_SS] != 0U) {
        return 0;
    }

    dual_cdc_register_bus(USB_CH32H417_BUS_SS);
    ret = usbd_initialize(USB_CH32H417_BUS_SS, USBSS_BASE, usb_event_handler);
    if (ret == 0) {
        cdc_bus_initialized[USB_CH32H417_BUS_SS] = 1U;
    }
    rt_kprintf("USBSS CDC init %s\r\n", (ret == 0) ? "ok" : "failed");
    return ret;
}

int ch32h417_usbfs_cdc_init(void)
{
    int ret;

    if (cdc_bus_initialized[USB_CH32H417_BUS_FS] != 0U) {
        return 0;
    }

    dual_cdc_register_bus(USB_CH32H417_BUS_FS);
    ret = usbd_initialize(USB_CH32H417_BUS_FS, USBFS_BASE, usb_event_handler);
    if (ret == 0) {
        cdc_bus_initialized[USB_CH32H417_BUS_FS] = 1U;
    }
    rt_kprintf("USBFS CDC init %s\r\n", (ret == 0) ? "ok" : "failed");
    return ret;
}

int ch32h417_usbhs_cdc_init(void)
{
    int ret;

    if (cdc_bus_initialized[USB_CH32H417_BUS_HS] != 0U) {
        return 0;
    }

    dual_cdc_register_bus(USB_CH32H417_BUS_HS);
    ret = usbd_initialize(USB_CH32H417_BUS_HS, USBHS_BASE, usb_event_handler);
    if (ret == 0) {
        cdc_bus_initialized[USB_CH32H417_BUS_HS] = 1U;
    }
    rt_kprintf("USBHS CDC init %s\r\n", (ret == 0) ? "ok" : "failed");
    return ret;
}

static int ch32h417_cdc_write(uint8_t busid, const void *data, uint32_t len)
{
    uint32_t mps;

    if ((busid >= CONFIG_USBDEV_MAX_BUS) || (data == RT_NULL)) {
        return -1;
    }

    if (len == 0U) {
        return 0;
    }

    if ((cdc_bus_initialized[busid] == 0U) || (cdc_bus_configured[busid] == 0U)) {
        return -2;
    }

    if (cdc_tx_busy[busid] != 0U) {
        return -3;
    }

    mps = cdc_bus_mps(busid);
    if (len > mps) {
        len = mps;
    }

    memcpy(cdc_tx_buffer[busid], data, len);
    cdc_tx_busy[busid] = 1U;
    if (usbd_ep_start_write(busid, CDC_IN_EP, cdc_tx_buffer[busid], len) != 0) {
        cdc_tx_busy[busid] = 0U;
        return -4;
    }

    return (int)len;
}

int ch32h417_usbfs_cdc_write(const void *data, uint32_t len)
{
    return ch32h417_cdc_write(USB_CH32H417_BUS_FS, data, len);
}

int ch32h417_usbhs_cdc_write(const void *data, uint32_t len)
{
    return ch32h417_cdc_write(USB_CH32H417_BUS_HS, data, len);
}

int ch32h417_usb_cdc_write(const void *data, uint32_t len)
{
    int ret = -2;

#if APP_ENABLE_USB2_HS_CDC
    ret = ch32h417_usbhs_cdc_write(data, len);
    if (ret > 0) {
        return ret;
    }
#endif

#if APP_ENABLE_USB2_FS_CDC
    ret = ch32h417_usbfs_cdc_write(data, len);
#endif

    return ret;
}

static int cdc_read_line(uint8_t busid, char *out, uint32_t out_len)
{
    uint8_t tail;
    uint8_t len;
    uint32_t copy_len;

    if ((busid >= CONFIG_USBDEV_MAX_BUS) || (out == RT_NULL) || (out_len == 0U)) {
        return -1;
    }

    if (cdc_rx_line_tail[busid] == cdc_rx_line_head[busid]) {
        return 0;
    }

    tail = cdc_rx_line_tail[busid];
    len = cdc_rx_line_len[busid][tail];
    copy_len = len;
    if (copy_len >= out_len) {
        copy_len = out_len - 1U;
    }

    memcpy(out, cdc_rx_line[busid][tail], copy_len);
    out[copy_len] = '\0';
    cdc_rx_line_tail[busid] = (uint8_t)((tail + 1U) % CDC_RX_LINE_COUNT);

    return (int)copy_len;
}

int ch32h417_usbfs_cdc_read_line(char *out, uint32_t out_len)
{
    return cdc_read_line(USB_CH32H417_BUS_FS, out, out_len);
}

int ch32h417_usbhs_cdc_read_line(char *out, uint32_t out_len)
{
    return cdc_read_line(USB_CH32H417_BUS_HS, out, out_len);
}

int ch32h417_usb_cdc_read_line(char *out, uint32_t out_len)
{
    int ret = 0;

#if APP_ENABLE_USB2_HS_CDC
    ret = ch32h417_usbhs_cdc_read_line(out, out_len);
    if (ret != 0) {
        return ret;
    }
#endif

#if APP_ENABLE_USB2_FS_CDC
    ret = ch32h417_usbfs_cdc_read_line(out, out_len);
#endif

    return ret;
}

void ch32h417_dual_cdc_poll(void)
{
#if !APP_ENABLE_USBSS_CDC
    return;
#elif defined(APP_USBSS_SKIP_FOR_V3F_OFFICIAL) && (APP_USBSS_SKIP_FOR_V3F_OFFICIAL != 0)
    return;
#else
    uint32_t reason = 0U;
    static uint8_t fallback_reported;

    if (cdc_bus_initialized[USB_CH32H417_BUS_HS] != 0U) {
        return;
    }

    if (usb_dc_ch32h417_usbss_take_hs_fallback_request(&reason) != 0U) {
        if (fallback_reported == 0U) {
            fallback_reported = 1U;
            rt_kprintf("USBSS requested USBHS fallback reason=%u ignored for SS debug\r\n",
                       (unsigned int)reason);
        }
    }
#endif
}

int ch32h417_dual_cdc_init(void)
{
    int ret_fs = -1;
    int ret_hs = -1;
    int ret_ss = -1;

#if APP_ENABLE_USBSS_CDC
#if defined(APP_USBSS_SKIP_FOR_V3F_OFFICIAL) && (APP_USBSS_SKIP_FOR_V3F_OFFICIAL != 0)
    rt_kprintf("USBSS CDC skipped on V5F; V3F official CH372 stack owns USBSS\r\n");
#else
    ret_ss = ch32h417_usbss_cdc_init();
#endif
#endif

#if APP_ENABLE_USB2_HS_CDC
    ret_hs = ch32h417_usbhs_cdc_init();
#endif

#if APP_ENABLE_USB2_FS_CDC
    ret_fs = ch32h417_usbfs_cdc_init();
#endif

    return ((ret_ss == 0) || (ret_hs == 0) || (ret_fs == 0)) ? 0 : -1;
}
