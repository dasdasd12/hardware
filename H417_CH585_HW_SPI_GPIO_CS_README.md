# H417 + CH585 hardware SPI CS plan

This note supersedes the earlier idea that "H417 should not use hardware SPI".
The corrected understanding is:

```text
H417 uses hardware SPI for SCK/MOSI/MISO.
CS can be either hardware NSS for one slave, or GPIO pins for multiple slaves.
CH585 uses hardware SPI0 slave, preferably with DMA.
```

The teammate's point was about hardware NSS/CS only. A single hardware SPI
controller normally has one NSS/CS output, which is not enough when one bus has
multiple CH585 slaves. The usual solution is to configure SPI as software-NSS
mode and drive each slave's CS with GPIO.

## Current firmware

The current H417 firmware uses hardware SPI2 for the data bus and PB12 as GPIO
CS. This is already the direction needed for two CH585 devices later.

```text
H417 GPIO CS0  PB12 -> CH585_0 PA12/SCS
H417 SPI2_SCK  PB13 -> CH585_0 PA13/SCK0
H417 SPI2_MOSI PC1  -> CH585_0 PA14/MOSI
H417 SPI2_MISO PC2  <- CH585_0 PA15/MISO

H417 PD7 is reserved as CH585_1 GPIO CS later.
GND common
3V3 common
```

Build-time settings in `rtthread_port/applications/ch585_spi_scan.c`:

```text
APP_CH585_SPI_HW_SPI2_BACKEND = 1
SPI2 NSS mode                 = SPI_NSS_Soft, PB12 is GPIO CS
SPI2 data size                = 8-bit
SPI mode                      = Mode 0, CPOL low, CPHA first edge, MSB first
SPI2 prescaler                = SPI_BaudRatePrescaler_Mode5 with HSRX mode1
Expected SCK at 100 MHz HCLK  = 14.29 MHz
H417 high-speed IO remap      = VIO3V3_IO_HSLV and VDD3V3_IO_HSLV enabled
SPI2 TX DMA request/channel   = request 65 / DMA1_Channel3
SPI2 RX DMA request/channel   = request 66 / DMA1_Channel2
```

One poll now uses software handshaking over SPI: H417 first resynchronizes the
bus, sends a short command frame, then reads one 24-byte key-state frame. The
host does not keep clocking continuously.

## 2026-06-20 high-speed experiment

The last known stable hardware-SPI setting before this experiment was:

```text
H417 SPI2 prescaler           = SPI_BaudRatePrescaler_Mode5 with HSRX mode2
Expected SCK at 100 MHz HCLK  = 14.29 MHz
CH585 system clock            = default 62.4 MHz
```

The high-speed experiment changed these speed-related conditions:

```text
H417 SPI2 prescaler           = Mode4/Mode3 experiments
Expected SCK at 100 MHz HCLK  = 16.67 MHz / 20.00 MHz
H417 high-speed IO remap      = enabled for 3.3V IO domains
CH585 system clock            = 78 MHz
CH585 PA15/MISO drive         = 20mA push-pull, one-CH585 test only
```

