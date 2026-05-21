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
- keymap, layer, and macro execution for offline-capable actions
- HID report generation
- keyboard engine health counters
- runtime config table validation

V3F does not own:

- PC profile library
- full Device Current Config persistence
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
- Device Current Config manager
- external Flash read/write coordination
- display UI
- device protocol state machine
- diagnostics aggregation
- CH585 USB HS coordination
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
5. V5F loads Device Current Config from external Flash.
6. V5F validates and compiles runtime config for V3F.
7. V5F transfers runtime config to V3F through IPC/shared memory.
8. V3F validates generation/checksum and activates runtime table.
9. V5F starts USB and exposes HID/Vendor HID interfaces.

Fallback behavior:

- If V5F config load fails, V5F sends a factory-safe runtime table.
- If V3F rejects runtime config, V5F keeps the previous valid config or factory
  default.
- The device must still expose diagnostics explaining the failure.

## IPC Model

IPC should use bounded messages and shared buffers.

Recommended channels:

```text
V5F -> V3F:
  runtime config install
  active profile/current config generation
  transport mode hint
  command to reset engine counters

V3F -> V5F:
  HID report ready
  key event trace, if enabled
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

- V5F writes runtime config into a staging buffer.
- V5F marks the buffer ready only after checksum is complete.
- V3F validates size, version, and checksum before activation.
- V3F swaps active tables only at a safe scan boundary.
- Runtime tables are immutable while active.
- Shared mutable state uses single-writer ownership.

Forbidden:

- V3F reading external Flash during scan path
- both cores mutating the same config object
- V5F patching an active V3F table in place
- unbounded strings in V3F runtime tables

## HID Report Handoff

V3F generates final HID reports for keyboard behavior. V5F sends those reports
through the USB stack.

```text
V3F:
  scan -> behavior engine -> HID report -> report queue

V5F:
  report queue -> USB HID endpoint
```

The handoff must preserve report ordering and provide drop/overrun diagnostics.

If the USB path is blocked, V3F should continue scanning and update health
counters. V5F decides how to report transport degradation.

## Config Generation

Runtime config uses a generation counter.

```text
Device Current Config revision -> compiled runtime config generation
```

V5F increments generation after a successful Device Current Config change.
V3F reports the active generation in health/status messages.

## Diagnostics

Minimum dual-core diagnostics:

- V3F heartbeat
- V5F heartbeat
- active runtime config generation
- IPC queue depth or overrun count
- HID report queue overrun count
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

