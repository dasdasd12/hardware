# SPI speed display and DMA debug notes

This note records the current debug change for the H417 + CH585M SPI link.

## What was changed

### H417: visible SPI speed

File:

```text
F:\嵌赛\hardware\rtthread_port\applications\ch585_spi_scan.c
```

The H417 firmware now measures the source0 software-SPI data phase with the
RISC-V `mcycle` counter. The measurement starts immediately before clocking the
142-byte frame and ends immediately after the last byte is read. The CS setup
delay is not counted as SCK time.

The COM4/WCH-Link log now prints a line like:

```text
src0 speed: 142 bytes 1136 bits xfer=xxxxx us cycles=xxxxx sck=xx.x kHz frame=xx.x fps core=xxxxxxxx Hz
```

Meaning:

- `xfer`: time used to clock the frame payload.
- `sck`: effective software-SPI clock.
- `frame`: maximum request/read frame rate for one CH585M at the measured speed.
- `core`: H417 core clock used for the conversion.

At 70 kHz, a 142-byte frame is:

```text
142 * 8 = 1136 bits
1136 / 70000 = 16.23 ms
```

So if the log shows about `16200 us` and `70.0 kHz`, it matches the teammate's
70 kHz expectation.

### CH585M: SPI0 slave DMA transmit

File:

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\src\main.c
```

The CH585M test firmware now defaults to:

```c
#define CH585_USE_SPI0_SLAVE_DMA 1
SPI0_SlaveDMATrans((uint8_t *)&g_frame, sizeof(g_frame));
```

This uses the WCH SDK SPI0 DMA path. It still keeps the old FIFO path behind a
compile-time switch:

```c
#define CH585_USE_SPI0_SLAVE_DMA 0
```

Use the DMA hex for CH585M:

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
```

## How to test

1. Burn H417:

```text
F:\嵌赛\hardware\rtthread_port\build\v5f\rtthread_ch32h417_v5f.hex
```

