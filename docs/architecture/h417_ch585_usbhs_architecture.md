# H417 to CH585 USB HS Architecture

CH585 is a wireless and auxiliary I/O protocol processor connected to H417
through USB HS.

## Physical Link

```text
CH32H417 USB HS Host <-> CH585 USB Device
```

The exact USB class/interface shape can evolve, but the logical link must carry
framed messages with flow control, status, and update support.

## CH585 Role

CH585 owns:

- BLE HID
- 2.4G private wireless protocol
- pairing and bonding state
- wireless retry/ack/channel handling
- wireless encryption/authentication, when implemented
- wireless connection status
- future auxiliary I/O protocol handling

CH585 does not own:

- full Device Current Config
- PC Profile Library
- Agent Control
- global keyboard policy
- V3F runtime table generation
- device screen UI

## H417 Role

H417 owns product state and coordination:

- V5F config manager
- V3F keyboard engine runtime tables
- device protocol to PC software
- display UI
- diagnostics aggregation
- CH585 high-level commands
- CH585 firmware update bridge

H417 treats CH585 as a capable protocol processor, not a dumb radio. However,
H417 remains the product coordination authority.

## Logical Message Families

H417 to CH585:

```text
WIRELESS_MODE_SET
BLE_ADV_START
BLE_ADV_STOP
PAIRING_START
PAIRING_CANCEL
RADIO_CHANNEL_SET
WIRELESS_CONFIG_SET
HID_REPORT_TX
CH585_FW_UPDATE_BEGIN
CH585_FW_UPDATE_CHUNK
CH585_FW_UPDATE_COMMIT
```

CH585 to H417:

```text
WIRELESS_STATUS
PAIRING_STATUS
BLE_CONNECTION_STATUS
2G4_CONNECTION_STATUS
HID_REPORT_RX_OR_ACK
RADIO_DIAGNOSTICS
CH585_ERROR
CH585_FW_UPDATE_STATUS
```

Message names are logical. Final IDs belong in a protocol header file when
implementation begins.

## Wireless HID Data Flow

For wireless output:

```text
V3F keyboard engine -> HID report -> V5F coordination -> CH585 USB HS -> BLE/2.4G
```

For wireless status:

```text
CH585 -> V5F diagnostics/config manager -> device screen and PC software
```

V3F should not directly own CH585 communication. V5F bridges keyboard engine
output and wireless protocol state.

## Config Boundary

CH585 may persist radio-specific settings:

- pairing/bonding data
- radio channel or hopping state
- BLE identity/bond metadata
- wireless calibration data

H417 persists product-level Device Current Config. If PC software changes a
wireless setting, V5F validates it and then sends the CH585-specific subset to
CH585.

## Firmware Update Bridge

CH585 firmware update is coordinated by H417:

```text
PC software -> Vendor HID -> H417 V5F -> USB HS -> CH585 boot/update mode
```

H417 verifies package target and transfer integrity before instructing CH585 to
commit. CH585 reports update progress and final status back through H417.

CH585 update must not corrupt H417 app firmware or Device Current Config.

## Diagnostics

Minimum CH585 diagnostics:

- CH585 firmware version
- wireless mode
- BLE connection status
- 2.4G connection status
- pairing state
- radio error counters
- USB HS link state
- last CH585 error code
- CH585 update status

These diagnostics should be exposed to PC software through the main device
protocol.

## Future Auxiliary I/O

CH585 may later handle auxiliary I/O if it is useful. Any auxiliary I/O feature
must follow these rules:

- H417 owns product-level configuration.
- CH585 owns only protocol/local hardware handling.
- H417 receives status and errors.
- Features are capability-negotiated.

