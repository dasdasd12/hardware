# Display UI Architecture

The keyboard display is rendered on-device. PC software sends state and
configuration; it does not stream full frames.

## Rendering Direction

```text
PC software:
  sends state, configuration, and projected agent/device summaries

V5F:
  renders UI with LVGL or custom renderer
  handles local setting edits
  persists Device Current Config changes
```

LVGL is the preferred first implementation direction. A custom renderer remains
a fallback if performance or memory constraints require it.

## UI Ownership

V5F owns:

- screen page model
- input handling for local UI controls
- rendering
- local config editing
- state projection from PC/device runtime
- Agent unavailable display

V3F owns:

- keyboard input behavior
- optional compact events for local UI navigation, if routed through V5F

PC software owns:

- profile library
- Agent Control state
- projected summaries for screen display

## Screen Content

Initial screen pages:

- device status
- active Device Current Config summary
- basic keyboard settings
- magnetic setting basics
- transport mode/status
- diagnostics summary
- Agent/status view when PC software is connected

Initial local editable settings:

- active/current config selection
- actuation and rapid trigger basics
- transport mode
- screen brightness/theme/page preference
- safe macro enable/disable

Screen rotation is not a first-stage user setting. If needed, it belongs to
factory/hardware-revision configuration or a hidden debug setting.

## Agent Display

The device does not understand Codex or Claude Code protocols.

Agent-related screen content is generic:

- focused slot label
- session/run status
- notification summary
- permission summary
- action availability
- Agent unavailable state

When PC software is not connected, Agent UI surfaces must show unavailable or
fallback content. They must not execute hidden local actions.

## Local Config Editing

Local edits go through V5F config manager:

```text
screen control -> V5F config edit -> validation -> Device Current Config
  -> persist -> compile runtime table -> V3F install
```

The screen UI should show whether the edit is active, pending, rejected, or
requires PC-side resolution.

## Page and Widget Model

The first implementation can use hard-coded pages. The architecture should
still keep a clean boundary:

```text
Page
  widgets
  data source
  actions

Widget
  type
  label
  value/state
  action binding
```

Future PC profile/screen layout sync can update page definitions or widget
configuration, but live agent state remains runtime data, not profile storage.

## Performance Rules

Display work must not block keyboard input.

Rules:

- no screen rendering work in V3F scan path
- no external Flash write on V3F
- long config writes show progress and avoid blocking report sending
- screen updates are rate-limited when driven by PC state
- diagnostics expose render/update timing if needed

## Storage

Persist on device:

- current screen page preference
- brightness/theme preference
- local settings that are part of Device Current Config
- fallback display state

Do not persist full agent logs or PC profile library on the device.

## MVP Scope

MVP display should prove:

- device status page
- one editable keyboard setting
- local edit persists
- local edit updates V3F runtime behavior
- PC can read the changed Device Current Config
- Agent unavailable state is displayed when PC software is disconnected

