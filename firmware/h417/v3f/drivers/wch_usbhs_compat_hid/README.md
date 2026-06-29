# WCH USBHS Compatibility HID

This directory contains the WCH CH32H417 USBHS CompatibilityHID device stack files used by the H417 standalone USB enumeration test.

Copied unchanged from the WCH CH32H417EVT USBHS CompatibilityHID example, limited to the files required by the local wrapper:

- `src/ch32h417_usbhs_device.c`
- `src/usb_desc.c`
- `include/ch32h417_usbhs_device.h`
- `include/usb_desc.h`
- `include/usbd_compatibility_hid.h`
- `system/v3f/system_ch32h417.c`

The V3F `system_ch32h417.c` is kept with this driver because the official USBHS stack depends on the HSE-based system clock setup used by the vendor example. The example `main.c`, board helper, and report demo loop are intentionally not included here.
