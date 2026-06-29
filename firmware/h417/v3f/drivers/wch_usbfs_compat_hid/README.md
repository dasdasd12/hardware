# WCH USBFS Compatibility HID

This directory contains the WCH CH32H417 USBFS CompatibilityHID device stack files used by the H417 standalone USB enumeration test and V3F product USBFS keyboard path.

The protocol core is copied from the WCH CH32H417EVT USBFS CompatibilityHID `Common` example, limited to the files required by the local wrapper:

- `src/ch32h417_usbfs_device.c`
- `src/usb_desc.c`
- `include/ch32h417_usbfs_device.h`
- `include/usb_desc.h`
- `include/usbd_compatibility_hid.h`

`usb_desc.c` is adapted for the AI keyboard MVP as a USBFS HID keyboard descriptor with one 16-byte NKRO IN report. The example `main.c`, board helper, and report demo loop are intentionally not included here; product firmware wraps this driver with local report policy instead of depending on an external WCH example path.
