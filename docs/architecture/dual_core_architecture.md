# Dual-Core Architecture

CH32H417 firmware is organized as an AMP system with explicit ownership between
V3F and V5F.

## Core Assignment

```text
V3F:
  Keyboard Engine Core
  bare-metal realtime loop

V5F:
  Application / UI / Protocol Core
  RT-Thread Standard
```

This split is an architecture decision, not a late optimization. The V3F path
must stay small, deterministic, and testable.

## V3F Responsibilities

V3F owns the realtime keyboard engine:

- magnetic sensor scan scheduling
- sensor normalization, filtering, and debounce
- calibration application
- per-key analog configuration
- actuation/release and rapid trigger
- DKS, SOCD, and SpeedTap
- binding scope, behavior, interaction rule, and macro execution for
  offline-capable actions
- RuntimeIntent generation
- keyboard engine health counters
- RuntimeTable validation

V3F does not own:

- PC profile library
- `DeviceSettings` / `ProfilePackage` / ScreenConfig / LightingConfig
  persistence
- USB Vendor HID control protocol
- Agent Control
- screen UI rendering
- external Flash filesystem policy

## V5F Responsibilities

V5F owns application and communication behavior:

- RT-Thread runtime
- USB device stack
- standard HID report sending
- Vendor HID control channel
- optional CDC debug channel
- explicit device resource managers: `DeviceSettings`, `DeviceProfileStore`,
  `ScreenConfig`, `LightingConfig`, `CalibrationData`, and `DeviceState`
- external Flash read/write coordination
- display UI
- device protocol state machine
- diagnostics aggregation
- CH585 coordination
- firmware update orchestration

V5F sends compiled runtime tables to V3F. It must not place policy-heavy
decision work in the V3F realtime loop unless that work is needed for offline
keyboard behavior.

## Startup Sequence

Target startup sequence:

1. V3F starts first from reset.
2. V3F initializes clocks needed for safe bring-up.
3. V3F wakes V5F.
4. V5F starts RT-Thread and initializes application services.
5. V5F loads `DeviceSettings` and selects the startup profile slot.
6. V5F loads the target `ProfilePackage` from `DeviceProfileStore`.
7. V5F validates and compiles `RuntimeTableBinary` for V3F.
8. V5F transfers RuntimeTable to V3F through IPC/shared memory.
9. V3F validates generation/checksum and activates runtime table.
10. V5F starts USB and exposes HID/Vendor HID interfaces.

Fallback behavior:

- If startup profile load fails, V5F uses the hidden factory-default slot.
- If runtime profile switch install fails, V5F rolls back to the previous active
  slot.
- If V3F rejects RuntimeTable during startup, V5F falls back to the
  factory-default slot.
- The device must still expose diagnostics explaining the failure.

## IPC Model

IPC should use bounded messages and shared buffers.

Recommended channels:

```text
V5F -> V3F:
  RuntimeTable install
  active profile slot / runtime generation
  compiled report policy parameters
  command to reset engine counters

V3F -> V5F:
  RuntimeIntent queue notification
  control/key event trace, if enabled
  keyboard engine health
  config activation result
  fault/error report
```

IPC messages must include:

- message type
- payload length
- generation or sequence
- checksum for config payloads
- status/error code for responses

## Shared Memory Rules

Use shared SRAM for IPC buffers and runtime tables. Do not rely on external
Flash or SDRAM in the V3F realtime loop.

Rules:

- V5F writes RuntimeTable into a staging buffer.
- V5F marks the buffer ready only after checksum is complete.
- V3F validates size, version, and checksum before activation.
- V3F swaps active tables only at a safe scan boundary.
- Runtime tables are immutable while active.
- Shared mutable state uses single-writer ownership.

Forbidden:

- V3F reading external Flash during scan path
- both cores mutating the same config object
- V5F patching an active V3F table in place except through declared mutable
  parameter slots
- unbounded strings in V3F runtime tables

## RuntimeIntent Handoff

V3F generates `RuntimeIntent` for keyboard behavior. V5F adapts those intents to
USB HID, CH585 wireless output, or allowed device requests.

```text
V3F:
  Control Data -> behavior engine -> RuntimeIntent -> intent queue

V5F:
  intent queue -> report adaptation -> USB HID endpoint / CH585 wireless path
```

The handoff must preserve intent ordering and provide drop/overrun diagnostics.

If the USB path is blocked, V3F should continue scanning and update health
counters. V5F decides how to report transport degradation.

## Config Generation

RuntimeTable uses a generation counter.

```text
ProfilePackage revision + DeviceSettings report policy -> RuntimeTable generation
```

V5F increments generation after a successful RuntimeTable install transaction.
V3F reports the active generation in health/status messages.

## Diagnostics

Minimum dual-core diagnostics:

- V3F heartbeat
- V5F heartbeat
- active runtime config generation
- IPC queue depth or overrun count
- RuntimeIntent queue overrun count
- last V3F error code
- last V5F config install error
- scan loop timing summary

## Implementation Boundary

The current repository already has:

- V5F RT-Thread port
- V3F wake-up sample
- dual-core flash script

Implementation should evolve from those pieces without changing this ownership
model.

