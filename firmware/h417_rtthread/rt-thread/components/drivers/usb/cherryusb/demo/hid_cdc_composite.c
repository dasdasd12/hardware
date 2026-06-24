/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "usbd_hid.h"
#include "usb_dc_ch32h417.h"

#define USBD_VID           0x1A86
#define USBD_PID           0xFE30
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define HID_INT_EP          0x84
#define HID_INT_EP_SIZE     8
#define HID_INT_EP_INTERVAL 1

#define HID_KEYBOARD_REPORT_DESC_SIZE 63

#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

#define USB_CONFIG_SIZE_HS (9 + CDC_ACM_DESCRIPTOR_LEN + HID_KEYBOARD_DESCRIPTOR_LEN)
#define USB_CONFIG_SIZE_SS (9 + 8 + 9 + 5 + 5 + 4 + 5 + 7 + 6 + 9 + 7 + 6 + 7 + 6 + 9 + 9 + 7 + 6)

#define USB_DESCRIPTOR_TYPE_BOS                     0x0FU
#define USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION   0x30U

/* Device descriptor: USB 2.0 with IAD support */
static const uint8_t device_descriptor_hs[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01)
};

/* SuperSpeed bMaxPacketSize0 is encoded as log2(512), not as 64 bytes. */
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

/* HS Configuration descriptor (USB2.0 fallback) */
static const uint8_t config_descriptor_hs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE_HS, 0x03, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02),
    HID_KEYBOARD_DESCRIPTOR_INIT(0x02, 0x01, HID_KEYBOARD_REPORT_DESC_SIZE, HID_INT_EP, HID_INT_EP_SIZE, HID_INT_EP_INTERVAL),
};

