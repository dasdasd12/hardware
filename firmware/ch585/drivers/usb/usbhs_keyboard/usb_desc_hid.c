/*******************************************************************************
 * usb_desc_hid.c — CH585 键盘 HID USB 描述符
 *
 * 接口结构：
 *   Interface 0：NKRO Keyboard（EP1 IN，bInterval=1，高速 125µs）
 *   Interface 1：Consumer Control（EP2 IN，bInterval=4）
 *   Interface 2：Custom/Config（EP3 IN+OUT，64B，bInterval=1）
 *
 * 与 f207 协议兼容：配置命令和 ADC 监控数据通过 EP3 传输，
 * 使用 0xAA 包头区分 ADC 数据包。
 ******************************************************************************/

#include "CH58x_common.h"
#include "usb_hid.h"

/* ─── 设备描述符 VID/PID（与 f207 一致）─── */
#define DEF_USB_VID     0x1A86
#define DEF_USB_PID     0xFE07
#define DEF_USB_BCD     0x0200   /* USB 2.0 */
#define DEF_IC_PRG_VER  0x01
#define DEF_USBD_UEP0_SIZE  64

/* ─── NKRO 键盘 Report Descriptor ─── */
/* 与 f207 格式兼容：
 *   Byte 0  = Modifiers
 *   Byte 1  = Reserved
 *   Byte 2-15 = 112-key NKRO bitmap（Usage 0x04–0x6F）*/
const uint8_t KeyRepDesc_NKRO[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)

    // Modifier bits
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0xE0,  // Usage Min (224)
    0x29, 0xE7,  // Usage Max (231)
    0x15, 0x00,  // Logical Min (0)
    0x25, 0x01,  // Logical Max (1)
    0x75, 0x01,  // Report Size (1)
    0x95, 0x08,  // Report Count (8)
    0x81, 0x02,  // Input (Data,Var,Abs) — Modifier byte

    // Reserved byte
    0x75, 0x08,  // Report Size (8)
    0x95, 0x01,  // Report Count (1)
    0x81, 0x03,  // Input (Const,Var,Abs) — Reserved

    // NKRO bitmap for keys 0x04~0x6F (112 keys)
    0x15, 0x00,  // Logical Min (0)
    0x25, 0x01,  // Logical Max (1)
    0x75, 0x01,  // Report Size (1)
    0x95, 0x70,  // Report Count (112)
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0x04,  // Usage Min (4)
    0x29, 0x73,  // Usage Max (115 = 0x73)
    0x81, 0x02,  // Input (Data,Var,Abs)

    // Padding to byte-align (128 - 8 - 8 - 112 = 0 extra bits; 16 bytes total)
    0xC0         // End Collection
};

/* ─── Consumer Control Report Descriptor ─── */
const uint8_t KeyRepDesc_Consumer[] = {
    0x05, 0x0C,  // Usage Page (Consumer)
    0x09, 0x01,  // Usage (Consumer Control)
    0xA1, 0x01,  // Collection (Application)
    0x15, 0x00,  // Logical Min (0)
    0x26, 0xFF, 0x03, // Logical Max (1023)
    0x19, 0x00,  // Usage Min (0)
    0x2A, 0xFF, 0x03, // Usage Max (0x3FF)
    0x75, 0x10,  // Report Size (16)
    0x95, 0x01,  // Report Count (1)
    0x81, 0x00,  // Input (Data,Array,Abs)
    0xC0         // End Collection
};

const uint8_t KeyRepDesc_Custom[] = {
    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,        // Usage (1)
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        // Logical Min (0)
    0x26, 0xFF, 0x00,  // Logical Max (255)
    0x75, 0x08,        // Report Size (8)
    0x95, 0x40,        // Report Count (64)
    0x09, 0x01,        // Usage (1)
    0x81, 0x02,        // Input (Data,Var,Abs)
    0x95, 0x40,        // Report Count (64)
    0x09, 0x01,        // Usage (1)
    0x91, 0x02,        // Output (Data,Var,Abs)
    0xC0               // End Collection
};

