# Device Protocol

The device protocol is the firmware-facing contract between PC software and
the keyboard. It runs over Vendor HID first, and later over BLE GATT or dongle
vendor channels when those transports exist.

The protocol is transport-independent. Transport frames carry device protocol
messages.

## Design Rules

- Version every protocol message family.
- Keep payloads bounded.
- Support capability negotiation.
- Support chunked transfer for config and firmware update.
- Do not encode Codex or Claude Code provider details in firmware.
- Use compact slot IDs for software-projected agent/session/notification state.
- Keep input HID reports separate from control protocol messages.

## Transport Mapping

Initial transport:

```text
USB Vendor HID
```

Future transports:

```text
BLE GATT custom service
2.4G dongle vendor channel
simulator transport for tests
```

The same logical messages should be usable across transports, subject to
payload-size differences.

## Message Envelope

The exact binary layout will be finalized with endpoint/report constraints. The
logical envelope must include:

```text
protocol_version
message_type
sequence
flags
payload_length
payload
checksum or transport-level integrity
```

Chunked operations must include:

```text
transfer_id
chunk_index
chunk_count or final flag
payload_offset
payload_length
payload_hash for commit
```

## Identity and Capabilities

Mandatory identity fields:

- device family
- hardware revision
- firmware version
- bootloader version, if available
- protocol version
- active config revision
- active runtime config generation

Mandatory capability fields:

- max payload size
- supported message families
- supported config schema versions
- supported runtime table versions
- supported screen widget/state versions
- firmware update targets
- debug CDC availability

## Message Families

Initial message families:

```text
hello
capabilities
current_config
runtime_config_status
screen_state
slot_mapping
diagnostics
firmware_update
error
```

Agent Control is represented only through generic projected state:

```text
agent slot
session slot
run slot
notification slot
permission slot
status text
action availability
```

The firmware does not know what Codex or Claude Code is.

## Current Config Messages

Required current config operations:

```text
GET_CONFIG_REVISION
GET_CURRENT_CONFIG_SUMMARY
READ_CURRENT_CONFIG_BEGIN
READ_CURRENT_CONFIG_CHUNK
READ_CURRENT_CONFIG_END
WRITE_CURRENT_CONFIG_BEGIN
WRITE_CURRENT_CONFIG_CHUNK
WRITE_CURRENT_CONFIG_COMMIT
WRITE_CURRENT_CONFIG_ABORT
CONFIG_CHANGE_RECORD
```

Write commit rules:

- V5F validates schema and device capability compatibility.
- V5F persists Device Current Config to external Flash.
- V5F compiles runtime tables for V3F.
- V3F accepts or rejects runtime table generation.
- Config revision advances only after persistence and V3F activation succeed,
  or the response must clearly report partial status.

## Screen State Messages

Screen state messages carry device-renderable state, not pixel frames.

Examples:

- focused agent/session display label
- Agent unavailable state
- notification summary
- permission summary
- device status
- config edit page state

The device renders this state using LVGL or a custom renderer.

## Slot Mapping Messages

PC software owns long IDs. Device receives bounded slots.

```text
agent_slot_id
session_slot_id
run_slot_id
notification_slot_id
permission_slot_id
slot_generation
```

If the device sends an action for an unknown or stale slot generation, V5F must
return a resync/error response rather than guessing.

## Diagnostics Messages

Diagnostics must expose enough state for PC software and development tooling:

- firmware/protocol versions
- active config revision
- active V3F runtime generation
- V3F heartbeat
- V5F heartbeat
- USB status
- Vendor HID framing errors
- IPC overrun counters
- scan/report timing summary
- last error code

## Firmware Update Messages

Firmware update is a protocol family, not an ad hoc file transfer.

Initial targets:

```text
h417_app
ch585
dongle
```

Required logical operations:

```text
FW_UPDATE_QUERY
FW_UPDATE_BEGIN
FW_UPDATE_CHUNK
FW_UPDATE_VERIFY
FW_UPDATE_COMMIT
FW_UPDATE_ABORT
FW_UPDATE_STATUS
```

H417 app update uses manifest + hash + A/B slot. Signing and anti-rollback are
reserved for production.

## Error Model

Errors must be structured:

```text
code
message family
message type
sequence
recoverable flag
detail code
```

Initial error codes:

- unsupported protocol version
- unsupported message type
- payload too large
- invalid checksum
- invalid chunk sequence
- config validation failed
- config generation rejected by V3F
- stale slot generation
- firmware manifest invalid
- firmware hash mismatch
- transport busy

## MVP Protocol Scope

MVP includes:

- hello/capabilities
- current config read/write
- config revision
- diagnostics summary
- error responses

MVP excludes:

- full OTA implementation
- full screen widget model
- wireless transport support
- complete Agent Control projection