/* SS Configuration descriptor (USB3.0 SuperSpeed) */
static const uint8_t config_descriptor_ss[] = {
    /* Configuration descriptor */
    0x09,                               /* bLength */
    USB_DESCRIPTOR_TYPE_CONFIGURATION, /* bDescriptorType */
    WBVAL(USB_CONFIG_SIZE_SS),         /* wTotalLength */
    0x03,                               /* bNumInterfaces */
    0x01,                               /* bConfigurationValue */
    0x00,                               /* iConfiguration */
    USB_CONFIG_BUS_POWERED,            /* bmAttributes */
    USBD_MAX_POWER,                    /* bMaxPower */

    /* Interface Association Descriptor */
    0x08,                               /* bLength */
    USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION,
    0x00,                               /* bFirstInterface */
    0x02,                               /* bInterfaceCount */
    USB_DEVICE_CLASS_CDC,              /* bFunctionClass */
    CDC_ABSTRACT_CONTROL_MODEL,        /* bFunctionSubClass */
    CDC_COMMON_PROTOCOL_NONE,          /* bFunctionProtocol */
    0x00,                               /* iFunction */

    /* CDC Control Interface */
    0x09,                               /* bLength */
    USB_DESCRIPTOR_TYPE_INTERFACE,
    0x00,                               /* bInterfaceNumber */
    0x00,                               /* bAlternateSetting */
    0x01,                               /* bNumEndpoints */
    USB_DEVICE_CLASS_CDC,              /* bInterfaceClass */
    CDC_ABSTRACT_CONTROL_MODEL,        /* bInterfaceSubClass */
    CDC_COMMON_PROTOCOL_NONE,          /* bFunctionProtocol */
    0x02,                               /* iInterface */

    /* CDC Header Functional */
    0x05,                               /* bLength */
    CDC_CS_INTERFACE,
    CDC_FUNC_DESC_HEADER,
    WBVAL(CDC_V1_10),

    /* CDC Call Management */
    0x05,                               /* bLength */
    CDC_CS_INTERFACE,
    CDC_FUNC_DESC_CALL_MANAGEMENT,
    0x00,                               /* bmCapabilities */
    0x01,                               /* bDataInterface */

    /* CDC ACM Functional */
    0x04,                               /* bLength */
    CDC_CS_INTERFACE,
    CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT,
    0x02,                               /* bmCapabilities */

    /* CDC Union Functional */
    0x05,                               /* bLength */
    CDC_CS_INTERFACE,
    CDC_FUNC_DESC_UNION,
    0x00,                               /* bMasterInterface */
    0x01,                               /* bSlaveInterface0 */

    /* CDC Interrupt IN Endpoint */
    0x07,                               /* bLength */
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    CDC_INT_EP,                        /* bEndpointAddress */
    0x03,                               /* bmAttributes: Interrupt */
    0x00, 0x04,                        /* wMaxPacketSize: 1024 */
    0x0A,                               /* bInterval */

    /* CDC Interrupt IN Endpoint Companion */
    0x06,                               /* bLength */
    USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION,
    0x00,                               /* bMaxBurst */
    0x00,                               /* bmAttributes */
    0x00, 0x00,                        /* wBytesPerInterval */

    /* CDC Data Interface */
    0x09,                               /* bLength */
    USB_DESCRIPTOR_TYPE_INTERFACE,
    0x01,                               /* bInterfaceNumber */
    0x00,                               /* bAlternateSetting */
    0x02,                               /* bNumEndpoints */
    CDC_DATA_INTERFACE_CLASS,          /* bInterfaceClass */
    0x00,                               /* bInterfaceSubClass */
    0x00,                               /* bInterfaceProtocol */
    0x00,                               /* iInterface */

    /* CDC Bulk OUT Endpoint */
    0x07,                               /* bLength */
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    CDC_OUT_EP,                        /* bEndpointAddress */
    0x02,                               /* bmAttributes: Bulk */
    0x00, 0x04,                        /* wMaxPacketSize: 1024 */
    0x00,                               /* bInterval */

    /* CDC Bulk OUT Endpoint Companion */
    0x06,                               /* bLength */
    USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION,
    0x00,                               /* bMaxBurst: 0 (1 packet per burst) */
    0x00,                               /* bmAttributes */
    0x00, 0x00,                        /* wBytesPerInterval */

    /* CDC Bulk IN Endpoint */
    0x07,                               /* bLength */
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    CDC_IN_EP,                         /* bEndpointAddress */
    0x02,                               /* bmAttributes: Bulk */
    0x00, 0x04,                        /* wMaxPacketSize: 1024 */
    0x00,                               /* bInterval */

    /* CDC Bulk IN Endpoint Companion */
    0x06,                               /* bLength */
    USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION,
    0x00,                               /* bMaxBurst */
    0x00,                               /* bmAttributes */
    0x00, 0x00,                        /* wBytesPerInterval */

    /* HID Interface */
    0x09,                               /* bLength */
    USB_DESCRIPTOR_TYPE_INTERFACE,
    0x02,                               /* bInterfaceNumber */
    0x00,                               /* bAlternateSetting */
    0x01,                               /* bNumEndpoints */
    0x03,                               /* bInterfaceClass: HID */
    0x01,                               /* bInterfaceSubClass: Boot */
    0x01,                               /* bInterfaceProtocol: Keyboard */
    0x00,                               /* iInterface */

    /* HID Descriptor */
    0x09,                               /* bLength */
    HID_DESCRIPTOR_TYPE_HID,
    0x11, 0x01,                        /* bcdHID */
    0x00,                               /* bCountryCode */
    0x01,                               /* bNumDescriptors */
    0x22,                               /* bDescriptorType: Report */
    WBVAL(HID_KEYBOARD_REPORT_DESC_SIZE),

    /* HID Interrupt IN Endpoint */
    0x07,                               /* bLength */
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    HID_INT_EP,                        /* bEndpointAddress */
    0x03,                               /* bmAttributes: Interrupt */
    0x00, 0x04,                        /* wMaxPacketSize: 1024 */
    0x01,                               /* bInterval: 1 (125us for SS) */

    /* HID Interrupt IN Endpoint Companion */
    0x06,                               /* bLength */
    USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION,
    0x00,                               /* bMaxBurst */
    0x00,                               /* bmAttributes */
    WBVAL(HID_INT_EP_SIZE),            /* wBytesPerInterval */
};

/* Device qualifier descriptor */
static const uint8_t device_quality_descriptor[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0xEF,
    0x02,
    0x01,
    0x40,
    0x01,
    0x00,
};

/* BOS descriptor for USB3.0 */
static const uint8_t bos_descriptor[] = {
    /* BOS header */
    0x05,                               /* bLength */
    USB_DESCRIPTOR_TYPE_BOS,           /* bDescriptorType */
    0x16, 0x00,                        /* wTotalLength: 22 */
    0x02,                               /* bNumDeviceCaps */

    /* USB 2.0 Extension */
    0x07,                               /* bLength */
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY,
    USB_DEVICE_CAPABILITY_USB_2_0_EXTENSION,
    0x02, 0x00, 0x00, 0x00,            /* bmAttributes: LPM support */

    /* SuperSpeed Device Capability */
    0x0A,                               /* bLength */
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY,
    USB_DEVICE_CAPABILITY_SUPERSPEED_USB,
    0x00,                               /* bmAttributes */
    0x0E, 0x00,                        /* wSpeedsSupported: SS | HS | FS */
    0x01,                               /* bFunctionalitySupport: FS */
    0x0A, 0x00,                        /* bU1DevExitLat: 10us */
    0x00, 0x00,                        /* wU2DevExitLat: 0us */
};

