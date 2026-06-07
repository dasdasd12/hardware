/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_DC_CH32H417_H
#define USB_DC_CH32H417_H

/* Include CherryUSB first so its definitions take precedence */
#include "usbd_core.h"

/* CH32H417 USBSS base address (only define if ch32h417.h hasn't already) */
#ifndef USBSS_BASE
#define USBSS_BASE ((uintptr_t)0x40034000)
#endif

/* CH32H417 USBHS base address (only define if unavailable in SDK headers) */
#ifndef USBHS_BASE
#define USBHS_BASE ((uintptr_t)0x40030000)
#endif

/* USBSS IRQ numbers are defined in ch32h417.h as enum values;
 * do NOT redefine them as macros here. */

/* Endpoint count supports EP0 + up to 7 additional endpoints */
#define USB_CH32H417_MAX_EP_NUM 8

/* Endpoint 0 max packet size for USBHS / USB2.0 high-speed mode */
#define USB_CH32H417_EP0_MPS 64

void ch32h417_usbss_isr(void);
void ch32h417_usbss_link_isr(void);

#endif /* USB_DC_CH32H417_H */