2. Burn CH585M manually:

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
```

3. Connect the existing first SPI set:

```text
H417 PD2 -> CH585 PA13  SCK
H417 PD3 -> CH585 PA14  MOSI
H417 PD4 <- CH585 PA15  MISO
H417 PD6 -> CH585 PA12  CS
GND      -> GND
3V3      -> 3V3
```

4. Watch COM4. A healthy DMA run should still show:

```text
src0 ok increasing
fetch=0
magic=0
crc not increasing
repair=0
src0 speed: ...
```

5. Watch COM5 for the existing USBFS CDC key-engine output.

## H417 software SPI plus DMA backend

The H417 side now has an experimental DMA GPIO backend while still keeping the
CPU GPIO backend in the same file.

Current default:

```c
#define APP_CH585_SPI_DMA_BACKEND 1
#define APP_CH585_SPI_DMA_TARGET_KHZ 70U
```

Implementation file:

```text
F:\嵌赛\hardware\rtthread_port\applications\ch585_spi_scan.c
```

The selected pins are unchanged:

```text
PD2 = SCK
PD3 = MOSI, held high for the current dummy request
PD4 = MISO0
PD6 = CS0
```

The backend uses `TIM8_UP` as the DMA trigger, request number `36` from the WCH
SoftUART example. It maps two DMA channels to the same timer update request:

```text
DMA1_Channel1: memory -> GPIOD->BSHR, outputs the SCK GPIO waveform
DMA1_Channel2: GPIOD->INDR -> memory, samples MISO
```

One SPI bit is represented by four timer/DMA phases:

```text
phase 0: write BR2, SCK low
phase 1: write BS2, SCK high
phase 2: write 0, keep SCK high and sample MISO
phase 3: write BR2, SCK low
```

This avoids sampling at the same instant as the SCK rising edge. At a 70 kHz SCK
target the timer DMA phase rate is about:

```text
70 kHz * 4 = 280 kHz
```

For the 142-byte test frame:

```text
142 bytes * 8 bits * 4 phases = 4544 DMA phases
```

After DMA completes, H417 packs the sampled PD4 bits back into the existing
142-byte frame buffer and runs the same magic/version/source/length/CRC checks
as before. The host still performs exactly one transaction per requested frame:

```text
CS low -> clock one 142-byte frame -> CS high -> validate/merge/process/report
```

So this still follows the "process current data, then request the next frame"
architecture.

The COM4 log now includes a DMA line:

```text
src0 backend=dma-gpio dma_cfg=1 runs=... timeout=... te=... target=70 kHz phase=4 period=... sample_or=... sample_and=...
```

Current observed H417 log after fixing the TIM8 clock base to `HCLKClock`:

```text
src0 ok=41 fetch=0 magic=0 ver=0 src=0 len=0 crc=0 seq_drop=0
src0 backend=dma-gpio dma_cfg=1 runs=41 timeout=0 te=0 target=70 kHz phase=4 period=356 sample_or=001c sample_and=000c
src0 speed: 142 bytes 1136 bits xfer=16222 us cycles=6488977 sck=70.0 kHz frame=61.6 fps core=400000000 Hz hclk=100000000 Hz
```

Important detail: V5F `SystemCoreClock` is 400 MHz, but TIM8 is clocked from the
100 MHz HCLK domain in this project. The DMA timer period must therefore be
calculated from `HCLKClock`, not `SystemCoreClock`. Using 400 MHz produced a
measured SCK of about 17.4 kHz because each SPI bit has four DMA phases.

Healthy signs:

```text
backend=dma-gpio
timeout=0
te=0
src0 crc not increasing
src0 speed close to 70.0 kHz
```

If DMA setup fails before a transfer starts, the code falls back to the old CPU
GPIO bit-bang backend. If DMA runs but samples wrong data, the CRC counters will
show the failure instead of silently hiding it.

## 70 MHz target stress test

The teammate clarified that the desired number was 70 MHz, not 70 kHz. We tried
that target on the current H417 "software SPI by GPIO DMA" backend:

```c
#define APP_CH585_SPI_DMA_TARGET_KHZ 70000U
```

Two hardware facts matter:

```text
H417 V5F core clock = 400 MHz
TIM8/GPIO/DMA clock base used here = HCLKClock = 100 MHz
Current safe waveform = 4 DMA phases per SPI bit
```

With 4 phases per bit, a true 70 MHz SCK would require:

```text
70 MHz * 4 = 280 MHz DMA/GPIO phase rate
```

That is above the 100 MHz HCLK domain used by this GPIO DMA path.

### Result 1: period = 0

Allowing the timer period to become 0 did not work. The DMA transfer timed out
and no valid source0 frame was decoded:

```text
src0 ok=0
src0 backend=dma-gpio dma_cfg=1 runs=9 timeout=9 te=0 target=70000 kHz phase=4 period=0
```

### Result 2: period = 1

Clamping the timer to the smallest usable period, `period=1`, works and keeps
CRC clean, but the measured SPI speed is only about 2.08 MHz:

```text
src0 ok=33 fetch=0 magic=0 ver=0 src=0 len=0 crc=0 seq_drop=0
src0 backend=dma-gpio dma_cfg=1 runs=33 timeout=0 te=0 target=70000 kHz phase=4 period=1
src0 speed: 142 bytes 1136 bits xfer=546 us cycles=218221 sck=2080.5 kHz frame=1831.5 fps core=400000000 Hz hclk=100000000 Hz
```

Conclusion: this GPIO-DMA software SPI backend can be pushed above 1 MHz and
was observed clean at about 2.08 MHz with the original 4-phase waveform, but it
cannot produce 70 MHz. A 70 MHz link needs H417 hardware SPI/QSPI-style
peripheral support with DMA, or a different parallel/high-speed transport
design.

### Phase compression tests

To see whether the GPIO-DMA path had more room, the waveform was compressed
while keeping the requested target at 70 MHz:

```text
4 phase/bit: low, high, sample while high, low
3 phase/bit: high, sample while high, low
2 phase/bit: high/sample, low
```

Observed results:

```text
4 phase/bit, period=1: sck=2080.5 kHz, crc=0
3 phase/bit, period=1: sck=2777.5 kHz, crc=0
2 phase/bit, period=1: sck=4161.1 kHz, crc=0
```

The current test firmware is left at the fastest clean GPIO-DMA setting:

```c
#define APP_CH585_SPI_DMA_TARGET_KHZ 70000U
#define CH585_SPI_DMA_PHASES_PER_BIT 2U
#define CH585_SPI_DMA_SAMPLE_PHASE   0U
```

Log example:

```text
src0 ok=17 fetch=0 magic=0 ver=0 src=0 len=0 crc=0 seq_drop=0
src0 backend=dma-gpio dma_cfg=1 runs=17 timeout=0 te=0 target=70000 kHz phase=2 period=1
src0 speed: 142 bytes 1136 bits xfer=273 us cycles=109161 sck=4161.1 kHz frame=3663.0 fps core=400000000 Hz hclk=100000000 Hz
```

### Further software-SPI options

Changing the GPIO write target from `GPIOD->BSHR` 32-bit writes to
`GPIOD->OUTDR` 16-bit halfword writes was also tested:

```c
#define APP_CH585_SPI_DMA_USE_OUTDR 1
```

It did not improve the measured SCK. The clean result stayed at about
`4161.1 kHz` with `phase=2` and `period=1`.

At this point the current PD2/PD3/PD4 GPIO-DMA soft-SPI path is limited by the
timer/DMA/GPIO service rate, not by the requested target value. Possible ways
to keep a "simulated" SPI-style interface and try to go faster are:

```text
1. Raise the HCLK/FPRE domain from the current 100 MHz.
   This may scale the timer/DMA trigger rate, but it also changes a shared
   clock domain and can affect USB, RT-Thread timing, flash wait states and
   other peripherals. Even a 4x HCLK improvement would only predict about
   16 MHz from the current 4.16 MHz result, still below 70 MHz.

