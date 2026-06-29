/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/05/26
 * Description        : usb device descriptor,configuration descriptor,
 *                      string descriptors and other descriptors.
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#include "usb_desc.h"

/* Device Descriptor */
const uint8_t  MyDevDescr[] =
{
    0x12,                                               // bLength
    0x01,                                               // bDescriptorType (Device)
    0x10, 0x01,                                         // bcdUSB 1.10
#if V3F_ENABLE_USBFS_CDC_DEBUG
    0xEF,                                               // bDeviceClass Miscellaneous
    0x02,                                               // bDeviceSubClass Common Class
    0x01,                                               // bDeviceProtocol IAD
#else
    0x00,                                               // bDeviceClass (Use class information in the Interface Descriptors)
    0x00,                                               // bDeviceSubClass
    0x00,                                               // bDeviceProtocol
#endif
    DEF_USBD_UEP0_SIZE,                                 // bMaxPacketSize0
    (uint8_t)DEF_USB_VID, (uint8_t)(DEF_USB_VID >> 8),  // idVendor 0x1A86
    (uint8_t)DEF_USB_PID, (uint8_t)(DEF_USB_PID >> 8),  // idProduct 0xFE07
    0x00, DEF_IC_PRG_VER,                               // bcdDevice 1.00
    0x01,                                               // iManufacturer (String Index)
    0x02,                                               // iProduct (String Index)
    0x03,                                               // iSerialNumber (String Index)
    0x01,                                               // bNumConfigurations 1
};

/* Configuration Descriptor */
const uint8_t  MyCfgDescr[] =
{
#if V3F_ENABLE_USBFS_CDC_DEBUG
    /* Configuration Descriptor */
    0x09,                           // bLength
    0x02,                           // bDescriptorType
    0x64, 0x00,                     // wTotalLength
    0x03,                           // bNumInterfaces
    0x01,                           // bConfigurationValue
    0x00,                           // iConfiguration
    0xA0,                           // bmAttributes Bus Powered, Remote Wakeup
    0x32,                           // bMaxPower 100mA

    /* IAD Descriptor for CDC interfaces 0/1 */
    0x08, 0x0B,                     // bLength, bDescriptorType
    0x00,                           // bFirstInterface
    0x02,                           // bInterfaceCount
    0x02,                           // bFunctionClass CDC
    0x02,                           // bFunctionSubClass ACM
    0x01,                           // bFunctionProtocol AT commands
    0x00,                           // iFunction

    /* Interface 0: CDC ACM control */
    0x09,                           // bLength
    0x04,                           // bDescriptorType (Interface)
    0x00,                           // bInterfaceNumber 0
    0x00,                           // bAlternateSetting
    0x01,                           // bNumEndpoints
    0x02,                           // bInterfaceClass CDC
    0x02,                           // bInterfaceSubClass ACM
    0x01,                           // bInterfaceProtocol AT commands
    0x00,                           // iInterface

    0x05, 0x24, 0x00, 0x10, 0x01,   // CDC header functional descriptor
    0x05, 0x24, 0x01, 0x00, 0x01,   // CDC call management descriptor
    0x04, 0x24, 0x02, 0x02,         // CDC ACM descriptor
    0x05, 0x24, 0x06, 0x00, 0x01,   // CDC union descriptor

    /* CDC notification endpoint */
    0x07,                           // bLength
    0x05,                           // bDescriptorType
    0x84,                           // bEndpointAddress: CDC interrupt IN EP4
    0x03,                           // bmAttributes interrupt
    0x08, 0x00,                     // wMaxPacketSize
    0x10,                           // bInterval

    /* Interface 1: CDC data */
    0x09,                           // bLength
    0x04,                           // bDescriptorType (Interface)
    0x01,                           // bInterfaceNumber 1
    0x00,                           // bAlternateSetting
    0x02,                           // bNumEndpoints
    0x0A,                           // bInterfaceClass CDC data
    0x00,                           // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface

    /* CDC data OUT endpoint */
    0x07,                           // bLength
    0x05,                           // bDescriptorType
    0x01,                           // bEndpointAddress: CDC bulk OUT EP1
    0x02,                           // bmAttributes bulk
    0x40, 0x00,                     // wMaxPacketSize
    0x00,                           // bInterval

    /* CDC data IN endpoint */
    0x07,                           // bLength
    0x05,                           // bDescriptorType
    0x83,                           // bEndpointAddress: CDC bulk IN EP3
    0x02,                           // bmAttributes bulk
    0x40, 0x00,                     // wMaxPacketSize
    0x00,                           // bInterval

    /* Interface 2: HID keyboard */
    0x09,                           // bLength
    0x04,                           // bDescriptorType (Interface)
    0x02,                           // bInterfaceNumber 2
    0x00,                           // bAlternateSetting
    0x01,                           // bNumEndpoints
    0x03,                           // bInterfaceClass HID
    0x00,                           // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface
#else
    /* Configuration Descriptor */
    0x09,                           // bLength
    0x02,                           // bDescriptorType
    0x22, 0x00,                     // wTotalLength
    0x01,                           // bNumInterfaces
    0x01,                           // bConfigurationValue
    0x00,                           // iConfiguration
    0xA0,                           // bmAttributes Bus Powered, Remote Wakeup
    0x32,                           // bMaxPower 100mA

    /* Interface Descriptor */
    0x09,                           // bLength
    0x04,                           // bDescriptorType (Interface)
    0x00,                           // bInterfaceNumber 0
    0x00,                           // bAlternateSetting
    0x01,                           // bNumEndpoints 1
    0x03,                           // bInterfaceClass HID
    0x00,                           // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface (String Index)
#endif

    /* HID Descriptor */
    0x09,                           // bLength
    0x21,                           // bDescriptorType
    0x11, 0x01,                     // bcdHID
    0x00,                           // bCountryCode
    0x01,                           // bNumDescriptors
    0x22,                           // bDescriptorType
    DEF_USBD_REPORT_DESC_LEN & 0xFF, DEF_USBD_REPORT_DESC_LEN >> 8, // wDescriptorLength

    /* HID keyboard IN endpoint */
    0x07,                           // bLength
    0x05,                           // bDescriptorType
    0x82,                           // bEndpointAddress: HID IN EP2
    0x03,                           // bmAttributes
    0x10, 0x00,                     // wMaxPacketSize
    0x01,                           // bInterval: 1mS
};

