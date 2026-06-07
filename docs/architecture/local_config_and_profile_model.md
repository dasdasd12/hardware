# Device Configuration Resources and Profile Store

This document replaces the older `Device Current Config` model.

The current architecture uses explicit configuration resources. The device does
not persist one monolithic current config object.

## Core Terms

```text
DeviceSettings
  Device-level persistent policies loaded before Profile activation.

DeviceProfileStore
  On-device persistent store for ProfilePackage slots.

ProfilePackage
  Canonical exchange and storage container for one DeviceProfile.

ProfileSlot
  One committed user slot in DeviceProfileStore.

FactoryDefaultSlot
  Hidden write-protected fallback profile used when no user slot can be loaded.

ScreenConfig
  Independent screen page/widget/media configuration.

LightingConfig
  Independent lighting effects and rules.

CalibrationData
  Persistent data used by Control Data Layer before Profile Processing.

DeviceState
  Current runtime facts such as runtime_active_slot_id and switch status.
```

## Authority Model

```text
PC Profile Library:
  source of truth for reusable PC-side DeviceProfiles and imports/exports

DeviceProfileStore:
  source of truth for committed on-device ProfilePackage slots

DeviceSettings:
  source of truth for device-level startup, profile policy, report policy,
  guard policy, local UI confirmation, and future power/transport policy

ScreenConfig / LightingConfig / CalibrationData:
  independent device resources, not part of ProfilePackage
```

When the keyboard is disconnected from the PC, it must still read
`DeviceSettings`, load a local profile slot, switch profiles locally, self
calibrate, and report to the host. PC software is a management plane, not part
of the realtime input chain.

## Device Storage

Device storage contains:

- `DeviceSettings`
- five user `ProfilePackage` slots
- hidden factory-default `ProfilePackage`
- slot metadata: valid bit, committed revision, source hash, package CRC
- optional `RuntimeTable` cache inside the package/build artifact area
- `ScreenConfig`
- `LightingConfig`
- `CalibrationData`
- `DeviceState` persisted only where a runtime fact must survive reset
- CH585-owned wireless/pairing state or H417 policy mirror, when defined

## User Flows

Write profile package to a slot:

```text
PC DeviceProfile
  -> validate against device capabilities
  -> encode ProfilePackage
  -> Vendor HID write to target slot
  -> V5F validates package
  -> persist slot atomically
  -> compile/install RuntimeTable when slot becomes active
```

Edit active profile from screen:

```text
screen edit
  -> patch active slot source_profile field
  -> update ProfilePackage revision/hash/CRC
  -> persist active slot
  -> recompile or hot-update mutable parameter slot when allowed
```

Edit DeviceSettings:

```text
PC or screen edit
  -> validate DeviceSettings field
  -> persist DeviceSettings
  -> apply immediately if field is runtime-applicable
```

Switch profile:

```text
switch request from key / screen / PC
  -> read target slot package
  -> validate / compile / install RuntimeTable
  -> update DeviceState.profile_runtime.runtime_active_slot_id on success
  -> rollback to previous active slot on failure
```

Factory reset:

```text
factory_reset command
  -> clear user ProfilePackage slots
  -> reset DeviceSettings
  -> clear user ScreenConfig and LightingConfig
  -> clear user calibration offsets
  -> clear CH585 user wireless state according to command contract
  -> keep hidden factory-default slot
```

## Runtime Table Flow

V3F does not parse full profiles and does not read external Flash in the scan
path.

```text
ProfilePackage source_profile
  -> V5F validation and compile
  -> RuntimeTableBinary
  -> V3F staging buffer
  -> V3F activation at safe boundary
  -> DeviceState active runtime generation
```

V3F receives compact immutable runtime tables and mutable parameter slots
explicitly designed for bounded hot updates.

## Revision and Conflict Model

Each persistent resource carries its own revision or generation.

```text
ProfilePackage.identity.revision
DeviceSettings.revision
ScreenConfig.revision
LightingConfig.revision
CalibrationData.revision
RuntimeTable.generation
```

Conflict cases:

- PC has a stale slot mirror and writes based on an old package revision.
- Screen edits the active slot after PC last read it.
- PC writes a target slot while local profile switch or profile edit is pending.

Default resolution:

- Device rejects stale writes unless the command explicitly requests replace.
- PC can merge only when changed resources and fields do not overlap.
- Device never silently overwrites the PC profile library.
- Screen edits persist only to the current active slot unless the UI explicitly
  selects another target resource.

## Local Screen Edit Scope

Initial local screen edits may include:

- profile switch among local slots
- startup preferred slot when user chooses "use this on boot"
- actuation and rapid trigger basics on the active profile
- profile-local mutable parameters supported by RuntimeTable slots
- self calibration and user calibration offset adjustment
- user-writable `DeviceSettings`

Not part of this resource:

- full PC profile library management
- software-only workspace presets
- agent binding authoring
- screen media/template authoring, until `ScreenConfig` editing is specified
- lighting rule authoring, until `LightingConfig` editing is specified

## Agent Binding Offline Behavior

Agent bindings are software-side product bindings, not firmware profile
bindings.

Offline behavior:

- normal keyboard behavior keeps working
- profile switching keeps working
- safe local macros keep working
- screen shows Agent unavailable when relevant
- firmware does not approve permissions or run shell/file/agent commands

## Validation Rules

V5F must validate before persistent commit or runtime install:

- resource type and schema version
- package/container size and section CRC
- canonical source hash
- hardware/control map compatibility
- supported profile features and behavior kinds
- magnetic parameter ranges
- macro bounds
- RuntimeTable compile success
- target slot and transaction state

Commit success and runtime activation are separate facts and must be reported
clearly.

## MVP Scope

MVP should support:

- read/write `DeviceSettings` summary
- read slot metadata
- write one `ProfilePackage` slot
- switch among local slots
- edit one active-profile mutable parameter from screen
- persist across reset
- read back device-side edits from PC
- compile and install a RuntimeTable that changes V3F behavior
