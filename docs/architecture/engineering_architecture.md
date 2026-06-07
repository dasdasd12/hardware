# Hardware Engineering Architecture

This document records the implementation architecture for the hardware
repository. Older pre-design reports may mention Ethernet, lwIP, and a device
side WebSocket client. Those are research notes, not the current engineering
direction.

## Architecture Decision

The keyboard does not use Ethernet as a product communication path. The device
does not connect to the PC bridge with WebSocket.

Keyboard to PC communication is based on keyboard-native transports:

- USB wired mode: USB HID for input, plus Vendor HID for product control,
  configuration, diagnostics, display events, and firmware update. CDC is
  debug-only and can be compiled out.
- 2.4G mode: keyboard to dongle uses a private low-latency wireless protocol;
  dongle to PC exposes USB HID plus a vendor control interface.
- Bluetooth mode: BLE HID for normal input, plus a custom GATT service for
  configuration and low-rate control messages.

WebSocket belongs on the PC side only, between the local bridge/service and UI
or agent-monitor clients.

## System Boundary

Hardware owns the firmware, real-time input path, local display behavior, and
device protocol framing. It does not own Codex, Claude Code, browser UI, or
agent-specific protocol details.

The firmware should expose stable device capabilities to the software side:

- keyboard input reports
- media/system key reports
- device identity and firmware version
- transport and connection status
- keymap and magnetic switch configuration
- macro and rapid-trigger configuration
- display/status event channel
- diagnostics and logs
- firmware update hooks, when implemented

## Physical Transport Model

```text
USB wired:
  CH32H417 -> PC
    Interface 0: standard USB HID keyboard
    Interface 1: consumer/system HID, if needed
    Interface 2: Vendor HID product control channel
    Optional debug-only: CDC ACM, compile-time gated

2.4G wireless:
  Keyboard -> CH585/2.4G radio -> USB dongle -> PC
    keyboard to dongle: private wireless protocol
    dongle to PC: USB HID + vendor control channel

Bluetooth:
  Keyboard -> CH585 BLE -> BLE host
    input: BLE HID
    control: custom BLE GATT service
```

The PC should see a normal keyboard even when the optional control software is
not running.

## Core Assignment

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
  DeviceSettings / DeviceProfileStore / ScreenConfig / LightingConfig managers
  external Flash coordination
  display UI
  diagnostics and firmware update orchestration

CH585 = Wireless / Auxiliary I/O Protocol Processor
  reports non-key control data to H417 through SPI ingest
  BLE HID
  2.4G private protocol
  pairing/retry/encryption/status
  joystick, encoder, and extension-module input handling
```

The split above is the implementation baseline. V3F is not an optional
optimization target; it owns the deterministic keyboard engine.

## Firmware Layering

```text
Application features
  local screen UI
  agent/status display surface
  configuration application

Device protocol
  command router
  binary frame encoder/decoder
  capability/version negotiation
  diagnostics and log events

Input pipeline
  V3F magnetic switch sampling
  V3F debounce/filtering
  V3F rapid trigger / DKS / SOCD / SpeedTap
  V3F binding scope / behavior / macro processing
  V3F RuntimeIntent generation
  V5F report adaptation and USB/wireless sending

Transport adapters
  USB HID keyboard
  USB Vendor HID control
  CDC debug-only channel
  H417-to-CH585 SPI ingest and control channel
  BLE HID and GATT

Board support
  RT-Thread port
  clock, GPIO, USB, DMA, display, storage
  dual-core startup and IPC
```

## Config Model

The product distinguishes PC-side reusable profiles from explicit device
resources.

```text
PC Profile Library:
  reusable DeviceProfiles and imports/exports
  software-side WorkspacePresets

Device resources:
  DeviceSettings
  DeviceProfileStore with five user ProfilePackage slots
  hidden factory-default ProfilePackage
  ScreenConfig / LightingConfig / CalibrationData
  DeviceState runtime facts
```

V5F reads `DeviceSettings`, selects a `ProfilePackage` slot, validates it,
compiles a compact RuntimeTable, and installs that table into V3F SRAM. V3F
does not parse full profiles and does not read external Flash in the scan path.

## Device Protocol Principles

The device protocol is shared with the software repository and must stay
transport-independent.

Required properties:

- versioned messages
- explicit capability discovery
- compact binary framing for device-facing links
- idempotent configuration writes where possible
- bounded payload sizes per transport
- deterministic error codes
- clear separation between input reports and configuration/control messages

Do not encode Codex or Claude Code concepts directly in firmware. Firmware may
display generic agent/session/task/status concepts provided by the PC bridge.

The first device-protocol milestone is Vendor HID hello/capability plus
`DeviceSettings` and `ProfilePackage` slot read/write. OTA, full screen
projection, and wireless transport support are reserved but should not block the
first control-plane milestone.

## Firmware Update Direction

Firmware update is PC-assisted and transport-controlled:

```text
PC software -> USB Vendor HID -> H417 V5F -> update target
```

Initial targets are:

- H417 app firmware
- CH585 firmware through the H417-managed CH585 update transport
- 2.4G dongle firmware through the dongle vendor channel

H417 app update is designed around manifest + hash + A/B slot. Production
signing and anti-rollback are required before release, but do not need to block
the first Vendor HID control-plane work.

## Out of Scope

These are not current implementation targets:

- RJ45 Ethernet product path
- lwIP as the primary PC communication layer
- device-side WebSocket client
- device-side TLS or cloud API access
- direct connection from firmware to OpenAI or Anthropic services

Ethernet-related material in `docs/pre_design_report` is retained as research
history only.

## Detailed Architecture Documents

The implementation architecture is split across:

- `hardware_architecture.md`
- `dual_core_architecture.md`
- `keyboard_engine_v3f.md`
- `usb_vendor_hid_architecture.md`
- `device_protocol.md`
- `local_config_and_profile_model.md`
- `h417_ch585_usbhs_architecture.md`
- `display_ui_architecture.md`
- `firmware_update_architecture.md`

## Development Workflow

Hardware changes that affect the software side must update the shared protocol
contract before implementation is considered complete.

Suggested flow:

1. Codex/GPT-5.5 defines or reviews the protocol and cross-repository boundary.
2. Claude Code implements the firmware or board-level change in this repository.
3. Codex/GPT-5.5 reviews the change for protocol compatibility, timing risk,
   and integration impact.
4. Software side updates its device adapter against the same protocol contract.

Hardware PRs should report:

- changed files
- build command and result
- target board or simulator
- transport tested
- logs or captured reports
- protocol changes, if any
