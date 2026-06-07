# Hardware Architecture

This document defines the hardware-side product architecture for implementation.
It is the entry point for firmware, board-level, and device-protocol work.

Older pre-design reports are research history. The current product architecture
does not use Ethernet or a device-side WebSocket client.

## Product Topology

```text
PC Software
  Local Core Service
  Profile Library
  DeviceTransport: USB Vendor HID / future BLE GATT / future dongle channel
        |
        | USB composite device
        v
CH32H417
  V5F Application / UI / Protocol Core
  V3F Keyboard Engine Core
        |
        | CH585 coordination link; SPI ingest for non-key control data
        v
CH585
  Wireless and auxiliary I/O protocol processor
  BLE HID
  2.4G private protocol
        |
        v
2.4G USB Dongle
  USB HID + vendor channel to PC
```

## Core Roles

```text
V3F = Keyboard Engine Core
  bare-metal realtime loop
  scan/filter/debounce
  magnetic switch algorithms
  binding scopes / behaviors / macros
  RuntimeIntent generation

V5F = Application / UI / Protocol Core
  RT-Thread Standard
  USB stack and Vendor HID control channel
  config manager and external Flash access
  display UI
  device protocol
  diagnostics aggregation
  CH585 coordination

CH585 = Wireless / Auxiliary I/O Protocol Processor
  BLE HID
  2.4G private protocol
  pairing, retry, encryption, wireless status
  joystick, encoder, and external-module non-key input
  unified control data reporting to H417
```

V3F owns the deterministic keyboard path. V5F owns product features, state
coordination, and communication with PC software.

## Required Offline Behavior

The keyboard must remain a good keyboard when PC software is not running.

Offline-capable behavior:

- standard USB HID keyboard input
- current binding scopes, behaviors, macros, and magnetic settings
- rapid trigger, DKS, SOCD, and SpeedTap
- local screen pages for keyboard settings
- local modification of active `ProfilePackage` fields and user-writable
  `DeviceSettings`
- current transport mode and basic diagnostics

Offline-degraded behavior:

- Agent Control is unavailable
- Codex/Claude state is not interpreted on the device
- permission approval is unavailable
- workspace/profile library management remains PC-side

## Configuration Model

The product separates reusable PC presets from explicit device resources.

```text
PC Profile Library
  reusable DeviceProfiles
  imported/exported profiles
  software-side WorkspacePresets

Device resources
  DeviceSettings
  DeviceProfileStore with five user ProfilePackage slots
  hidden factory-default ProfilePackage
  ScreenConfig
  LightingConfig
  CalibrationData
  DeviceState runtime facts
```

PC software may write a `ProfilePackage` to a device slot and may read slot
packages back into the PC library. The device screen can switch profiles and
edit allowed fields of the current active profile or `DeviceSettings`, but it
does not manage the full PC profile library.

## Control and Data Paths

```text
Input realtime path:
  H417 key scan + CH585 SPI ingest -> V3F keyboard engine
    -> RuntimeIntent -> V5F report adaptation -> USB/CH585 wireless output

PC control path:
  PC software -> USB Vendor HID -> V5F device protocol -> resource managers

Runtime config path:
  DeviceSettings + ProfilePackage -> V5F compiler -> RuntimeTable -> V3F SRAM

Local config path:
  device screen UI -> V5F resource manager -> external Flash -> RuntimeTable

Wireless path:
  V3F RuntimeIntent -> V5F report adaptation -> CH585 wireless path -> BLE/2.4G
```

## MVP Target

The first implementation milestone is not just "keyboard can type". That is
already expected to be manageable.

MVP target:

```text
PC <-> MCU Vendor HID control plane
DeviceSettings and ProfilePackage slot read/write
local screen edit of the active profile or DeviceSettings
external Flash persistence
V5F-to-V3F RuntimeTable update
V3F input behavior affected by config
```

MVP acceptance criteria:

- USB composite enumerates with Keyboard HID and Vendor HID.
- PC software can send `HELLO` and read capabilities.
- PC software can read `DeviceSettings` and device slot metadata.
- PC software can write one `ProfilePackage` slot or one real
  `DeviceSettings` field.
- Device screen can edit the same setting locally.
- PC software can reconnect and read the device-side edit.
- Config is persisted across reset.
- V5F sends compiled RuntimeTable to V3F.
- V3F RuntimeIntent output changes according to the updated RuntimeTable.

## Out of Scope for MVP

- LAN/Ethernet product path
- device-side WebSocket
- cloud OTA
- full wireless feature completion
- complete on-device keymap editor
- full firmware signing and anti-rollback implementation
- full Agent Control feature set

The architecture reserves these paths where needed, but MVP implementation
should focus on the control/config communication loop.

## Architecture Documents

This architecture is split by engineering boundary:

- `dual_core_architecture.md`
- `keyboard_engine_v3f.md`
- `usb_vendor_hid_architecture.md`
- `device_protocol.md`
- `local_config_and_profile_model.md`
- `h417_ch585_usbhs_architecture.md`
- `display_ui_architecture.md`
- `firmware_update_architecture.md`

