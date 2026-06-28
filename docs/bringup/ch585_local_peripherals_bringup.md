# CH585 Local Peripherals Bring-up

This note records the seven small modules still needed around the key matrix.
It follows the contest report pin split:

- U2 / left CH585: left-half scan, EEPROM, screen/five-way interaction,
  wireless resources.
- U3 / right CH585: right-half scan, EC11 encoder, battery gauge and charging
  status.

The new code is deliberately independent from the active key-scan and SPI
bring-up path. Add each module to a test build only when that step is being
verified.

## 1. I2C Access Layer

File:

```text
firmware/ch585/drivers/ch585_i2c_bus.h
firmware/ch585/drivers/ch585_i2c_bus.c
```

Purpose:

- One small callback wrapper for 7-bit I2C transactions.
- Supports `probe`, `write`, `read`, and combined `write_read`.
- Can be backed by WCH hardware I2C or by the existing PB20/PB21 soft-I2C
  bring-up code.

Board pins from the report:

```text
U2 EEPROM: ROM_SDA PB20, ROM_SCL PB21
U3 Battery: BAT_SDA PB20, BAT_SCL PB21
```

First test should only scan/probe the bus. Do not write EEPROM during this
step.

## 2. EEPROM Driver

File:

```text
firmware/ch585/drivers/hx24lc16.h
firmware/ch585/drivers/hx24lc16.c
```

Device:

```text
HX24LC16BDBVRG, 16 Kbit = 2048 bytes, 16-byte page write
```

Addressing:

```text
addr7 = 0x50 | ((mem_addr >> 8) & 0x07)
wire word address = mem_addr[7:0]
```

Bring-up order:

1. Probe `0x50`.
2. Read byte `0x0000` and a small block from a known unused profile area.
3. Only after confirming the profile map, enable a guarded write/restore test.

Use EEPROM for persistent config on U2: BLE pairing/config cache, profile
metadata, keymap/version stamp. Avoid storing rapidly changing runtime state.

## 3. MAX17048 Fuel Gauge

File:

```text
firmware/ch585/drivers/max17048.h
firmware/ch585/drivers/max17048.c
```

Device:

```text
MAX17048 address = 0x36
VCELL register = 0x02, 78.125 uV/LSB
SOC register = 0x04, 1/256 percent/LSB
```

U3 pins:

```text
BAT_SDA   PB20
BAT_SCL   PB21
BAT_ALERT PB5
```

Bring-up order:

1. Probe `0x36`.
2. Read `VERSION`, `VCELL`, `SOC`, `CONFIG`, `STATUS`.
3. Print voltage in mV and SOC percent on UART.
4. Treat `BAT_ALERT` as an input flag and forward it to H417 sideband status.

Do not call quick-start or POR in normal loops; those are explicit debug
operations only.

## 4. HE3342 Charge Status

No I2C driver is needed here; it is GPIO status on U3:

```text
CHARGE PB6
STDBY  PB7
```

File:

```text
firmware/ch585/drivers/ch585_power_status.h
firmware/ch585/drivers/ch585_power_status.c
```

The exact active polarity should be confirmed on the PCB with USB/battery
states. The firmware should expose a normalized state:

```text
0 = unknown
1 = discharging
2 = charging
3 = standby/full
4 = fault_or_conflict
```

This state is packed into the CH585 sideband frame for H417.

## 5. Central Inputs

File:

```text
firmware/ch585/drivers/ch585_local_inputs.h
firmware/ch585/drivers/ch585_local_inputs.c
```

U2 screen/five-way pins:

```text
SCR_COM   PA7
SCR_UP    PB7
SCR_DOWN  PB6
SCR_RIGHT PB5
SCR_LEFT  PB4
SCR_CHA   PA10
SCR_CHB   PA11
```

`SCR_COM` is the RKJXT common / Push-Com related net. It should not be
treated as an independent center key by itself. In the current UART test,
bit/id 4 means `COM` changed, not "center pressed".

The U2 local peripheral UART test therefore generates debounced five-way
events only for `UP/DOWN/LEFT/RIGHT`. `COM/CHA/CHB` are still printed as raw
levels, and should be interpreted separately after the RKJXT push/encoder
behavior is confirmed on hardware.

U3 EC11 pins:

```text
ENC_BUTTON PA7
ENC_CHA    PA10
ENC_CHB    PA11
```

The module provides:

- Debounced button logic.
- EC11 quadrature decoding with configurable `steps_per_detent`.
- Five-way/screen input event generation.

First test should print events on the local CH585 UART before sending them to
H417.

## 6. CH585 to H417 Sideband Frame

File:

```text
firmware/ch585/applications/ch585_sideband.h
firmware/ch585/applications/ch585_sideband.c
```

Purpose:

- Keep key-state traffic separate from low-rate local peripheral traffic.
- Carry battery, charge, EEPROM status, encoder delta and five-way events.

Frame summary:

```text
magic/version/type/source/seq/flags/event_count/alert_flags
vbat_mv/soc_q8_percent/input_mask/charge_state
events[type,id,value,flags]...
crc8
```