2. Let a timer output pin generate SCK directly and use DMA only for sampling.
   This needs SCK to move to a timer output-capable GPIO. The current PD2 pin is
   not suitable because it is TIM3_ETR input, not a timer PWM/output channel.

3. Use a programmable/high-speed peripheral instead of ordinary GPIO bit
   toggling. This becomes a new transport design and is no longer the same
   simple GPIO soft-SPI link.
```

So the current wiring can still be used for protocol bring-up and algorithm
debugging, but it should not be treated as the final 70 MHz transport.

### Manual recheck notes

The board/chip manuals were rechecked after the GPIO-DMA tests.

H417 evaluation board manual:

```text
USBFS connector: PA11 = DM, PA12 = DP
WCH-Link is the normal debug/download path.
```

H417 data manual:

```text
SPI2_SCK  candidates: PB13(AF5), PB10(AF5), PA9(AF5), PA12(AF5), PD3(AF5)
SPI2_MOSI candidates: PB15(AF5), PC1(AF5), PC3(AF5)
SPI2_MISO candidates: PB14(AF5), PC2(AF5)
SPI2_NSS  candidates: PB12(AF5), PB9(AF5), PA11(AF5), PB4(AF7)
```

This means a real hardware-SPI2 experiment is not just an SCK-only change; it
needs the SPI2 signal group to be rewired.

The same H417 manual also shows timer-output-capable pins that can be used for
a "timer-generated SCK" experiment. Useful candidates near the current wiring
are:

```text
PD3 = TIM11_CH1(AF2) or TIM3_CH1(AF9)
PD4 = TIM11_CH2(AF2) or TIM3_CH2(AF9)
PD5 = TIM11_CH3(AF2) or TIM3_CH3(AF9)
PD6 = TIM11_CH4(AF2) or TIM3_CH4(AF9)
PC6 = TIM8_CH1(AF3)  or TIM3_CH1(AF2)
PC7 = TIM8_CH2(AF3)  or TIM3_CH2(AF2)
PC8 = TIM8_CH3(AF3)  or TIM3_CH3(AF2)
PC9 = TIM8_CH4(AF3)  or TIM3_CH4(AF2)
```

By contrast, the current `PD2` pin is `TIM3_ETR(AF2)`, which is an external
timer input, not a PWM/output-compare channel. That is why `PD2` is a poor SCK
pin for the timer-generated-SCK scheme.

CH585 data manual:

```text
CH585M SPI0 pins:
PA12 = SCS
PA13 = SCK0
PA14 = MOSI
PA15 = MISO

SPI0 supports master and slave mode.
SPI1 supports master mode only.
SPI supports mode 0 and mode 3, 8-bit transfer, MSB/LSB selectable, FIFO and DMA.
The documented maximum SPI clock is up to Fsys/2.
CH585 maximum system clock is 78 MHz.
```

So for CH585 as SPI0 slave, an external H417 SCK around 39 MHz is the clear
datasheet-level ceiling from the CH585 side if Fsys is 78 MHz. A 70 MHz SCK is
above that SPI clock statement and should be treated as out of spec unless WCH
or a proven local example confirms otherwise.

### Practical route to real 70 MHz

The local WCH SPI DMA example uses the H417 hardware SPI2 peripheral:

```text
SPI2_SCK  = PB13, AF5
SPI2_MOSI = PC1,  AF5
SPI2_MISO = PC2,  AF5
SPI2_NSS  = PB12, AF5 if hard NSS is used
DMA TX request = 65
DMA RX request = 66
```

The WCH SPI driver also exposes:

```c
SPI_HighSpeedMode_Config(SPI2, SPI_HIGH_SPEED_MODE1, ENABLE);
SPI_HighSpeedMode_Config(SPI2, SPI_HIGH_SPEED_MODE2, ENABLE);
```

So the next high-speed experiment should rewire one CH585M to the SPI2 pins and
test hardware SPI2 + DMA. The current PD2/PD3/PD4 GPIO wiring is good for
bring-up, but it is not a realistic 70 MHz transport.
