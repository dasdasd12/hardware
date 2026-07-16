# CH32H417 production firmware

This directory builds the deployable dual-core keyboard firmware. Hardware
experiments and diagnostic-only images belong under `hardware/hw_tests`.

Run `make` in this directory to build both production cores:

- `build/V3F/h417_V3F.hex`: complete keyboard controller. V3F owns USBHS HID,
  USBFS CDC, both CH585 links, profile synchronization, RF output, and RGB.
- `build/V5F/rtthread_ch32h417_V5F.hex`: RT-Thread display controller. V5F owns
  the 800x480 RGB panel and does not initialize the keyboard or USB paths.
  Its default image is the validated Claude Code-style welcome frame from the
  LTDC hardware test: `Welcome back!` above the centered orange mascot.

V3F configures all of its peripherals before waking V5F at flash offset
`0x00010000`. This avoids concurrent read-modify-write initialization of shared
RCC and GPIO registers.

`Fn + five-way center` is reserved for the software companion. V3F emits HID
F24 on USB, while the left CH585 emits the same F24 action for RF24/BLE. The
normal center-button Enter action is suppressed until the center pulse releases.

Physical `Fn + 0/1/2/3` is reserved for local Profile selection: `0` selects
the embedded factory Profile and `1..3` select the persistent user slots. The
chord is consumed on both USB and wireless paths, independent of Profile
remapping. An erased user slot behaves as a copy of the embedded factory
Profile until the PC writes a package into that slot; no boot-time Flash copy
is needed. The selected slot number is still retained across reset and power
loss.

The canonical targets are `all`, `v3f`, and `v5f`. There are no reduced or
probe firmware variants in this directory.

`make flash` uses the repository MounRiver/WCH-Link automation with explicit
production HEX paths. It programs V3F first, then programs V5F at
`0x08010000` without an erase-all operation, verifies both images, and resets.
The initial V3F erase-all also clears the user Profile slot region; preserving
written user Profiles across a firmware upgrade requires backup and restore.
After erase-all, all three user slots again use the embedded factory fallback.
