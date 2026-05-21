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
        | USB HS internal board link
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
  keymap/layer/macro
  HID report generation

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
  future auxiliary I/O
```

V3F owns the deterministic keyboard path. V5F owns product features, state
coordination, and communication with PC software.

## Required Offline Behavior

The keyboard must remain a good keyboard when PC software is not running.

Offline-capable behavior:

- standard USB HID keyboard input
- current keymap, layers, macros, and magnetic settings
- rapid trigger, DKS, SOCD, and SpeedTap
- local screen pages for keyboard settings
- local modification of Device Current Config
- current transport mode and basic diagnostics

Offline-degraded behavior:

- Agent Control is unavailable
- Codex/Claude state is not interpreted on the device
- permission approval is unavailable
- workspace/profile library management remains PC-side

## Configuration Model

The product separates reusable PC presets from device runtime state.

```text
PC Profile Library
  preset profiles
  user custom profiles
  imported/exported profiles

Device Current Config
  current active device settings
  persisted on device
  editable from device screen
  readable and writable from PC software
```

PC software may write a profile to the device. PC software may also read Device
Current Config and save it as a new profile. The device screen edits current
device settings, not the full PC profile library.

## Control and Data Paths

```text
Input realtime path:
  sensors -> V3F keyboard engine -> HID report -> V5F USB stack -> PC

PC control path:
  PC software -> USB Vendor HID -> V5F device protocol -> config manager

Runtime config path:
  external Flash -> V5F config manager -> compiled runtime table -> V3F SRAM

Local config path:
  device screen UI -> V5F config manager -> external Flash -> V3F runtime table

Wireless path:
  V3F HID/key state -> V5F coordination -> CH585 over USB HS -> BLE/2.4G
```

## MVP Target

The first implementation milestone is not just "keyboard can type". That is
already expected to be manageable.

MVP target:

```text
PC <-> MCU Vendor HID control plane
Device Current Config read/write
local screen edit of the same config
external Flash persistence
V5F-to-V3F runtime config update
V3F input behavior affected by config
```

MVP acceptance criteria:

- USB composite enumerates with Keyboard HID and Vendor HID.
- PC software can send `HELLO` and read capabilities.
- PC software can read Device Current Config.
- PC software can write one real setting into Device Current Config.
- Device screen can edit the same setting locally.
- PC software can reconnect and read the device-side edit.
- Config is persisted across reset.
- V5F sends compiled runtime config to V3F.
- V3F output changes according to the updated runtime config.

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

