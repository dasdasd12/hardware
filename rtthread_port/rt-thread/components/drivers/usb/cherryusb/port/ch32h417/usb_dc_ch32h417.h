/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_DC_CH32H417_H
#define USB_DC_CH32H417_H

/* Include CherryUSB first so its definitions take precedence */
#include "usbd_core.h"
#include "ch32h417.h"

/* CH32H417 USBSS base address (only define if ch32h417.h hasn't already) */
#ifndef USBSS_BASE
#define USBSS_BASE ((uintptr_t)0x40034000)
#endif

/* CH32H417 USBHS base address (only define if unavailable in SDK headers) */
#ifndef USBHS_BASE
#define USBHS_BASE ((uintptr_t)0x40030000)
#endif

/* CH32H417 USBFS base address (only define if unavailable in SDK headers) */
#ifndef USBFS_BASE
#define USBFS_BASE ((uintptr_t)0x40023400)
#endif

/* USBSS IRQ numbers are defined in ch32h417.h as enum values;
 * do NOT redefine them as macros here. */

#define USB_CH32H417_BUS_SS 0
#define USB_CH32H417_BUS_FS 1
#define USB_CH32H417_BUS_HS 2

/* Endpoint count supports EP0 + up to 7 additional endpoints */
#define USB_CH32H417_MAX_EP_NUM 8

/* Endpoint 0 max packet sizes for the two device controllers */
#define USB_CH32H417_FS_EP0_MPS 64
#define USB_CH32H417_SS_EP0_MPS 512
#define USB_CH32H417_EP0_MPS    USB_CH32H417_FS_EP0_MPS

void ch32h417_usbss_isr(void);
void ch32h417_usbss_link_isr(void);
void usb_dc_ch32h417_dump_diag(void);

#endif /* USB_DC_CH32H417_H */
