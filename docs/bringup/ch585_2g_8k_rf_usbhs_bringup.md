# CH585 2.4G 8K RF + USBHS Bring-up

This note records the first practical path for testing wireless 8K-style
reporting with CH585. This is not BLE. The PC still sees a USBHS receiver.

## Goal

Validate this chain before connecting it to the keyboard scan path:

```text
CH585 RF TX
  -> custom 2.4G short frames at 8000 Hz
  -> CH585 RF RX + USBHS receiver
  -> UART statistics and later USBHS HID
```

BLE HID cannot do true 8K polling because its connection interval is in
`N * 1.25 ms` units and the minimum interval is 7.5 ms. True PC-side 8K
requires USB high-speed microframes or a custom wireless dongle that reports to
the PC over USBHS.

## Current Test Firmware

TX project:

```text
F:\嵌赛\CH585M_RF_TX
```

TX hex:

```text
F:\嵌赛\CH585M_RF_TX\obj\CH585M_RF_TX.hex
```

RX USBHS project:

```text
F:\嵌赛\CH585M_RF_RX_USBHS
```

RX USBHS hex:

```text
F:\嵌赛\CH585M_RF_RX_USBHS\obj\CH585M_RF_RX_USBHS.hex
```

The tracked source snapshot is kept under:

```text
firmware/ch585/applications/rf_keyboard_tx
firmware/ch585/applications/rf_receiver_usbhs
```

Generated `obj/` files and hex files are not tracked.

## What Changed

The TX side now defaults to a key-state short-frame path:

```text
RF_TX_MODE             = RF_TX_MODE_KEYSTATE_8K
RF_TX_TIMER_HZ         = 8000
RF_KEYSTATE_FRAME_LEN  = 13 bytes
RF PHY                 = 2M
ACK                    = disabled
```

Key-state frame:

```text
[0]      magic = 0xB1
[1]      len   = 11
[2:4]    seq, little-endian u16
[4:12]   down_bits[8], 64-key bitmap, bit=1 means key down
[12]     xor over bytes 0..11
```

The TX firmware also reuses the existing H417-to-left-CH585 SPI bridge frame
from the BLE path:

```text
[0]      magic = 0xB8
[1]      type  = 0x31
[2]      version = 1
[3]      H417 seq
[4]      flags
[5:13]   8-byte boot keyboard report from H417
[13]     first right-half key_id, 0xff if none
[14:16]  crc16-ccitt over bytes 0..13
```

The TX converts that 8-byte report back to the right-half `down_bits[8]` using
the fixed bring-up key map, then sends the compact `0xB1` RF frame. If H417 has
not sent a bridge frame yet, the RF frame is all-released. `RF_TX_GetDownBits()`
remains a weak hook for a future direct scanner source.

Stress mode is still available by compiling:

```text
RF_TX_MODE = RF_TX_MODE_STRESS_8K
```

Stress frame:

```text
[0] magic  = 0xA8
[1] len    = 6
[2] seq_lo
[3] seq_hi
[4] 0x5A
[5] 0xA5
[6] low byte of local tick counter
[7] xor over bytes 0..6
```

The RX USBHS side now accepts two frame types:

```text
0xA8 short stress frame
  -> counted only, no keyboard output

0xB1 short key-state frame
  -> converts right-half down_bits[8] to USBHS NKRO keyboard report

0x55 legacy 25-byte keyboard frame
  -> still converted to USBHS HID keyboard report
```

This avoids accidental typing during RF rate testing.

The RX USBHS side also forwards the latest short-frame statistics through the
USBHS custom HID interface:

```text
VID/PID     = 1A86:FE07
interface   = custom HID, usually interface 2
endpoint    = EP3 IN
packet size = 64 bytes
bInterval   = 1 high-speed microframe, 125 us
```

USBHS debug report:

```text
[0]     0xA8
[1]     report type 0x01
[2:4]   latest RF seq, little-endian u16
[4:8]   total valid RF short frames
[8:12]  total seq-derived RF lost frames
[12:16] bad short-frame xor count
[16:20] bad short-frame length count
[20:24] RF CRC error count
[24:28] RF RX interrupt count
[28:32] USBHS custom reports accepted by EP3
[32:36] USBHS custom reports overwritten/dropped before EP3 accepted them
[36:40] legacy 25-byte keyboard frame count
[40:44] RF timeout count
[44]    last RSSI as int8
[45:49] key-state 0xB1 frame count
[49:51] latest key-state seq
[63]    xor over bytes 0..62
```

