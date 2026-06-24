/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_DC_CH32H417_USBHS_H
#define USB_DC_CH32H417_USBHS_H

#include "usb_dc_ch32h417.h"

int usb_dc_ch32h417_usbhs_init(uint8_t busid);
int usb_dc_ch32h417_usbhs_deinit(uint8_t busid);
int usb_dc_ch32h417_usbhs_set_address(uint8_t busid, uint8_t addr);
int usb_dc_ch32h417_usbhs_set_remote_wakeup(uint8_t busid);
uint8_t usb_dc_ch32h417_usbhs_get_port_speed(uint8_t busid);
int usb_dc_ch32h417_usbhs_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep);
int usb_dc_ch32h417_usbhs_ep_close(uint8_t busid, uint8_t ep);
int usb_dc_ch32h417_usbhs_set_stall(uint8_t busid, uint8_t ep);
int usb_dc_ch32h417_usbhs_clear_stall(uint8_t busid, uint8_t ep);
int usb_dc_ch32h417_usbhs_is_stalled(uint8_t busid, uint8_t ep, uint8_t *stalled);
int usb_dc_ch32h417_usbhs_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len);
int usb_dc_ch32h417_usbhs_start_read(uint8_t busid, uint8_t ep, uint8_t *data, uint32_t data_len);
void usb_dc_ch32h417_usbhs_dump_diag(void);
void USBHS_IRQHandler(void);

#endif /* USB_DC_CH32H417_USBHS_H */
