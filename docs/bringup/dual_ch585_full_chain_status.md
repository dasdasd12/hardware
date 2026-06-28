# Dual CH585 Full Chain Bring-up Status

Last updated: 2026-06-26

## Verified Chain

The current PCB full-chain path has been verified at the log/debug level:

```text
Left ADS7948 + MUX -> left CH585 key judgment -> SPI short frame source0
Right ADS7948 + MUX -> right CH585 key judgment -> SPI short frame source1
Both CH585 share H417 SPI1 SCK/MOSI/MISO, with separate GPIO CS
H417 merges source0/source1 raw key state
H417 USBFS CDC prints S0/S1 status
```

Verified H417 USBFS log:

```text
SS hb=17 ok=18 fetch=0 mag=0 ver=0 src=0 crc=0 seq=0 s1ok=18
S0 cnt=0 ids=255,255,255,255 ...
S1 cnt=0 ids=255,255,255,255 ...
```

Simultaneous left/right key press was also verified:

```text
SS hb=57 ok=58 fetch=0 mag=0 ver=0 src=0 crc=0 seq=0 s1ok=58
S0 cnt=1 ids=6,255,255,255 ...
S1 cnt=1 ids=19,255,255,255 ...
```

This means both CH585 sources are being pulled by H417, both CRC paths are clean, and shared MISO no longer blocks the right CH585.

## Current Firmware Images

Flash these three images for the current dual-CH585 test:

```text
Left CH585:
F:\嵌赛\hardware\firmware\ch585\build\spi_slave_test\ch585_spi_slave_test.hex

Right CH585:
F:\嵌赛\hardware\firmware\ch585\build\spi_slave_test\ch585_spi_slave_test.hex

H417 V5F:
F:\嵌赛\hardware\firmware\h417\build\V5F\rtthread_ch32h417_V5F.hex
```

Important compile-time meaning:

```text
Left CH585:
CH585_SCAN_SOURCE_ID=0
CH585_ADC_PROBE_RIGHT_HALF=0
CH585_SPI0_MISO_STRONG_DRIVE=0

Right CH585:
CH585_SCAN_SOURCE_ID=1
CH585_ADC_PROBE_RIGHT_HALF=1
CH585_SPI0_MISO_STRONG_DRIVE=0

H417:
APP_CH585_SPI_REAL_SOURCE1=1
APP_CH585_SPI_FAKE_SOURCE1=0
APP_ENABLE_USB2_FS_CDC=1
```

## SPI Wiring

The PCB wiring follows the contest report:

```text
H417 PB3 / SPI1_SCK  -> both CH585 PA13 / SPI0_SCK
H417 PB5 / SPI1_MOSI -> both CH585 PA14 / SPI0_MOSI
H417 PB4 / SPI1_MISO <- both CH585 PA15 / SPI0_MISO
H417 PF2             -> left CH585 PA12 / SPI0_CS
H417 PD9             -> right CH585 PA12 / SPI0_CS
GND                  -> common ground
```

The H417 dual-source test uses:

```text
source0 = left CH585, CS = PF2
source1 = right CH585, CS = PD9
```

## MISO Bus Issue Found

An important issue was found during right-side bring-up:

```text
CH585_SPI0_MISO_STRONG_DRIVE=1
```

is not safe when two CH585 devices share one H417 MISO line. With the old strong-drive build, the right CH585 readback was:

```text
SH head=00 00 00 00 00 00 00 00
```

After both CH585 images were rebuilt with:

```text
CH585_SPI0_MISO_STRONG_DRIVE=0
```

the right-side physical SPI pattern test succeeded:

```text
SH head=a5 5a 3c c3 11 22 33 44 tail=55 66 77 88
```

Then the right-side runtime frame succeeded:

```text
SH head=d7 11 01 ...
```

Conclusion: all shared-MISO CH585 runtime images must use the tri-state/non-strong-drive MISO configuration unless the hardware is changed to isolate the MISO lines.

## Current SPI Speed

Current validated H417 SPI speed:

```text
SP sck=16000.0
```

That is 16 MHz. It is stable in the current dual-CH585 short-frame debug path.

## Current Frame Model

The current H417/CH585 path uses the short key-state frame:

```text
magic     = 0xD7
type      = 0x11 key state
source_id = 0 left, 1 right
seq
ack_seq
flags
down_bits[8]
crc16
```

CH585 does local ADC/MUX scan, filtering/calibration defaults, and key down judgment. H417 currently treats the `down_bits` result as:

```text
released -> raw 1000
pressed  -> raw 3000
```

This is enough for binary key bring-up and routing, but not yet a final analog-position reporting path.

## Interpreting USBFS Logs

Useful H417 COM5 lines:

```text
SS hb=... ok=... crc=... seq=... s1ok=...
```

`ok` is source0/left accepted frames. `s1ok` is source1/right accepted frames.

```text
S0 cnt=... ids=...
S1 cnt=... ids=...
```

`S0` is left CH585 pressed physical slot ids. `S1` is right CH585 pressed physical slot ids.

```text
SH head=d7 11 01 ...
```

The latest captured source0/source1 raw SPI frame head. For source1/right runtime, the third byte should be `01`.

## Next Work

1. Keep `tristate` CH585 images as the default for shared-MISO PCB testing.
2. Confirm a wider key sweep with both halves connected, especially edge keys and Space/Enter/Shift/Ctrl positions.
3. Map physical slot ids to final global key ids and HID usage codes.
4. Connect H417 merged 128-key state to the chosen report path:
   - short-term: USBFS debug or temporary HID
   - later: USBHS HID / Vendor HID
   - wireless path: CH585 2.4G receiver + USBHS
5. Decide whether the product path needs only `down_bits`, or whether CH585 should also expose position/config/debug frames for calibration tools.
