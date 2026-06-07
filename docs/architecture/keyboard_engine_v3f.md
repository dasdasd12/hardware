# V3F Keyboard Engine

The V3F core owns deterministic realtime keyboard behavior. It runs a
bare-metal loop and produces bounded `RuntimeIntent` output for V5F to adapt and
send.

V3F does not generate final USB or wireless host reports. V5F owns report
adaptation, USB endpoints, and CH585 wireless coordination.

## Runtime Model

```text
bare-metal loop + interrupts + static RuntimeTable + bounded queues
```

Do not run RT-Thread Nano on V3F in the initial architecture. The engine should
prefer bounded memory, predictable loop timing, and explicit state machines.

## Pipeline

```text
H417 key scan
CH585 non-key input over SPI ingest
  -> Control Data Layer
  -> type+mode trigger algorithms
  -> ControlSignal queue
  -> interaction_rules
  -> binding_scopes dispatch
  -> behavior execution
  -> RuntimeIntent queue
  -> V5F report adaptation / device request handling
```

Every stage must have bounded execution time.

## Inputs

Runtime inputs:

- `RuntimeTableBinary`
- control index map
- trigger tables for control `type + mode`
- dispatch tables for `binding_scopes`
- behavior table
- interaction rule table
- macro bytecode
- mutable parameter slots
- calibration-corrected `ControlState` / `ControlEvent`
- `DeviceSettings.report_policy` fields compiled into runtime parameters where
  they affect keyboard algorithm or RuntimeIntent shape

These inputs are provided by V5F as compiled runtime tables in SRAM. V3F does
not parse full `ProfilePackage` source and does not read external Flash in the
scan path.

## Outputs

Primary output:

- `RuntimeIntent` queue

RuntimeIntent families:

- host input intent: keyboard, consumer, mouse, gamepad usage intent
- device request intent: bounded commands allowed from profile behavior
- macro executor scheduling result
- diagnostic/trace events in debug builds

Secondary outputs:

- scan timing statistics
- active runtime generation
- fault and health status
- optional key/control event trace in debug builds

## Per-Control Runtime State

Each control has compact runtime state appropriate to its type:

```text
raw or normalized value
calibrated value
pressed/released/down state
trigger baseline
last signal timestamp
mode-specific state
feature flags
release-to-rearm state
```

Per-control trigger settings belong to profile trigger parameters, not to
binding dispatch. Binding dispatch answers "what happens after this signal";
trigger parameters answer "when does this signal exist".

## Behavior Features

Initial engine features:

- normal analog key trigger
- rapid trigger
- analog output mode
- DKS
- SOCD interaction rule
- combo interaction rule
- binding scope activation
- tap-hold
- profile switch request
- safe offline macros

The feature implementation must use runtime tables, fixed-size arrays, and
bounded queues.

## Profile Processing Ownership

V3F owns offline-capable profile processing:

- trigger algorithm state
- binding scope activation state
- behavior dispatch
- macro scheduling
- interaction rule state
- RuntimeIntent generation

V5F owns:

- `DeviceSettings` persistence
- `ProfilePackage` slot persistence
- validation and compilation
- runtime table installation transaction
- screen UI
- report adaptation and transport sending
- Agent unavailable/fallback presentation

Agent-related bindings do not execute offline as agent actions. A future
software-side agent action may be projected to the screen or Local Core, but it
must not be hidden inside V3F keyboard behavior.

## Runtime Config Install

Runtime config install flow:

1. V5F validates the target `ProfilePackage` and relevant `DeviceSettings`.
2. V5F compiles `RuntimeTableBinary`.
3. V5F writes the table to staging shared memory.
4. V5F sends install request with version, size, generation, and checksum.
5. V3F validates the staging data.
6. V3F freezes dispatch at a safe boundary, neutralizes active outputs through
   RuntimeIntent, and clears engine runtime state.
7. V3F activates the new table at a safe scan boundary.
8. V3F reports success/failure and active generation.

Failed install must not corrupt the previous active table. If installation fails
after freeze, V3F restarts the previous valid table with cleared runtime state
and V5F reports rollback to the previous active slot.

## Timing Requirements

Exact scan and report targets depend on hardware measurement. The architecture
requires:

- scan loop timing is measured
- trigger and dispatch cost is measured
- RuntimeIntent queue latency is measured
- overruns are counted
- feature cost can be disabled and compared
- timing summaries are available to V5F diagnostics

Do not add display, storage, report transport, or protocol parsing work to the
V3F scan path.

## Memory Rules

V3F realtime path should use:

- SRAM runtime tables
- fixed-size arrays
- bounded queues
- no heap allocation in scan path
- no external Flash access in scan path
- no unbounded string processing

External Flash stores persisted resources, but V5F loads and compiles them.

## Error Handling

V3F error classes:

- invalid runtime table version
- runtime table checksum mismatch
- table too large
- scan overrun
- RuntimeIntent queue overrun
- unsupported feature flag
- sensor/control fault
- IPC timeout

Errors are reported to V5F and surfaced through device diagnostics.

## Test Expectations

Engine tests should cover:

- normal actuation/release
- rapid trigger state transitions
- per-control override
- binding scope priority
- overlay activation
- macro scheduling
- SOCD resolution
- combo suppress/release behavior
- RuntimeIntent generation
- runtime table generation swap
- invalid table rejection
- scan/dispatch/queue overrun counters