CH585 hex for this experiment:

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
```

Important: the 20mA push-pull MISO setting is only for the current single-CH585
bench test. A final two-CH585 shared-MISO bus must either rely on CH585 SPI0
tri-stating MISO when CS is inactive, or add external isolation/buffering.

## Current key-state frame

The current test protocol lets CH585 do the key decision locally. CH585 may use
ADC values internally, but it sends only a compact key-state bitmap to H417.
H417 validates the frame, merges the bitmap, and exposes released/pressed values
as 1000/3000 in the existing `KS` debug stream.

```text
offset  size  field       value
0       2     magic       0x4BD3, little-endian bytes d3 4b
2       1     version     2
3       1     type        0x10 = KEY_STATE
4       1     source_id   0 or 1
5       1     key_count   64
6       2     seq         CH585 state-frame sequence
8       2     flags       READY/OVERRUN/ADC_ERROR/STALE/SYNC_LOST/CMD_ERROR
10      2     ack_seq     last accepted H417 command, 0xffff until commands exist
12      8     down_bits   64 keys, bit=1 means pressed, key0 is bit0 of byte0
20      2     diag        diagnostic/reserved
22      2     crc16       CRC-CCITT over bytes 0..21
```

Frame size is 24 bytes. Two CH585 devices therefore cost 48 bytes per complete
keyboard scan. At 8 kHz that is 384 kB/s, or 3.072 Mbit/s of payload before SPI
overhead, much smaller than the old 142-byte ADC frame.

## Software handshake, no extra READY wire

The current handshake plan does not add a hardware READY pin. H417 and CH585 use
two SPI transactions per scan request:

```text
transaction 1: H417 -> CH585 command frame, 12 bytes
transaction 2: H417 <- CH585 key-state frame, 24 bytes
```

Command frame:

```text
offset  size  field       value
0       2     magic       0x524B
2       1     version     2
3       1     cmd         0x01 = GET_STATE
4       2     host_seq    H417 request sequence
6       2     ack_seq     last CH585 seq accepted by H417, or 0xffff
8       2     flags       reserved
10      2     crc16       CRC-CCITT over bytes 0..9
```

Response rule:

```text
H417 sends command(host_seq=N)
CH585 validates command, builds a state frame, and sets frame.ack_seq=N
H417 accepts the state frame only if CRC is valid and ack_seq == N
```

Before the formal command, H417 also clocks one 24-byte drain transaction. This
resynchronizes the two sides if reset or a failed transfer leaves CH585 in the
"waiting to send data" state. If CH585 receives an invalid command, it discards
that transaction and returns to waiting for a command. This gives us a real
request/response/ack loop without adding another wire.

Current observed result at Mode5/HSRX mode1:

```text
cmd_timeout=0
ack_err=0
crc=0
seq_drop=0
magic errors stop after startup resync
effective measured SCK about 13.7 MHz
```

## Current correction

Previous interpretation:

```text
H417 does not use hardware SPI, and uses GPIO-DMA software SPI instead.
```

Correct interpretation:

```text
H417 should use hardware SPI2 for the fast data bus.
Only CS is controlled by GPIO.
```

This is important because only the CS edges cost GPIO toggles. SCK, MOSI and
MISO stay inside the SPI peripheral, so the bus can run at hardware-SPI speed
and can use DMA normally.

## Recommended final one-bus wiring

Use one H417 hardware SPI bus shared by the two CH585 boards:

```text
H417 SPI2_SCK  -> CH585_0 PA13/SCK0
H417 SPI2_SCK  -> CH585_1 PA13/SCK0

H417 SPI2_MOSI -> CH585_0 PA14/MOSI
H417 SPI2_MOSI -> CH585_1 PA14/MOSI

H417 SPI2_MISO <- CH585_0 PA15/MISO
H417 SPI2_MISO <- CH585_1 PA15/MISO

H417 GPIO CS0  -> CH585_0 PA12/SCS
H417 GPIO CS1  -> CH585_1 PA12/SCS