/* HID Report Descriptor */
const uint8_t  MyHIDReportDesc[ ] =
{
    0x05, 0x01,                     // Usage Page (Generic Desktop)
    0x09, 0x06,                     // Usage (Keyboard)
    0xA1, 0x01,                     // Collection (Application)
    0x05, 0x07,                     //   Usage Page (Keyboard)
    0x19, 0xE0,                     //   Usage Minimum (Left Control)
    0x29, 0xE7,                     //   Usage Maximum (Right GUI)
    0x15, 0x00,                     //   Logical Minimum (0)
    0x25, 0x01,                     //   Logical Maximum (1)
    0x75, 0x01,                     //   Report Size (1)
    0x95, 0x08,                     //   Report Count (8)
    0x81, 0x02,                     //   Input (Data,Var,Abs)
    0x75, 0x08,                     //   Report Size (8)
    0x95, 0x01,                     //   Report Count (1)
    0x81, 0x03,                     //   Input (Const,Var,Abs)
    0x05, 0x07,                     //   Usage Page (Keyboard)
    0x19, 0x04,                     //   Usage Minimum (Keyboard A)
    0x29, 0x73,                     //   Usage Maximum (Keyboard F24)
    0x15, 0x00,                     //   Logical Minimum (0)
    0x25, 0x01,                     //   Logical Maximum (1)
    0x75, 0x01,                     //   Report Size (1)
    0x95, 0x70,                     //   Report Count (112)
    0x81, 0x02,                     //   Input (Data,Var,Abs)
    0xC0,                           // End Collection
};

/* Language Descriptor */
const uint8_t  MyLangDescr[] =
{
    0x04, 0x03, 0x09, 0x04
};

/* Manufacturer Descriptor */
const uint8_t  MyManuInfo[ ] =
{
    0x0E, 
    0x03, 
    'w', 0, 
    'c', 0, 
    'h', 0, 
    '.', 0, 
    'c', 0, 
    'n', 0
};

/* Product Information */
const uint8_t MyProdInfo[ ]  =
{
    0x22,
    0x03, 
    'A', 0,
    'I', 0,
    ' ', 0,
    'K', 0,
    'e', 0,
    'y', 0,
    ' ', 0,
    'H', 0,
    '4', 0, 
    '1', 0, 
    '7', 0,
    ' ', 0,
    'N', 0,
    'K', 0,
    'R', 0,
    'O', 0
};

/* Serial Number Information */
const uint8_t  MySerNumInfo[ ] =
{
    0x16, 
    0x03, 
    '0', 0, 
    '1', 0, 
    '2', 0, 
    '3', 0, 
    '4', 0, 
    '5', 0, 
    '6', 0, 
    '7', 0, 
    '8', 0, 
    '9', 0
};

