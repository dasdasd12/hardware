# Right Half To BLE Bridge Bring-up Plan

This note records the current no-USB keyboard path we want to bring up on the
PCB:

```text
right half Hall sensors
  -> ADS7948 + MUX
  -> right CH585 local magnetic algorithm
  -> right CH585 SPI half-state bitset
  -> H417 profile/debug mapping
  -> left CH585 wireless output command
  -> left CH585 BLE HID
  -> PC
```

The goal is to make the right half behave like a normal keyboard half even when
H417 USB is not connected.

## Current Constraints

- H417 USB is not used for host input in this stage.
- Left CH585 BLE HID output has already been validated with UART debug command
  `tap a`.
- Right CH585 can be debugged from UART1 on `PA9/PA8`.
- The current CH585-to-H417 runtime frame is a short `KEY_STATE` frame carrying
  `down_bits[8]`.
- H417 is the SPI master and should poll the latest committed CH585 frame.
- CH585 owns magnetic ADC processing; H417 should not run the magnetic raw ADC
  algorithm in the final split.

## Bring-up Steps

### Step 1: Right CH585 Local Half Scan

Run right CH585 without H417 first.

Expected output:

```text
KEY_DEBUG / UART lines show raw ADC, filtered ADC, position and down state.
Pressing a physical right-half key changes exactly one bit or one key id.
```

Implementation scope:

- ADS7948 read
- MUX channel select
- settle / discard / oversample / IIR
- per-key threshold or RT decision
- `down_bits[8]` generation

### Step 2: Right CH585 To H417 SPI

Keep H417 on the existing PCB SPI pins:

```text
H417 PB3/SPI1_SCK  -> CH585 PA13/SPI0_SCK
H417 PB5/SPI1_MOSI -> CH585 PA14/SPI0_MOSI
H417 PB4/SPI1_MISO <- CH585 PA15/SPI0_MISO
H417 PD9 GPIO CS   -> right CH585 PA12/SPI0_CS
```

Expected output:

```text
H417 receives valid KEY_STATE frames.
Sequence and CRC stay valid.
H417 edge-detects down_bits changes.
```

### Step 3: Temporary H417 Debug Profile

Before the full profile compiler exists, H417 should use a fixed mapping table:

```text
right bit 0 -> keyboard.a
right bit 1 -> keyboard.b
right bit 2 -> keyboard.c
...
```

This creates `press/release` report intent from right-half bit changes.

### Step 4: H417 To Left CH585 Wireless Command

Add a small H417-to-left-CH585 command path. The temporary command should carry
keyboard HID intent, not raw profile data:

```text
magic
version
seq
report_modifiers
report_keys[6]
crc
```

Left CH585 consumes the command and sends BLE HID. The existing UART debug
interface remains available for status and manual `tap` tests.

### Step 5: PC Observation

When the path works:

```text
press right-half physical key
  -> PC receives the mapped BLE keyboard key
release physical key
  -> PC receives BLE key release
```

If a key is held during reset or profile switch, H417/left CH585 must send an
empty report to avoid stuck keys.

## Protocol Notes

Normal CH585-to-H417 input should stay short:

```text
KEY_STATE:
  magic/type/source/seq/flags
  down_bits[8]
  crc16
```

Analog raw ADC frames are diagnostic or calibration traffic, not the default
runtime path.

H417-to-left-CH585 output is a different direction and should use a separate
frame type. It is allowed to be slower than the right CH585 scan loop because
it only carries the current HID report after H417 profile processing.

## Near-Term To Do

## Current Implementation Snapshot

The first temporary bridge is now implemented but still needs board-level
verification:

- H417:
  - `firmware/h417/v5f_rtthread/applications/ch585_ble_bridge.c`
  - `firmware/h417/v5f_rtthread/applications/ch585_ble_bridge.h`
  - enabled with `APP_ENABLE_CH585_BLE_BRIDGE=1`
  - polls right CH585 key state through the existing SPI scanner
  - maps the right-half PCB `key_id` table to a standard 8-byte keyboard HID
    report
  - sends a 16-byte report frame to the left CH585 through the shared SPI bus
    with left CS `PF2`
- Left CH585 BLE:
  - `F:/嵌赛/CH585M/src/BLE/ble_spi_bridge.c`
  - `F:/嵌赛/CH585M/src/BLE/ble_spi_bridge.h`
  - receives the 16-byte H417 report frame as SPI0 slave
  - validates magic/version/CRC
  - forwards the received 8-byte keyboard report through BLE HID
  - exposes UART `status` counters:
    `bridge: seq=... ok=... crc=... ble_err=...`

Build artifacts:

```text
H417:
  F:/嵌赛/hardware/firmware/h417/build/v5f/rtthread_ch32h417_v5f.hex

left CH585 BLE:
  F:/嵌赛/CH585M/obj/CH585M.hex

right CH585 ADC/SPI:
  F:/嵌赛/hardware/firmware/ch585/build/spi_slave_adc_probe/ch585_spi_slave_adc_probe.hex
```

Temporary H417-to-left-CH585 frame:

```text
byte 0      magic = 0xB8
byte 1      type = 0x31
byte 2      version = 1
byte 3      seq
byte 4      flags
byte 5..12  USB/BLE keyboard report[8]
byte 13     first pressed right-half key_id, 0xff if none
byte 14..15 crc16-ccitt, little-endian in memory
```

Diagnostic `flags`:

```text
bit0: H417 decoded at least one right-half key as down
bit1: H417 has received at least one valid source0/right CH585 frame
bit2: H417 has accumulated source0 SPI/frame errors
bit7: H417 could not read source0 stats
```

Right-half debug key mapping is copied from the right CH585 ADS7948 probe map:
`F12 F11 F10 F9 F8 F7 F6 Backspace Equal Minus 0 9 8 7 Backslash ] [ P O I U Y Enter ' ; L K J H Shift / . , M N B Ctrl Win Fn Alt Space`.
`Fn` is currently treated as a local layer key and does not emit BLE HID.

## Near-Term To Do

1. Burn the three images above to left CH585, right CH585 and H417.
2. Keep the SPI wiring on the PCB mapping:
   - H417 `PB3/SPI1_SCK` -> CH585 `PA13/SPI0_SCK`
   - H417 `PB5/SPI1_MOSI` -> CH585 `PA14/SPI0_MOSI`
   - H417 `PB4/SPI1_MISO` <- CH585 `PA15/SPI0_MISO`
   - H417 `PD9` -> right CH585 `PA12/CS`
   - H417 `PF2` -> left CH585 `PA12/CS`
   - all boards share GND
3. Open left CH585 UART1 debug and run `status`; verify `bridge ok`
   increases and `crc/ble_err` stay stable. The status line also prints
   `flags`, `src_seq`, `first`, and the last 8-byte HID report.
4. Press the currently connected right-half Hall key; verify the PC receives
   the mapped BLE key and the release report clears it.
5. Add a small profile/runtime-table compiler path after the fixed debug map
   is proven.
6. Replace the temporary H417 debug map with compiled `RuntimeTable` lookup.
