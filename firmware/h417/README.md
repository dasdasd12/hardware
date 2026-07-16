# CH32H417 production firmware

This directory builds the deployable dual-core keyboard firmware. Hardware
experiments and diagnostic-only images belong under `hardware/hw_tests`.

Run `make` in this directory to build both production cores:

- `build/V3F/h417_V3F.hex`: complete keyboard controller. V3F owns USBHS HID,
  USBFS CDC, both CH585 links, profile synchronization, RF output, and RGB.
- `build/V5F/rtthread_ch32h417_V5F.hex`: RT-Thread display controller. V5F owns
  the 800x480 RGB panel and does not initialize the keyboard or USB paths.

V3F configures all of its peripherals before waking V5F at flash offset
`0x00010000`. This avoids concurrent read-modify-write initialization of shared
RCC and GPIO registers.

The canonical targets are `all`, `v3f`, and `v5f`. There are no reduced or
probe firmware variants in this directory.

`make flash` uses the repository MounRiver/WCH-Link automation with explicit
production HEX paths. It programs V3F first, then programs V5F at
`0x08010000` without an erase-all operation, verifies both images, and resets.