This frame should ride in a low-rate slot of the existing CH585-H417 command
response flow. It should not replace the high-rate key short frame.

## 7. H417 Consumer

H417 should decode `ch585_sideband` from both CH585s and merge it with the
global keyboard state:

- U2 sideband: EEPROM/profile status, screen/five-way events, wireless status.
- U3 sideband: battery, charge, EC11 events.
- H417 policy: SOCD, combos, macros, profile selection, USB HID/Vendor HID.

Suggested H417 bring-up order:

1. Add a parser-only test that consumes a canned sideband frame.
2. Print decoded fields on USBFS CDC.
3. Poll U2/U3 sideband at low rate, for example 50-200 Hz.
4. Map EC11 and five-way events to temporary HID keys for visible testing.
5. Move final behavior into the profile/config layer.

## Integration Notes

- Keep ADC/key scan fast path independent from these low-rate modules.
- EEPROM writes must be explicit and rate-limited.
- Battery/charge can update at low rate; MAX17048 VCELL updates about every
  250 ms in active mode.
- Central input events should be queued so H417 does not miss encoder steps
  while it is polling key frames.

## Local UART Test Firmware

The CH585 SPI slave project now has a local-peripheral-only test mode:

```text
firmware/ch585/applications/spi_slave_test/ch585_local_periph_test.c
firmware/ch585/applications/spi_slave_test/ch585_soft_i2c.c
```

When `CH585_LOCAL_PERIPH_TEST_MODE=1`, the firmware does not start SPI, ADC
scan or BLE. It only prints local peripheral state on UART.

Default debug UART:

```text
UART1 TX PA9
UART1 RX PA8
baud 115200
```

Build U3 / right CH585 test:

```powershell
make -C firmware/ch585 APP=spi_slave_test DEFS_EXTRA="-DCH585_LOCAL_PERIPH_TEST_MODE=1 -DCH585_LOCAL_PERIPH_TEST_RIGHT=1 -DCH585_LOCAL_PERIPH_TEST_UART_PORT=1"
```

Saved output:

```text
firmware/ch585/build/spi_slave_test/ch585_spi_slave_test.hex
```

Expected U3 banner:

```text
CH585 local peripheral test start
side=right/U3 uart=UART1 baud=115200 i2c=PB20/PB21
U3 MAX17048 probe addr=0x36 rc=...
U3 pins: ENC_BUTTON=PA7 ENC_CHA=PA10 ENC_CHB=PA11 BAT_ALERT=PB5 CHARGE=PB6 STDBY=PB7
```

Expected U3 periodic line:

```text
U3 ST ab=... btn=... alert=... chg=... stdby=... vbat=...mV soc=... charge_state=...
```

Encoder changes print as events:

```text
U3 EC11 event type=1 id=0 value=1
U3 EC11 event type=2 id=0 value=-1
```

Build U2 / left CH585 test:

```powershell
make -C firmware/ch585 APP=spi_slave_test DEFS_EXTRA="-DCH585_LOCAL_PERIPH_TEST_MODE=1 -DCH585_LOCAL_PERIPH_TEST_RIGHT=0 -DCH585_LOCAL_PERIPH_TEST_UART_PORT=1"
```

Saved output:

```text
firmware/ch585/build/spi_slave_test/ch585_spi_slave_test.hex
```

Expected U2 banner:

```text
CH585 local peripheral test start
side=left/U2 uart=UART1 baud=115200 i2c=PB20/PB21
U2 EEPROM HX24LC16 probe addr=0x50 rc=...
U2 input pins: COM=PA7 UP=PB7 DOWN=PB6 RIGHT=PB5 LEFT=PB4 CHA=PA10 CHB=PA11
U2 fiveway event mask: UP/DOWN/LEFT/RIGHT only; COM/CHA/CHB are raw-level debug
```

Expected U2 periodic line:

```text
U2 ST raw=0x.. active=0x.. levels COM=... UP=... DOWN=... LEFT=... RIGHT=... A=... B=...
```

Five-way changes print as events:

```text
U2 FIVEWAY event type=3 id=... value=1
U2 FIVEWAY event type=3 id=... value=-1
```

The first EEPROM test only probes and reads. It does not write EEPROM.

## 9. Current Validation Status

Validated on the keyboard PCB:

- U3 / right CH585 MAX17048 I2C probe and read work. UART showed plausible
  battery voltage and SOC values.
- U3 / right CH585 EC11 rotation and push button both produce UART events.
- U2 / left CH585 HX24LC16 EEPROM probe and read work. Blank EEPROM read as
  `0xff`.
- U2 / left CH585 `SCR_COM` is confirmed to be a common / Push-Com related
  net, not an independent center key signal.
- U2 / left CH585 five-way event generation now masks out `COM/CHA/CHB`.
  `UP` was re-tested after the change and generated only `id=0` press/release
  events, while `COM` still appeared only in raw-level debug.

Still worth checking before marking the five-way block complete:

- Press `DOWN/LEFT/RIGHT` once each and confirm they report only `id=1/2/3`.
- Decide how the RKJXT center press should be represented after observing the
  raw `COM/CHA/CHB` behavior during a center press.
