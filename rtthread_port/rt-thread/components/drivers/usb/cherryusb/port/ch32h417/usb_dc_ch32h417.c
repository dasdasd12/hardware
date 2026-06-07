/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_dc_ch32h417.h"
#include "usb_dc_ch32h417_usbhs.h"

int usb_dc_init(uint8_t busid)
{
    return usb_dc_ch32h417_usbhs_init(busid);
}

int usb_dc_deinit(uint8_t busid)
{
    return usb_dc_ch32h417_usbhs_deinit(busid);
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    return usb_dc_ch32h417_usbhs_set_address(busid, addr);
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    return usb_dc_ch32h417_usbhs_set_remote_wakeup(busid);
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    return usb_dc_ch32h417_usbhs_get_port_speed(busid);
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    return usb_dc_ch32h417_usbhs_ep_open(busid, ep);
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    return usb_dc_ch32h417_usbhs_ep_close(busid, ep);
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    return usb_dc_ch32h417_usbhs_set_stall(busid, ep);
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    return usb_dc_ch32h417_usbhs_clear_stall(busid, ep);
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    return usb_dc_ch32h417_usbhs_is_stalled(busid, ep, stalled);
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    return usb_dc_ch32h417_usbhs_start_write(busid, ep, data, data_len);
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    return usb_dc_ch32h417_usbhs_start_read(busid, ep, data, data_len);
}

