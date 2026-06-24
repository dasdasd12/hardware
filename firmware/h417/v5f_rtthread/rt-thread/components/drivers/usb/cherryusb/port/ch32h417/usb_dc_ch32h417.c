/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_dc_ch32h417.h"
#include "usb_dc_ch32h417_usbss.h"
#include "usb_dc_ch32h417_usbfs.h"
#include "usb_dc_ch32h417_usbhs.h"

void usb_dc_ch32h417_dump_diag(void)
{
    usb_dc_ch32h417_usbss_dump_diag();
    usb_dc_ch32h417_usbfs_dump_diag();
    usb_dc_ch32h417_usbhs_dump_diag();
}

int usb_dc_init(uint8_t busid)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_init(busid);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_init(busid);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_init(busid);
    default:
        return -1;
    }
}

int usb_dc_deinit(uint8_t busid)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_deinit(busid);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_deinit(busid);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_deinit(busid);
    default:
        return -1;
    }
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_set_address(busid, addr);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_set_address(busid, addr);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_set_address(busid, addr);
    default:
        return -1;
    }
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_set_remote_wakeup(busid);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_set_remote_wakeup(busid);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_set_remote_wakeup(busid);
    default:
        return -1;
    }
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_get_port_speed(busid);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_get_port_speed(busid);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_get_port_speed(busid);
    default:
        return USB_SPEED_UNKNOWN;
    }
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_ep_open(busid, ep);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_ep_open(busid, ep);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_ep_open(busid, ep);
    default:
        return -1;
    }
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_ep_close(busid, ep);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_ep_close(busid, ep);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_ep_close(busid, ep);
    default:
        return -1;
    }
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_set_stall(busid, ep);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_set_stall(busid, ep);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_set_stall(busid, ep);
    default:
        return -1;
    }
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_clear_stall(busid, ep);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_clear_stall(busid, ep);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_clear_stall(busid, ep);
    default:
        return -1;
    }
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_is_stalled(busid, ep, stalled);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_is_stalled(busid, ep, stalled);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_is_stalled(busid, ep, stalled);
    default:
        return -1;
    }
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_start_write(busid, ep, data, data_len);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_start_write(busid, ep, data, data_len);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_start_write(busid, ep, data, data_len);
    default:
        return -1;
    }
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    switch (busid) {
    case USB_CH32H417_BUS_SS:
        return usb_dc_ch32h417_usbss_start_read(busid, ep, data, data_len);
    case USB_CH32H417_BUS_FS:
        return usb_dc_ch32h417_usbfs_start_read(busid, ep, data, data_len);
    case USB_CH32H417_BUS_HS:
        return usb_dc_ch32h417_usbhs_start_read(busid, ep, data, data_len);
    default:
        return -1;
    }
}
