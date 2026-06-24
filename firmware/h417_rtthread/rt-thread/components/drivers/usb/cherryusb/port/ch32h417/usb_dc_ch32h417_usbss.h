/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_DC_CH32H417_USBSS_H
#define USB_DC_CH32H417_USBSS_H

#include "usb_dc_ch32h417.h"

int usb_dc_ch32h417_usbss_init(uint8_t busid);
int usb_dc_ch32h417_usbss_deinit(uint8_t busid);
int usb_dc_ch32h417_usbss_set_address(uint8_t busid, uint8_t addr);
int usb_dc_ch32h417_usbss_set_remote_wakeup(uint8_t busid);
uint8_t usb_dc_ch32h417_usbss_get_port_speed(uint8_t busid);
int usb_dc_ch32h417_usbss_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep);
int usb_dc_ch32h417_usbss_ep_close(uint8_t busid, uint8_t ep);
int usb_dc_ch32h417_usbss_set_stall(uint8_t busid, uint8_t ep);
int usb_dc_ch32h417_usbss_clear_stall(uint8_t busid, uint8_t ep);
int usb_dc_ch32h417_usbss_is_stalled(uint8_t busid, uint8_t ep, uint8_t *stalled);
int usb_dc_ch32h417_usbss_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len);
int usb_dc_ch32h417_usbss_start_read(uint8_t busid, uint8_t ep, uint8_t *data, uint32_t data_len);
uint8_t usb_dc_ch32h417_usbss_take_hs_fallback_request(uint32_t *reason);
void usb_dc_ch32h417_usbss_dump_diag(void);

#endif /* USB_DC_CH32H417_USBSS_H */