GND common
3V3 common
```

For the first hardware-SPI experiment, use the WCH example pin group:

```text
H417 PB13 -> CH585 PA13  SCK
H417 PC1  -> CH585 PA14  MOSI
H417 PC2  <- CH585 PA15  MISO
H417 PB12 -> CH585 PA12  CS0   (method 2 hardware NSS)
```

When the second CH585 is added:

```text
H417 PB13 -> both CH585 PA13/SCK0
H417 PC1  -> both CH585 PA14/MOSI
H417 PC2  <- both CH585 PA15/MISO
H417 PD6  -> CH585_0 PA12/SCS
H417 PD7  -> CH585_1 PA12/SCS
```

Sharing MISO is valid only if each CH585 releases MISO when its CS is inactive.
The CH585 manual says SPI0 slave can auto switch MISO output during chip-select
active time, but this should still be verified on the bench. During early
bring-up, test one CH585 first.

## H417 SPI2 setup

Use SPI2 in master mode:

```c
SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
SPI_InitStructure.SPI_NSS = SPI_NSS_Hard;  /* current method-2 firmware */
SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
```

For the later all-GPIO-CS multi-slave version, change `SPI_NSS_Hard` to
`SPI_NSS_Soft` and drive both CS0/CS1 as GPIO.

The official H417 SPI DMA example uses:

```text
SPI2_TX DMA request = 65
SPI2_RX DMA request = 66
DMA1_Channel3 = TX
DMA1_Channel2 = RX
```

The current CH585 key-state frame is 24 bytes, so H417 should transfer exactly
24 dummy bytes while receiving 24 bytes:

```text
CSx low
start SPI2 TX DMA with 0xFF dummy bytes
start SPI2 RX DMA into frame buffer
wait for DMA complete and SPI not busy
CSx high
validate magic/version/source/seq/crc
process and report
request next frame
```

This still satisfies the architecture rule:

```text
Host gets one frame -> host processes/sends it -> host requests the next frame.
```

## Why GPIO CS is enough

For two CH585 slaves on one SPI bus:

```text
SCK/MOSI/MISO are shared high-speed bus signals.
CS0/CS1 choose which CH585 is active.
```

Only one GPIO transition is needed before the DMA transfer and one after it.
These two GPIO writes do not limit the SPI clock. The previous GPIO-DMA
software-SPI limit happened because every SCK edge was a GPIO/DMA event.

## Can hardware NSS be mixed with GPIO CS?

It is possible in principle:

```text
SPI2_NSS hardware pin -> CH585_0 CS
extra GPIO CS         -> CH585_1 CS
```

But this is not the recommended bring-up wiring unless the exact H417 hardware
NSS behavior is verified. The risk is that when H417 communicates with CH585_1
using the extra GPIO CS, the SPI peripheral may also drive its hardware NSS
active and unintentionally select CH585_0 at the same time. That would make two
CH585 MISO outputs contend on the shared MISO bus.

For the first reliable multi-slave design, use:

```text
SPI_NSS_Soft
CS0 = GPIO
CS1 = GPIO
```

Hardware SPI still provides the high-speed SCK/MOSI/MISO transfer. GPIO CS only
selects the target slave.

## Speed expectation

H417 hardware SPI supports clocking from the HCLK domain and DMA, so it should
be much faster than the measured GPIO-DMA software-SPI result of about 4.16 MHz.

The H417 register manual lists SPI clock up to 75 MHz. The CH585 timing table
does not require `Fsys/2`; with a 78 MHz system clock, its setup/hold timing is
compatible with a 70 MHz-class SPI clock in principle. The real limit now depends
on CH585 MISO output-valid time, H417 sampling phase, wire length, common ground,
and whether the CH585 slave DMA is armed before CS goes active.

The target is 40 MHz or higher, then approach 70 MHz only after the 24-byte frame
has zero CRC and sequence errors. Raise gradually:

```text
11 MHz -> 20 MHz -> 30 MHz -> 40 MHz -> 50 MHz -> 60 MHz -> 70 MHz
```

For high speed, keep H417 on hardware SPI + DMA, keep CS setup/hold controlled,
use short wires, add multiple ground wires if needed, and test SPI mode/phase
against CRC/sequence counters rather than visual output alone.

Current bench result with one CH585 and 24-byte state frame:

```text
Mode7, HSRX mode1, expected 11.11 MHz: stable
Mode5, HSRX mode1, expected 14.29 MHz: stable; current software-handshake default
Mode5, HSRX mode2, expected 14.29 MHz: stable before software handshake
Mode4, HSRX mode1, expected 16.67 MHz: mostly works with software handshake, but still has occasional magic errors
Mode4, HSRX mode2, expected 16.67 MHz: fails with version/CRC errors before software handshake
Mode3, HSRX mode1, expected 20.00 MHz: fails with magic errors, both Mode0 and Mode3
```

## Next code change

The H417 code now has a hardware-SPI2 backend and keeps the current GPIO-DMA
software-SPI backend as a fallback:

```text
preferred backend = hw-spi2-gpiocs
fallback backend  = dma-gpio/cpu-gpio
```

The first target should be one CH585 on `CS0`, then add `CS1` after the single
slave path is stable.