/* ─── 设备描述符 ─── */
const uint8_t MyDevDescr[] = {
    0x12,                                    // bLength
    0x01,                                    // bDescriptorType (Device)
    0x00, 0x02,                              // bcdUSB 2.0
    0x00,                                    // bDeviceClass (per interface)
    0x00,                                    // bDeviceSubClass
    0x00,                                    // bDeviceProtocol
    DEF_USBD_UEP0_SIZE,                      // bMaxPacketSize0
    (uint8_t)DEF_USB_VID, (uint8_t)(DEF_USB_VID >> 8),
    (uint8_t)DEF_USB_PID, (uint8_t)(DEF_USB_PID >> 8),
    0x00, DEF_IC_PRG_VER,                    // bcdDevice
    0x01,                                    // iManufacturer
    0x02,                                    // iProduct
    0x03,                                    // iSerialNumber
    0x01                                     // bNumConfigurations
};

/* ─── 配置描述符（含3个接口）─── */
/* wTotalLength = 9 + (9+9+7)×2 + (9+9+7+7) = 9+50+32 = 91 bytes */
const uint8_t MyCfgDescr[] = {
    /* Configuration */
    0x09, 0x02, 0x5B, 0x00, /* wTotalLength=91 */
    0x03,                   /* bNumInterfaces */
    0x01, 0x00,             /* bConfigurationValue, iConfiguration */
    0xA0, 0x32,             /* bmAttributes（Bus+Remote Wakeup），MaxPower=100mA */

    /* ── Interface 0：Keyboard ── */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
    /* HID Descriptor */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)sizeof(KeyRepDesc_NKRO),
    (uint8_t)(sizeof(KeyRepDesc_NKRO) >> 8),
    /* EP1 IN，Interrupt，64B，bInterval=1（高速=125µs） */
    0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01,

    /* ── Interface 1：Consumer Control ── */
    0x09, 0x04, 0x01, 0x00, 0x01, 0x03, 0x01, 0x00, 0x00,
    /* HID Descriptor */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)sizeof(KeyRepDesc_Consumer),
    (uint8_t)(sizeof(KeyRepDesc_Consumer) >> 8),
    /* EP2 IN，Interrupt，8B，bInterval=4（高速=500µs） */
    0x07, 0x05, 0x82, 0x03, 0x08, 0x00, 0x04,

    /* ── Interface 2：Custom Config ── */
    0x09, 0x04, 0x02, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
    /* HID Descriptor（无报告描述符直接返回0长度时主机不要求它）*/
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)sizeof(KeyRepDesc_Custom),
    (uint8_t)(sizeof(KeyRepDesc_Custom) >> 8),
    /* EP3 IN，64B */
    0x07, 0x05, 0x83, 0x03, 0x40, 0x00, 0x01,
    /* EP3 OUT，64B */
    0x07, 0x05, 0x03, 0x03, 0x40, 0x00, 0x01,
};

/* ─── 语言描述符 ─── */
const uint8_t MyLangDescr[] = { 0x04, 0x03, 0x09, 0x04 };

/* ─── 厂商字符串 ─── */
const uint8_t MyManuInfo[] = { 0x0E, 0x03,
    'K',0, 'e',0, 'y',0, 'b',0, 'd',0, 'H',0 };

/* ─── 产品字符串 ─── */
const uint8_t MyProdInfo[] = { 0x1A, 0x03,
    'H',0,'a',0,'l',0,'l',0,' ',0,'K',0,'e',0,'y',0,'b',0,'o',0,'a',0,'r',0,'d',0 };

/* ─── 序列号字符串 ─── */
const uint8_t MySerialInfo[] = { 0x0A, 0x03,
    'C',0,'H',0,'5',0,'8',0,'5',0 };
