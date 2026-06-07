# Firmware Update Architecture

Firmware update is PC-assisted over the device control channel. The keyboard
does not need cloud or network access for the first implementation.

## Update Targets

Initial targets:

```text
h417_app
ch585
dongle
```

H417 bootloader is a special target and should be treated with stricter rules.
Bootloader update can be deferred until recovery design is proven.

## First-Version Strategy

```text
manifest + hash + A/B slot
```

Development version:

- firmware package manifest
- target/hardware compatibility checks
- image hash verification
- A/B application slot flow for H417 app

Production before release:

- signed firmware package
- bootloader public-key verification
- anti-rollback version
- recovery mode validation

## Package Format

Logical package layout:

```text
firmware-package/
  manifest.json
  images/
    h417_app.bin
    ch585.bin
    dongle.bin
  release_notes.md
  signatures/
```

Manifest fields:

```text
package_version
product
target
image_version
hardware_revision compatibility
protocol_version_min
image path
image size
sha256
requires_bootloader_version
```

Signing fields can be added without changing the transfer protocol family.

## H417 A/B Flow

```text
active slot: A
update slot: B

PC -> Vendor HID -> V5F receiver
V5F writes image to update slot
V5F verifies hash
bootloader marks candidate
reboot
bootloader starts candidate
app self-test passes
app commits slot
failure -> bootloader rolls back to previous slot
```

Slot sizes depend on final Flash layout. The architecture requires A/B first;
if Flash capacity later proves insufficient, the fallback is bootloader +
staging + recovery mode.

## Bootloader Responsibilities

Bootloader owns:

- slot selection
- image validation
- candidate boot
- rollback
- recovery mode entry
- production signature verification, when enabled
- anti-rollback, when enabled

Application owns:

- receiving update through Vendor HID
- writing update slot through controlled storage API
- hash verification before reboot
- self-test and commit request
- user-facing update status

## CH585 Update

CH585 update is bridged through H417:

```text
PC software -> Vendor HID -> H417 V5F -> CH585 update transport
```

Rules:

- package target must match CH585 hardware/bootloader
- H417 tracks transfer progress
- CH585 verifies received image as far as its bootloader supports
- H417 reports final status to PC software
- failed CH585 update must not prevent H417 wired keyboard mode

## Dongle Update

Dongle update is a separate target. Preferred path:

```text
PC software -> dongle vendor channel -> dongle boot/update mode
```

H417 may display dongle update status if PC software projects it, but H417 does
not need to proxy dongle updates unless a later product decision requires it.

## Device Protocol Messages

Firmware update message family:

```text
FW_UPDATE_QUERY
FW_UPDATE_BEGIN
FW_UPDATE_CHUNK
FW_UPDATE_VERIFY
FW_UPDATE_COMMIT
FW_UPDATE_ABORT
FW_UPDATE_STATUS
```

Each transfer has:

- transfer ID
- target
- manifest metadata
- chunk index/offset
- total size
- expected hash
- progress/status

## Safety Rules

- basic keyboard mode should remain recoverable after failed app update
- update images are never executed before validation
- DeviceSettings, ProfilePackage slots, ScreenConfig, LightingConfig, and
  CalibrationData are not erased by firmware update unless explicitly required
  by migration
- bootloader/recovery path must be testable
- production firmware must reject unsigned packages once signing is enabled

## MVP Scope

MVP architecture reserves update messages and package format. Full update
implementation can follow after Vendor HID DeviceSettings and ProfilePackage
slot read/write are stable.

First implementation target:

- query firmware versions
- validate package manifest on PC side
- expose firmware update capability fields
- reserve H417 A/B layout in storage plan

Actual image swap can be implemented after the control plane is reliable.

