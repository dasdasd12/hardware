# Local Config and Profile Model

The product separates PC-side reusable profiles from the keyboard's active
runtime configuration.

## Core Terms

```text
Profile:
  PC-side reusable preset or user custom configuration.

Device Current Config:
  The configuration currently stored and running on the keyboard.

ConfigRevision:
  Monotonic revision of Device Current Config on the keyboard.

ConfigChangeRecord:
  A record describing a field-level change and its source.
```

## Authority Model

```text
PC Profile Library:
  source of truth for saved profiles and presets

Keyboard Device Current Config:
  source of truth for what the device is currently running
```

PC software can write a profile to the device. It can also read Device Current
Config and save it as a new profile. The device screen edits Device Current
Config, not the full PC profile library.

## User Flows

Write profile to device:

```text
PC profile -> validate -> Vendor HID write -> Device Current Config
```

Edit current config from PC:

```text
PC setting edit -> Vendor HID write -> Device Current Config
```

Edit current config from keyboard:

```text
screen UI edit -> V5F config manager -> Device Current Config
```

Save device state as a new profile:

```text
PC reads Device Current Config -> user saves as new Profile
```

## Config Storage

Device storage:

- Device Current Config
- config revision
- change record summary
- compiled/offline subset for keyboard operation
- last valid runtime generation
- factory default fallback

PC storage:

- profile library
- user custom profiles
- workspace bindings
- device current config mirror
- import/export files

## Runtime Table Flow

Device Current Config is not used directly by V3F.

```text
Device Current Config
  -> V5F validation
  -> V5F compile to V3F runtime tables
  -> V3F staging buffer
  -> V3F activation
  -> active runtime generation
```

V3F receives compact, immutable runtime tables. External Flash is not read by
the V3F scan path.

## Config Revision

Every successful Device Current Config commit increments `ConfigRevision`.

Revision changes should record:

- source: PC software or device local UI
- changed field path
- old/new summary, when safe and compact
- timestamp or monotonic counter
- resulting runtime generation

If time is unavailable, use monotonic counters and sync timestamps from PC later.

## Conflict Model

The architecture avoids most conflicts by distinguishing profiles from current
device config.

Conflict cases:

- PC has a stale mirror and writes based on old revision.
- Device screen modified the same field since PC last read.
- PC writes a profile while local device edit is pending.

Default resolution:

- If changed fields do not overlap, PC software may merge.
- If the same field changed on both sides, PC software asks the user.
- Device does not silently overwrite PC profiles.
- Device may reject stale writes unless the command explicitly requests force.

## Local Screen Edit Scope

Initial local screen edits:

- active profile/current config selection
- actuation and rapid trigger basics
- transport mode selection
- screen brightness/theme/page preferences
- safe macro enable/disable
- Agent display focus when PC software is available

Not initial scope:

- full keymap editor
- full macro authoring
- profile library management
- cloud sync

Per-key settings should be added after the basic current config flow is proven.

## Agent Binding Offline Behavior

Agent bindings are not normal keyboard actions when PC software is absent.

Offline behavior:

- show Agent unavailable on screen, if relevant
- do not approve permissions
- do not run shell/file/agent commands
- keep normal keyboard functions working

## Validation Rules

V5F must validate before commit:

- schema version
- config size
- hardware compatibility
- supported features
- magnetic parameter ranges
- key IDs and matrix layout
- macro bounds
- runtime table compile success

Config is committed only after validation and runtime install succeed, unless
the response explicitly reports staged-but-not-active state.

## MVP Scope

MVP should support:

- read Device Current Config summary
- read Device Current Config chunks
- write one or more config fields from PC
- edit the same field from local screen
- persist across reset
- read back device-side edit from PC
- save read config as a new PC profile later in software

