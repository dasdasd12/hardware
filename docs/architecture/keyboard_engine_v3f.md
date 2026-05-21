# V3F Keyboard Engine

The V3F core owns the deterministic keyboard engine. It runs a bare-metal
realtime loop and produces final HID keyboard reports for V5F to send.

## Runtime Model

```text
Bare-metal loop + interrupts + static runtime tables
```

Do not run RT-Thread Nano on V3F in the initial architecture. The engine should
prefer bounded memory, predictable loop timing, and explicit state machines.

## Pipeline

```text
scan schedule
  -> hall sensor acquisition
  -> normalization/filtering
  -> calibration correction
  -> per-key analog state
  -> debounce/actuation/release
  -> rapid trigger / DKS / SOCD / SpeedTap
  -> keymap/layer/macro
  -> HID report generation
  -> V5F report queue
```

Every stage must have bounded execution time.

## Inputs

Runtime inputs:

- matrix and sensor layout table
- per-key analog config
- calibration table
- keymap table
- layer table
- macro table
- behavior feature flags
- transport mode hints from V5F

These inputs are provided by V5F as compiled runtime tables in SRAM. V3F does
not parse full PC profiles.

## Outputs

Primary output:

- HID keyboard reports

Secondary outputs:

- optional consumer/system reports, if included in runtime table
- key event trace in debug builds
- scan timing statistics
- active config generation
- fault and health status

## Per-Key Analog State

Each physical key should have a compact runtime state:

```text
raw sample
filtered position
calibrated position
pressed/released state
actuation threshold
release threshold
rapid trigger baseline
last event timestamp
feature flags
```

Per-key settings belong to analog key config, not to keymap. Keymap answers
"what does this key do"; analog config answers "when does this key trigger".

## Behavior Features

Initial engine features:

- actuation/release threshold
- rapid trigger
- DKS
- SOCD
- SpeedTap
- layer activation
- macros that are safe to execute offline

The feature implementation must use runtime tables, not dynamic allocation.

## Keymap, Layer, and Macro Ownership

V3F owns offline-capable keyboard behavior:

- keymap resolution
- layer state
- macro sequencing
- HID report composition

V5F owns:

- full Device Current Config
- persistence
- editing UI
- validation and compilation
- Agent Control unavailable/fallback behavior

Agent-related bindings do not execute offline as agent actions. V3F may expose
an unavailable action state or pass a compact event to V5F when PC software is
available.

## Runtime Config Install

Runtime config install flow:

1. V5F validates Device Current Config.
2. V5F compiles compact tables for V3F.
3. V5F writes tables to staging shared memory.
4. V5F sends install request with version, size, generation, and checksum.
5. V3F validates the staging data.
6. V3F activates tables at a safe scan boundary.
7. V3F reports success/failure and active generation.

Failed install must not corrupt the previous active table.

## Timing Requirements

Exact scan and report targets depend on hardware measurement. The architecture
requires:

- scan loop timing is measured
- report generation timing is measured
- overruns are counted
- feature cost can be disabled and compared
- timing summaries are available to V5F diagnostics

Do not add display, storage, or protocol parsing work to V3F scan path.

## Memory Rules

V3F realtime path should use:

- SRAM runtime tables
- fixed-size arrays
- bounded queues
- no heap allocation in scan path
- no external Flash access in scan path
- no unbounded string processing

External Flash stores persisted config, but V5F loads and compiles it.

## Error Handling

V3F error classes:

- invalid runtime table version
- runtime table checksum mismatch
- table too large
- scan overrun
- report queue overrun
- unsupported feature flag
- sensor fault
- IPC timeout

Errors are reported to V5F and surfaced through device diagnostics.

## Test Expectations

Engine tests should cover:

- threshold actuation/release
- rapid trigger state transitions
- per-key override
- layer priority
- macro sequencing
- SOCD resolution
- HID report generation
- config generation swap
- invalid table rejection
- scan/report overrun counters

