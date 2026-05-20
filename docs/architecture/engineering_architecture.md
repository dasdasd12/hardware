# Hardware Engineering Architecture

This document records the implementation architecture for the hardware
repository. Older pre-design reports may mention Ethernet, lwIP, and a device
side WebSocket client. Those are research notes, not the current engineering
direction.

## Architecture Decision

The keyboard does not use Ethernet as a product communication path. The device
does not connect to the PC bridge with WebSocket.

Keyboard to PC communication is based on keyboard-native transports:

- USB wired mode: USB HID for input, plus Vendor HID or CDC for control,
  configuration, diagnostics, and display events.
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
    Interface 2: Vendor HID or CDC control channel

2.4G wireless:
  Keyboard -> CH585/2.4G radio -> USB dongle -> PC
    keyboard to dongle: private wireless protocol
    dongle to PC: USB HID + vendor control channel

Bluetooth:
  Keyboard -> BLE host
    input: BLE HID
    control: custom BLE GATT service
```

The PC should see a normal keyboard even when the optional control software is
not running.

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
  magnetic switch sampling
  debounce/filtering
  rapid trigger / DKS / SOCD / SpeedTap
  keymap resolution
  report routing

Transport adapters
  USB HID keyboard
  USB Vendor HID or CDC control
  CH585 2.4G channel
  BLE HID and GATT

Board support
  RT-Thread port
  clock, GPIO, USB, DMA, display, storage
  dual-core startup and IPC
```

## Core Responsibilities

V3F should be reserved for hard real-time or near-real-time work when that split
is useful:

- key scan timing
- USB event responsiveness
- low-level watchdog/health monitoring
- narrow IPC messages to V5F

V5F owns feature-level processing:

- magnetic switch algorithms if timing allows
- device protocol command handling
- display UI
- persistent settings
- high-level transport routing

The exact split can evolve after timing measurements, but keyboard input
latency must be protected from display and agent-status features.

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

## Out of Scope

These are not current implementation targets:

- RJ45 Ethernet product path
- lwIP as the primary PC communication layer
- device-side WebSocket client
- device-side TLS or cloud API access
- direct connection from firmware to OpenAI or Anthropic services

Ethernet-related material in `docs/pre_design_report` is retained as research
history only.

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