static const struct usb_bos_descriptor bos_desc = {
    .string = bos_descriptor,
    .string_len = sizeof(bos_descriptor),
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "WCH",                        /* Manufacturer */
    "CH32H417 HID+CDC Composite", /* Product */
    "2025010101",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    if ((speed == USB_SPEED_HIGH) || (speed == USB_SPEED_FULL)) {
        return device_descriptor_hs;
    }
    return device_descriptor_ss;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    if ((speed == USB_SPEED_HIGH) || (speed == USB_SPEED_FULL)) {
        return config_descriptor_hs;
    }
    return config_descriptor_ss;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;
    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor composite_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
    .bos_descriptor = &bos_desc,
};

/* HID keyboard report descriptor (standard 6-key rollover) */
static const uint8_t hid_keyboard_report_desc[HID_KEYBOARD_REPORT_DESC_SIZE] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x06, // USAGE (Keyboard)
    0xA1, 0x01, // COLLECTION (Application)
    0x05, 0x07, // USAGE_PAGE (Key Codes)
    0x19, 0xE0, // USAGE_MINIMUM (224)
    0x29, 0xE7, // USAGE_MAXIMUM (231)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x75, 0x01, // REPORT_SIZE (1)
    0x95, 0x08, // REPORT_COUNT (8)
    0x81, 0x02, // INPUT (Data,Variable,Absolute)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x08, // REPORT_SIZE (8)
    0x81, 0x01, // INPUT (Constant)
    0x95, 0x03, // REPORT_COUNT (3)
    0x75, 0x01, // REPORT_SIZE (1)
    0x05, 0x08, // USAGE_PAGE (LEDs)
    0x19, 0x01, // USAGE_MINIMUM (1)
    0x29, 0x03, // USAGE_MAXIMUM (3)
    0x91, 0x02, // OUTPUT (Data,Variable,Absolute)
    0x95, 0x05, // REPORT_COUNT (5)
    0x75, 0x01, // REPORT_SIZE (1)
    0x91, 0x01, // OUTPUT (Constant,Array,Absolute)
    0x95, 0x06, // REPORT_COUNT (6)
    0x75, 0x08, // REPORT_SIZE (8)
    0x26, 0xFF, 0x00, // LOGICAL_MAXIMUM (255)
    0x05, 0x07, // USAGE_PAGE (Key Codes)
    0x19, 0x00, // USAGE_MINIMUM (0)
    0x29, 0x91, // USAGE_MAXIMUM (145)
    0x81, 0x00, // INPUT(Data,Array,Absolute)
    0xC0        // END_COLLECTION
};

/* CDC ACM interfaces and endpoints */
static struct usbd_interface cdc_intf;
static struct usbd_interface cdc_data_intf;

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t cdc_rx_buffer[1024];

static struct usbd_endpoint cdc_out_ep = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = NULL
};
static struct usbd_endpoint cdc_in_ep = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = NULL
};
static struct usbd_endpoint cdc_int_ep = {
    .ep_addr = CDC_INT_EP,
    .ep_cb = NULL
};

/* HID interface and endpoint */
static struct usbd_interface hid_intf;
static struct usbd_endpoint hid_in_ep = {
    .ep_addr = HID_INT_EP,
    .ep_cb = NULL
};

static void cdc_acm_data_recv(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    if (nbytes > 0) {
        /* Echo back received data */
        usbd_ep_start_write(0, CDC_IN_EP, cdc_rx_buffer, nbytes);
    }
    /* Continue receiving */
    usbd_ep_start_read(0, CDC_OUT_EP, cdc_rx_buffer, sizeof(cdc_rx_buffer));
}

static void cdc_acm_data_sent(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
}

/* USB event handler */
static void usb_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;
    switch (event) {
    case USBD_EVENT_CONFIGURED:
        /* Start CDC OUT endpoint read */
        usbd_ep_start_read(0, CDC_OUT_EP, cdc_rx_buffer, sizeof(cdc_rx_buffer));
        break;
    case USBD_EVENT_RESET:
    case USBD_EVENT_DISCONNECTED:
        break;
    default:
        break;
    }
}

int hid_cdc_composite_init(void)
{
    cdc_out_ep.ep_cb = cdc_acm_data_recv;
    cdc_in_ep.ep_cb = cdc_acm_data_sent;

    usbd_desc_register(0, &composite_descriptor);

    /* CDC ACM: Control + Data interfaces */
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &cdc_intf));
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &cdc_data_intf));
    usbd_add_endpoint(0, &cdc_out_ep);
    usbd_add_endpoint(0, &cdc_in_ep);
    usbd_add_endpoint(0, &cdc_int_ep);

    /* HID Keyboard */
    usbd_add_interface(0, usbd_hid_init_intf(0, &hid_intf, hid_keyboard_report_desc, HID_KEYBOARD_REPORT_DESC_SIZE));
    usbd_add_endpoint(0, &hid_in_ep);

    return usbd_initialize(0, USBHS_BASE, usb_event_handler);
}