## Build

Because the workspace path contains Chinese characters, use MounRiver's make
and force Windows `cmd.exe` as the shell. Otherwise MSYS `/usr/bin/sh` may
break paths.

TX:

```powershell
$env:PATH='F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin;' + $env:PATH
$env:SHELL='C:\Windows\System32\cmd.exe'
& 'F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe' -C F:\嵌赛\CH585M_RF_TX\obj all -j4 SHELL=C:\Windows\System32\cmd.exe
```

RX USBHS:

```powershell
$env:PATH='F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin;' + $env:PATH
$env:SHELL='C:\Windows\System32\cmd.exe'
& 'F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe' -C F:\嵌赛\CH585M_RF_RX_USBHS\obj all -j4 SHELL=C:\Windows\System32\cmd.exe
```

## Flashing

Use two CH585 boards:

```text
keyboard-side / transmitter CH585 -> CH585M_RF_TX.hex
USB receiver CH585               -> CH585M_RF_RX_USBHS.hex
```

For the first test, do not connect this to the main H417/ADC path. The point is
only to measure the RF link.

## Observing

The keyboard-side RF TX now uses UART1 for debug so SPI0 can stay connected to
H417:

```text
PA9 -> UART1 TX
PA8 -> UART1 RX
baud = 921600
```

Do not use remapped UART0 on PA14/PA15 for the keyboard-side RF TX while H417
SPI is connected, because PA14/PA15 are SPI0 MOSI/MISO.

On the TX UART, expected once per second:

```text
rf tx gen=8000 done=... seq=... hz=8000 br=... br_crc=... br_magic=... hseq=... first=...
```

`gen` is the number of timer-generated frames per second. `done` is the number
of RF TX-finish callbacks seen in that second. `br` is the number of valid SPI
bridge frames received from H417. `br_crc` and `br_magic` should stay stable.

On the RX UART, expected once per second:

```text
rf8k rx=... ok/s=... lost/s=... total_ok=... total_lost=... bad_xor/s=... bad_len/s=... crc/s=... rssi=... seq=...
```

Important fields:

```text
ok/s       ideally close to 8000
lost/s     should be 0 or very small
bad_xor/s  frame data corruption count
crc/s      RF CRC error count
rssi       last observed RSSI
```

If `ok/s` is far below 8000, the current RF Basic one-way short-frame approach
is not enough and the next step is to test WCH's `RFBound_Start8kHost` /
`RFBound_Start8kDevice` path.

USBHS observation:

```powershell
py -m pip install hidapi
py F:\嵌赛\hardware\tools\ch585\read_rf8k_usbhs.py
```

Expected output in stress mode:

```text
hs/s= 8000.0 seq=12345 rf_ok=... rf_lost=0 crc=0 usb_sent=... usb_drop=... rssi=-40
```

`hs/s` is the number of custom HID reports read by the PC per second. `rf_ok`
and `rf_lost` are measured on the receiver from the 2.4G sequence numbers.
In key-state mode, the custom HID report is intentionally low-rate debug; watch
`key_ok` and `key_seq` instead of expecting one EP3 report for every RF frame.

## Next Steps

1. Flash TX and RX USBHS hexes onto two CH585 boards.
2. Watch RX UART for `ok/s`, `lost/s`, `crc/s`, and `rssi`.
3. Move boards closer and farther apart to see link margin.
4. For the current PCB path, connect H417 to the keyboard-side CH585 RF TX on
   the same left-CH585 SPI bridge pins used by the BLE bridge. Watch TX UART:
   `br` should increase when H417 sends reports, while `br_crc/br_magic` stay
   stable.
5. Keep raw ADC off the 2.4G link. The wireless link should carry final key
state/events only.
6. If Basic RF at 8K is not stable, switch to the official 8K bound mode APIs:

```text
RFBound_Start8kHost()
RFBound_Start8kDevice()
RFRole_PHYUpdate(dev_id, 1)  // 2M PHY
```
