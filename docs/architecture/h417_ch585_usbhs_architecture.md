# H417 to CH585 Link Architecture

This file keeps its historical filename, but the current architecture no longer
treats USB HS as the required H417-CH585 runtime input link.

The current HTML architecture is authoritative:

- H417 directly handles key scan.
- CH585 handles non-key hardware input, wireless state, and external modules.
- Non-key input that participates in keyboard behavior is converted by CH585
  into the unified control data contract and reported to H417 through the SPI
  ingest path.
- CH585 internal scan architecture, filtering, module enumeration, and private
  extension-module protocols are not defined in this document.

USB HS may be reconsidered later for a separate high-bandwidth or firmware
update path, but it is not the current Control Data Layer ingest contract.

## Physical and Logical Links

Current required link:

```text
CH585 -> H417:
  SPI ingest frames for unified non-key control data

H417 -> CH585:
  bounded control/config commands required for wireless policy and diagnostics
  exact transport for these commands remains part of the CH585/H417 protocol
  spec, not Profile schema
```

Possible future or optional link:

```text
USB HS:
  reserved for future high-bandwidth diagnostics, firmware update, or alternate
  CH585 control transport if hardware and firmware choose to use it
```

## CH585 Role

CH585 owns:

- BLE HID and 2.4G private wireless protocol
- pairing and bonding state
- wireless retry/ack/channel handling
- wireless encryption/authentication, when implemented
- wireless connection status
- joystick, encoder, and other non-key local input handling
- external module ingress when modules are attached to CH585 USB or other
  extension ports
- conversion of non-key/module input into unified source/control/event reports
  before H417 consumes it

CH585 does not own:

- `DeviceSettings`
- `ProfilePackage` source semantics
- PC Profile Library
- Agent Control
- global keyboard behavior policy
- V3F RuntimeTable generation
- device screen UI

## H417 Role

H417 owns product state and coordination:

- V5F resource managers
- V3F keyboard engine RuntimeTables
- device protocol to PC software
- display UI
- diagnostics aggregation
- CH585 high-level policy commands
- factory reset coordination across H417 and CH585 resources

H417 treats CH585 as a capable protocol processor, not a dumb radio. However,
H417 remains the product coordination authority.

## SPI Ingest Boundary

CH585-to-H417 non-key input frames must enter the same Control Data Layer as
H417 key scan.

```text
CH585 local input or extension module
  -> CH585 source adapter
  -> unified source_index / control_index / data_kind / event_code
  -> SPI ingest
  -> H417 Control Data Layer
  -> V3F Profile Processing
```

Rules:

- SPI v1 carries unified `control_index`, not a CH585-private channel number.
- Unknown source, unknown control, or type mismatch cannot enter Profile
  Processing.
- CH585 raw calibration is applied before SPI where CH585 owns the raw hardware.
- H417/V3F consumes normalized `ControlState` and `ControlEvent` facts.
- Profile never defines CH585 physical channels or extension-module private
  protocols.

## Logical Message Families

CH585 to H417:

```text
CONTROL_STATE_FRAME
CONTROL_EVENT_FRAME
SOURCE_STATUS
WIRELESS_STATUS
PAIRING_STATUS
BLE_CONNECTION_STATUS
2G4_CONNECTION_STATUS
RADIO_DIAGNOSTICS
CH585_ERROR
CH585_FW_UPDATE_STATUS
```

H417 to CH585:

```text
WIRELESS_MODE_SET
BLE_ADV_START
BLE_ADV_STOP
PAIRING_START
PAIRING_CANCEL
RADIO_CHANNEL_SET
WIRELESS_CONFIG_SET
WIRELESS_RATE_SET
CH585_DIAGNOSTIC_QUERY
CH585_FW_UPDATE_BEGIN
CH585_FW_UPDATE_CHUNK
CH585_FW_UPDATE_COMMIT
```

Message names are logical. Final IDs and frame layout belong in the CH585/H417
protocol header and the Runtime Contract SPI ingest specification.

## Wireless Output Boundary

V3F does not directly talk to CH585.

```text
V3F RuntimeIntent
  -> V5F report adaptation
  -> CH585 wireless output path
  -> BLE / 2.4G host
```

CH585 owns wireless host details. H417 owns product policy and decides what
profile/report policy is active. Wireless host records, pairing state, and
current connection facts are not stored in `ProfilePackage`.

## Config Boundary

CH585 may persist radio-specific settings:

- pairing/bonding data
- radio channel or hopping state
- BLE identity/bond metadata
- wireless calibration data

H417 persists product-level resources such as `DeviceSettings` and
`ProfilePackage` slots. If PC software changes a wireless policy, V5F validates
the system-level setting and sends the CH585-specific subset to CH585.

## Firmware Update Bridge

CH585 firmware update is coordinated by H417:

```text
PC software -> Vendor HID -> H417 V5F -> CH585 update transport
```

The exact H417-to-CH585 update transport is not fixed by this document. If USB
HS is later used for update mode, it must remain separate from the SPI control
data ingest contract.

H417 verifies package target and transfer integrity before instructing CH585 to
commit. CH585 reports update progress and final status back through H417.

CH585 update must not corrupt H417 app firmware, `DeviceSettings`,
`ProfilePackage` slots, or user calibration data.

## Diagnostics

Minimum CH585 diagnostics:

- CH585 firmware version
- SPI ingest health
- wireless mode
- BLE connection status
- 2.4G connection status
- pairing state
- radio error counters
- extension-module source status
- last CH585 error code
- CH585 update status

These diagnostics should be exposed to PC software through the main device
protocol and to Screen through compact local status pages.
