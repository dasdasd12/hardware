# Hardware test firmware

`catalog.json` is the source of truth for flashable tests, their lifecycle,
destructive ranges, build selectors, paired targets, and preserved feasibility
work.

```powershell
make -C hw_tests list
make -C hw_tests check
make -C hw_tests build-active
make -C hw_tests build-32k
make -C hw_tests build-all
```

Directory roles:

- `h417/cases/`: shared V3F standalone and V5F RT-Thread test sources;
- `h417/v3f/`: independent V3F projects such as the internal Flash CDC test;
- `ch585/`: selectable CH585 board-test firmware;
- `common/`: protocols shared by paired H417/CH585 tests;
- chip-local `tools/`: serial monitors and interactive test front ends.

Build success is not a hardware result. Record a real board result separately
before changing a catalog entry from bring-up or unknown state.

## H417 SDRAM native-x16 low-byte test over USBFS CDC

`h417_v5f_sdram_memtest` does not initialize the LCD or LTDC. The V5F test
reuses the USBFS CDC initialization already called by `main`, waits for a
`start` command from the PC, and keeps FMC Bank5 in the fitted SDRAM's native
x16 mode. Version 23 uses aligned volatile `uint16_t` loads and stores, so both
the CPU and FMC use the SDRAM's natural width. Every test value has a zero high
byte and verification masks the read value with `0x00ff`; only the proven
D0-D7 lane participates in pass/fail decisions. Logical element `n` is at
`base + 2*n`, so the 16 Mi-element logical range covers the complete 32 MiB
physical window `0xC0000000-0xC1FFFFFF`.

The firmware reports the programmed SDRAM control register, both DQM alternate
functions, and PB1/BA0 plus PB15/BA1 alternate functions. In native x16 mode,
the four 4 MiB logical Bank bases are physically 8 MiB apart at `0xC0000000`,
`0xC0800000`, `0xC1000000`, and `0xC1800000`. Distinct Bank values and both
grouped and alternating reads isolate read-side Bank switching. Pair tests are
then repeated with normal automatic row management and with an explicit PALL
(precharge-all) command between accesses. These short probes are diagnostic
only. Before them, a 144-point scan covers normal reads, enhanced reads, and
RBURST reads; each legal mode is combined with all 16 documented read phases
and all three read-pipe delays using alternating masked-halfword reads across
all four Banks. The scan is diagnostic: functional tests always restore normal
read mode, RBURST disabled, phase 10, and no read-pipe delay. This prevents a
small noisy score difference from selecting the unusable RBURST mode. The
enhanced-read bit is written directly as documented at `R32_SDRAM_MISC[15]`; the vendor
header's clear mask incorrectly names bit 12 for that field. Regardless of
the short probes, the data/address/March/pattern/inverse/retention sequence
covers the complete 16 MiB logical range. Its address-based patterns provide
the final pass/fail decision and expose real Bank aliasing without relying on a
two-address probe. The large phases use separate write and read sweeps rather
than alternating direction on every byte. Progress is reported after every
MiB. SDCLK remains about 33.33 MHz, the correctly shifted refresh field is
used, the forced read mode is reported, and the native Bank5 window is used with
`BMP=0` and BCR1 `FMC_EN` clear.

Before the legacy pair probes, version 20 also measures Bank-switch settling
with both byte loads and native aligned halfword loads. For each CPU access
width it discards 0, 1, 2, 4, 8, 16, or 32 reads after every Bank change and
scores the following value in forward and reverse Bank order. A score of 256
means all measured reads matched. This distinguishes true BA aliasing from a
stale first read and shows whether byte transactions are the trigger.

Version 21 then compares one dummy read followed by an HB fence, 1 us or 10 us
delay, or an explicit PALL command against direct, one-dummy, and 32-dummy
reads. The 10 us case is repeated with the refresh counter temporarily changed
from 240 to 8191, then immediately restored. This distinguishes ordering,
elapsed-time, row-state, and periodic-refresh effects without using the long
refresh interval for the full memory test.

Version 22 adds a transition matrix using two independently stored values. It
compares adjacent columns, two rows in the same Bank, BA0, BA1, and BA0 while
the upper Bank is selected. Each pair is scored with direct reads, one dummy
plus an HB fence, and one dummy plus PALL. The BA0 pair is repeated after
temporarily selecting the controller's two-Bank mode, then the four-Bank SDCR
is restored before the remaining diagnostics and full test.

Version 23 moves the transition matrix and the full-memory test to aligned
halfword transactions while comparing only D0-D7. It also writes distinct
values to all four Banks through `0xC0000000`, switches `BMP` to expose the
same SDRAM at `0x60000000`, and reads them back. A second set is written through
the remapped window and verified again after restoring the native window.
Each direction reports direct, explicit-PALL, and 32-discard-read scores. This
separates an SDRAM Bank problem from a native HB-window mapping-path problem.

Version 24 holds the refresh counter at 8191 while comparing direct access,
20 us delay, 32 discarded reads, one or two PALL commands, PALL plus one
explicit auto-refresh, and an SDRAM-controller enable toggle. It also reports
the first returned values after a known BA0 transition. A separate refresh
profile scores counts 41, 80, 110, 160, 240, 480, and 8191; count 110 is the
approximately correct 33.33 MHz value for an IS45S16160J A2-grade 8192/32 ms
refresh requirement, while count 240 represents 8192/64 ms. If ordinary
cross-Bank reads still fail, the complete March/pattern/retention sequence is
run independently inside each of the four 4 Mi-element Banks. The report then
distinguishes `STORAGE RESULT` from `FUNCTION RESULT`: healthy storage does not
turn unreliable ordinary cross-Bank loads into a functional pass.

Version 25 compares the current native/normal-read profile with WCH's April
2026 16-bit reference profile: enhanced-read bit 15 enabled and the SDRAM
window remapped to `0x60000000`. Both profiles are populated by aligned 32-bit
stores containing repeated bytes, then read across all four Banks using CPU
8-, 16-, and 32-bit transactions. Each access width reports scores after 0,
1, 2, 4, 8, 16, and 32 discarded reads. This tests the material differences
from WCH's example without trusting its limited 10 KiB, Bank-0-only demo loop,
and restores native mapping, normal reads, phase 10, and no read pipe before
the existing diagnostics and storage tests.

Version 26 adds the discriminator missing from the CPU-width matrix. DMA1
channel 3 copies one 32-byte block from each Bank to aligned internal SRAM 256
times, first as eight 32-bit transfers and then as one native 256-bit transfer.
It also performs a single aligned 64-byte DMA copy across each of the three
physical Bank boundaries and verifies the 32 bytes on both sides. Transfer
errors are reported separately from data mismatches. A DMA pass alongside a
CPU failure localizes the problem to the V5F load-response path; matching DMA
and CPU failures localize it below the CPU, in the FMC/SDRAM transaction path.

Version 27 corrects the DMA verifier to compare `0x00FF00FF` in every returned
32-bit word. A 32-bit CPU or DMA transaction over the x16 bus contains two
SDRAM beats, so this mask checks D0-D7 in both beats while excluding the still
unproven D8-D15 lane. It also reports the first two raw words from a 32-bit
Bank-0 DMA and a 256-bit Bank-1 DMA before printing the masked scores.

Version 28 measures whether a usable DMA recovery rule exists. Both 32-bit and
256-bit DMA Bank scans are repeated with one or two full-block dummy transfers
to the destination Bank, PALL, PALL plus one explicit auto-refresh command, or
a 10 us delay before the scored transfer. Each line reports the direct and
recovered scores out of 256 and aggregates command/transfer failures separately
from masked data mismatches.

Version 29 tests matched CAS-latency settings rather than changing only the
controller side. It issues a mode-register load for CL3 (`0x0230`) and CL2
(`0x0220`), updates the FMC CAS field to the same value, and scans all 16 read
phases and three read-pipe delays for each mode. The best CPU score is verified
again and then checked with both 32-bit and 256-bit masked DMA reads. The test
restores CL3, phase 10, no read pipe, normal read mode, and the native window
before the existing diagnostics continue.

Version 30 separates two remaining WCH-specific controller fields from live
register switching. It reruns the SDRAM initialization sequence in normal mode with
`NRFS_CNT=0`, in enhanced/prefetch mode with `NRFS_CNT=0`, and in normal mode
with `NRFS_CNT=15`; bit 15 and the refresh-burst field are programmed before
the SDRAM initialization command sequence. Every cold profile scans all 16
phases and three read-pipe delays, verifies the best CPU score, checks the
32-discard workaround, and runs a 256-bit DMA Bank scan. It then tests all 16
`NRFS_CNT` values in normal mode and reports CPU and DMA scores in value order.
The reference manual defines bit 15 as enhanced/prefetch when set and
`NRFS_CNT=n` as `n+1` auto-refresh commands per refresh event. The vendor header
is not used for bit 15 because its clear mask and Enable/Disable constants are
internally inconsistent. A final initialization-sequence rerun restores normal mode,
`NRFS_CNT=0`, CL3, phase 10, no read pipe, and the native address window.
Version 31 later showed that these sequence reruns did not reset the FMC
prefetch state, so the v30 prefetch score is retained only as diagnostic
history and must not be treated as a functional pass.

Version 31 follows the v30 result that the apparent cold-start prefetch produced a perfect
256-sample CPU scan but not a reliable DMA result. The runtime is narrowed to
that discriminator: it cold-initializes bit 15 prefetch with `NRFS_CNT=0`,
performs 4096 ordinary CPU reads alternating across the four Banks, and reads
256 consecutive low-byte elements across each of the three Bank boundaries.
It captures raw 256-bit DMA words from every Bank, then scores 32-bit and
256-bit DMA reads with direct, one- and two-dummy, PALL, PALL plus auto-refresh,
and 10 us delay strategies. Finally, it runs the complete 16 MiB low-byte
data/address/March/pattern/inverse/retention test across all four Banks while
prefetch remains enabled. CPU storage success and direct DMA success are
reported separately; a CPU pass does not hide a failed DMA path.

Version 32 corrects that experimental flaw. The test no longer initializes
SDRAM in normal mode before the prefetch test. Bit 15 is programmed before the
first and only FMC enable and SDRAM command sequence after MCU reset, matching
the order in WCH's reference example. The validator then writes fresh patterns
and runs the v31 CPU, Bank-boundary, DMA, and complete 16 MiB checks without
reinitializing the controller. The board must be reset or power-cycled after
flashing so no FMC prefetch state survives from the preceding firmware run.

Version 33 follows the v32 result that first-boot prefetch can repeatedly
return an initially fetched value but does not provide coherent ordinary RAM:
later writes are hidden by stale prefetched data, sequential Bank-boundary
verification fails, and DMA observes prior-Bank values. It tests whether a
software mode transition can make the feature usable. A single address is
written and reread before and after disabling/re-enabling prefetch; two sets of
four-Bank values are written with prefetch disabled and read after enabling it;
and two untouched Bank-0 ranges compare prefetch-on writes with normal writes
followed by prefetch reads. DMA is also retried after normal-mode population
and a prefetch transition. This focused diagnostic ends after reporting CPU
and DMA coherency instead of rerunning the already failing 16 MiB test.

Version 34 tests the largest remaining difference from WCH's reference code:
the official example writes `FMC_SDClockPeriod=1`, so with the board's 100 MHz
HCLK the SDRAM clock is also 100 MHz. Earlier diagnostics deliberately reduced
SDCLK to 33.33 MHz, but CH32H417's private prefetch/read-return path may depend
on the documented 1:1 ratio. Version 34 keeps first-boot prefetch, phase 10,
no read pipe, CL3, the vendor timing fields, and the vendor refresh value 677,
then reruns the focused v33 coherency tests. This is safe for the fitted
`-7` speed-grade SDRAM and directly distinguishes an FMC clock-ratio problem
from the already excluded CAS, phase, pipe, and refresh-burst settings.

Version 35 follows the v34 result that the 1:1, 100 MHz FMC clock fixes all
four-Bank alternating reads and both 32-bit and 256-bit DMA paths, while the
optional bit-15 prefetch mode remains incoherent for writes and CPU halfword
streams. It therefore starts directly in normal read mode at 100 MHz. A quick
gate performs 4096 alternating CPU reads, a 2048-element sequential CPU test,
and both DMA widths. The existing Bank probe and complete 16 MiB low-byte
data/address/March/pattern/inverse/retention test then run without prefetch.
For the fitted A2 refresh grade, the refresh window is changed from 64 ms to
32 ms; at 100 MHz this programs count 370 (encoded `SDRTR=0x000002e4`).

Use a fresh build root so old dependency files from the `passed/` to `cases/`
directory migration cannot affect the build:

```powershell
make -B -C hw_tests/h417 BUILD_ROOT=build_sdram_x8 `
  HW_TEST=h417_v5f_sdram_memtest
```

Flash both H417 images:

```text
hw_tests/h417/build_sdram_x8/h417_v5f_sdram_memtest/V3F/h417_V3F.hex
hw_tests/h417/build_sdram_x8/h417_v5f_sdram_memtest/V5F/rtthread_ch32h417_V5F.hex
```

Then open the enumerated CDC port with the included reader. It sends `start`,
prints every stage and progress report, and exits with code 0 on pass or 2 on
failure:

```powershell
powershell -ExecutionPolicy Bypass `
  -File hw_tests/h417/tools/read_sdram_result.ps1 `
  -Port COM8 -TimeoutSeconds 180
```

The decisive startup reports are `READ_SCAN`, `MODE_SCAN`, and `ROUTE_SCAN`.
After those finite scans, `SCOPE READ` and `SCOPE PIN` repeat until Ctrl+C;
`rawbad` counts FMC mismatches and `fusebad` counts mismatches after diagnostic
GPIO reconstruction.

## Preserved 32K profile-path feasibility tests

These targets isolate the three independent requirements behind a 32K wired
profile path. They are test firmware, not product images.

## 1. USB 3.0 communication

This target compiles WCH's `USBSS/DEVICE/CH372Device` example directly. The
USBSS controller, its interrupts and the CH372 transfer loop all run on V3F;
V5F is not started and no V5F image is required.

Build the V3F image:

```powershell
make -B -C hw_tests/h417 HW_TEST=h417_v3f_usbss_ch372
```

The default example location is
`C:/program1/hardware/WCH/CH32H417/CH32H417EVT/EVT/EXAM/USBSS/DEVICE/CH372Device`.
Override `OFFICIAL_USBSS_EXAMPLE_ROOT` when the WCH EVT tree is elsewhere.

Flash only this V3F image:

```text
hw_tests/h417/build/h417_v3f_usbss_ch372/h417_v3f_usbss_ch372.bin
```

This is a vendor-specific CH372 device, not CDC. It will not create a COM
port. Install the driver shipped with the example, then verify the device is
present as `VID=1A86, PID=5537`:

```powershell
$example = 'C:\program1\hardware\WCH\CH32H417\CH32H417EVT\EVT\EXAM\USBSS\DEVICE\CH372Device'
$toolDir = 'hw_tests\h417\build\ch372_tool'
Expand-Archive -LiteralPath "$example\CH32H417_USBSS_Device_Speed_Test_Tool.zip" -DestinationPath $toolDir -Force
& "$toolDir\CH372DRV.EXE"
Get-PnpDevice -PresentOnly | Where-Object InstanceId -Match 'VID_1A86&PID_5537' | Format-Table Status,Class,FriendlyName,InstanceId
```

The executables are packaged in
`CH32H417_USBSS_Device_Speed_Test_Tool.zip` beside the example. Run
`& "$toolDir\CH37x端点测试工具_V130.exe"` with these official test cases:

1. Endpoint 3 download: bulk, fixed `0xA5`, default 4 MiB packet.
2. Endpoint 3 upload: bulk, fixed `0xA5`, default 4 MiB packet.
3. Loopback: bulk, upload endpoint 1, download endpoint 2, incrementing data,
   verify all data.

Pass criteria: the device is present, all three transfers run without data
errors, and sustained throughput is above 100 MB/s. The throughput threshold
distinguishes SuperSpeed from the example's USB 2.0 fallback and is well above
the 2.048 MB/s payload needed for 32,000 64-byte reports per second. This test
proves the raw USBSS path; it does not yet prove the final HID report format.

### USBSS link diagnostics over USBFS CDC

When the CH372 baseline appears only on its USB 2.0 companion port, build the
V3F SS-only diagnostic image:

```powershell
make -B -C hw_tests/h417 HW_TEST=h417_v3f_usbss_fs_diag
```

Flash only the V3F image (HEX is preferred by WCH-LinkUtility):

```text
hw_tests/h417/build/h417_v3f_usbss_fs_diag/h417_v3f_usbss_fs_diag.hex
hw_tests/h417/build/h417_v3f_usbss_fs_diag/h417_v3f_usbss_fs_diag.bin
```

This image starts only the product USBFS CDC/HID stack already used by the
working keyboard firmware (`VID:PID 1A86:FE17`). It reuses the product V3F
board initialization and startup code and starts the complete product USBHS
stack before USBFS, including UTMI setup and the product interrupt priorities.
The product USBHS and USBFS buffer addresses and V3F stack range are fixed to
the values in the working keyboard image; descriptor symbols are renamed only
inside the build so the product USBHS stack can coexist with the official USBSS
example. The large USBSS buffers use a separate high-RAM section.

Before the host raises DTR, version 11 intentionally performs no CDC transmit,
formatting, debug-register access, or USBSS initialization. First flash the image
and confirm the normal USBHS device plus USBFS CDC/HID device remain present in
USB Tree View for at least 30 seconds without opening the COM port. This is the
product dual-USB enumeration baseline. If it remains stable, start USBSS and read
the FS CDC log:

```powershell
python hw_tests/h417/tools/usbss_fs_diag_monitor.py
```

To open the COM port while leaving USBSS disabled, use:

```powershell
python hw_tests/h417/tools/usbss_fs_diag_monitor.py --no-start
```

FS-only mode is intentionally silent. The monitor prints an
`INTENTIONALLY_SILENT` marker locally; any firmware output before DTR is raised
is a failure of the baseline. After the staged DTR handshake, USBFS is raised
above USBSS interrupts,
SS event records are sent one non-blocking 64-byte packet at a time, and the
diagnostic policy blocks the example's USBHS fallback. A working `1A86:5537`
device can therefore only appear on the SuperSpeed port.

Version 12 uses DTR rising edges on the CDC control interface instead of bulk
OUT commands. It advances only after the monitor has printed a checkpoint and
pulsed DTR. It reports `START_ACK`, `BUFFERS_CLEARED`, `USBHS_QUIESCED`,
`PLATFORM_READY`, `PLL_ARMED`, `PLL_WRITE_OK`, and either `PLL_READY` or
`PLL_TIMEOUT`. `PLL_ARMED` is emitted before writing `RCC_USBSS_PLLON`, while
`PLL_WRITE_OK` proves that write returned. The USBHS controller is stopped
before USBSS takes ownership, while the USBHS PLL and UTMI needed by USBFS
remain enabled. `CALL_DEVICE_INIT` followed by a DTR pulse without
`DEVICE_INIT_RETURNED` means the fault occurs inside `USBSS_Device_Init()` or
that call disrupts the USBFS debug path.

Version 13 is an ordering-isolation image. Immediately after
`v3f_board_init()` establishes the system clocks, it writes
`RCC_USBSS_PLLON` and waits up to 200 ms for `RCC_USBSS_PLLRDY`; only then does
it initialize the product USBHS and USBFS stacks. The first DTR edge reports
either `EARLY_PLL_READY` or `EARLY_PLL_TIMEOUT`, including RCC snapshots from
before the write, immediately after the write, after the wait, and after USB2
initialization. It deliberately stops at that checkpoint and does not call the
USBSS device layer; the monitor exits after printing the result. If the CDC
device does not enumerate at all, the failure is before USB2 initialization.
If `EARLY_PLL_READY` is reported and CDC remains stable, the PLL and USB2 paths
coexist when initialized in that order.

Version 14 adds recovery for failures that happen before CDC exists. Before the
PLL write it starts the official independent-watchdog driver and stores ordered
stage markers at `0x20178180`, outside the existing V3F trace and V5F wake-probe
ranges. If execution stops, IWDG resets the MCU after about 3.2 seconds. The
next boot sees the retained marker, skips the PLL write, starts the known-good
USB2 stacks, and reports one of `RECOVER_PLL_ARMED`,
`RECOVER_WRITE_RETURNED`, `RECOVER_READ_RETURNED`, or `RECOVER_USB2_INIT`.
These respectively isolate the stop to the PLL register write, its first
readback, the ready-wait loop, or USB2 initialization after a successful PLL
probe. The monitor waits up to 12 seconds for the recovery CDC port and exits
after printing the V14 result. In V14, `--no-start` only suppresses the DTR
request for the stored result; it does not suppress the boot-time PLL probe.

Version 15 splits the PLL read-modify-write into independently retained stages:
`READ_ARMED`, `VALUE_LOADED`, `STORE_ARMED`, `STORE_RETURNED`, and
`READBACK_RETURNED`. Its first attempt also uses the official USBSS prelude
(`SystemInit` followed by `SystemAndCoreClockUpdate`) without applying the
product board initialization's VIO18 override. On a watchdog recovery boot it
does apply the product board initialization, but only to restore the proven CDC
path after the PLL attempt has been skipped. `RECOVER_STORE_ARMED` therefore
proves that the computed CTLR value was committed to SRAM and execution stopped
on the peripheral store itself.

Version 16 fixes a diagnostic-only privilege error exposed by the V15 result.
V3F enters `main` in user mode, so the V15 probe's machine-mode `mcycle` read
raised an illegal-instruction trap immediately after `READ_ARMED`; the default
HardFault loop then waited for the watchdog reset. The boot PLL probe now uses
a bounded poll count and therefore performs no counter-CSR access around the
RCC transaction. Other diagnostic timing reads use the user-mode `cycle`
(`ucycle`) CSR. A strong HardFault handler also retains the interrupted stage,
`mcause`, `mepc`, and `mtval`; recovery reports it as `RECOVER_TRAP`. Thus
`RECOVER_READ_ARMED` in V16 means the RCC read itself did not return, while
`RECOVER_STORE_ARMED` means the RCC write did not return. After retaining the
probe result, both the normal and recovery paths reapply `v3f_board_init()`
before starting USBHS/USBFS so the CDC result channel always uses the proven
product setup. That reinitialization may clear the live USBSS PLL bit; use the
retained `read` and `wait` fields, rather than `now`, to judge the PLL probe.

Version 17 continues after the V16 PLL result. Its first line uses
`SS17 boot=...`; only a fresh `boot=PLL_READY rec=0` result advances into the
existing DTR-controlled sequence. It then reports `BUFFERS_CLEARED`,
`USBHS_QUIESCED`, `PLATFORM_READY`, `PLL_ARMED`, `PLL_WRITE_OK`, `PLL_READY`,
and `CALL_DEVICE_INIT` before calling the unmodified WCH
`USBSS_Device_Init(ENABLE)` implementation. The retained record distinguishes
`RECOVER_DEVICE_INIT_ARMED` from `RECOVER_DEVICE_INIT_RETURNED`. After the call
returns, the firmware intentionally stops feeding IWDG until the link reaches
U0 and `USBSS_DevEnumStatus` becomes one; success is reported as
`USBSS_ENUMERATED`. If USBSS initialization interrupts the FS result channel or
never enumerates, IWDG restores the product USB2 path and the monitor reconnects
to print the retained link, USB controller, enumeration, and trap state.

Version 18 restores a required line from the official V3F `main`: before the
USBSS path starts, it assigns `Chip = (DBGMCU_GetCHIPID() >> 4) & 0x0f`.
Version 17 left `Chip` at its zero-initialized value, so newer silicon followed
the old-revision branch and did not enable or service `RX_SET_FC`. The retained
and live result lines now include the raw chip ID/revision, `LINK_INT_CTRL`,
`LINK_CFG`, `LINK_CTRL`, received/transmitted LMP data, and port-capability
state. The linked product `Delay_Us` is self-contained, so the official
`Delay_Init` call is not required in this combined image.

Version 19 removes two diagnostic timing changes exposed by the V18 LMP state.
The V18 result received and transmitted `0x280` (`LINK_SPEED | PORT_CAP`) but
did not set the received capability-valid bit before the 20-us LMP timeout.
USBSS LINK, device, and TIM12 interrupts now use the official priority zero,
while FS CDC remains at priority `0x80`. The official `USBSS_LINK_Handle` is
also compiled under its original symbol, so the LINK ISR no longer captures a
large register event before or after the handler. USB2 fallback remains
suppressed for this test to preserve the FS result channel, but its hook does
no work in the LINK ISR. A `state=U0` followed by `dev=1` means USBSS
enumeration completed; retained main-loop snapshots still report the final
state after watchdog recovery.

## 2. H417 to CH585 SPI speed

Build one CH585 slave and the matching H417 sweep target. Repeat for the other
half by changing both `HALF` and `SPI_SOURCE`.

```powershell
make -B -C hw_tests/ch585 TEST=ch585_spi0_speed_slave HALF=left
make -B -C hw_tests/h417 HW_TEST=h417_v5f_ch585_spi_speed SPI_SOURCE=left
```

Flash the selected CH585 image plus both H417 V3F/V5F images from the matching
build directory. Then run:

```powershell
python hw_tests/h417/tools/ch585_spi_32k_budget.py --port COM5
```

The CH585 test runs at 78 MHz. Both ends use DMA so every 193-byte frame is a
continuous Mode 0 transfer instead of a CPU-paced byte stream. The firmware
sweeps valid SPI dividers and H417 receive sampling modes with 512 validated
frames per setting. The PC tool selects the fastest error-free setting and
projects two protocol layouts:

- Current path: 32-byte command plus 12-byte state for each half.
- Proposed fast path: one 12-byte state transaction for each half.

Pass means the measured stable SCK can carry two short state frames inside
31.25 us. It does not include ADC production time or H417 Profile processing.
The reported frequency accounts for the H417 high-speed mode's non-power-of-two
divider mapping, and elapsed time uses the V5F `mcycle` clock rather than HCLK.

## 3. CH585 full-half 32K acquisition

Build and test both physical halves independently:

```powershell
make -B -C hw_tests/ch585 TEST=ch585_ads7948_32k_pipeline HALF=left
make -B -C hw_tests/ch585 TEST=ch585_ads7948_32k_pipeline HALF=right
```

Flash the selected CH585 image and connect UART1 (`PA9 TX`, `PA8 RX`). The
test uses a fixed per-half MUX schedule, preserves the ADS7948 one-conversion
pipeline and scans every active key on every frame. It alternates 2437/2438
system-clock periods to represent 31.25 us at 78 MHz.

```powershell
python hw_tests/ch585/tools/ch585_32k_acq_monitor.py --port COM6 --windows 3
```

Pass criteria: zero deadline overruns, worst scan cycles no greater than the
2438-cycle budget and at least 31,900 complete half-frames/s. UART output is
emitted between measurement windows and is excluded from scan timing.

## Interpreting the result

The complete 32K path is feasible only when all three tests pass on both
halves. A USBSS rate pass with repeated samples is not an input-rate pass, and
an ADC pass without enough SPI budget cannot reach H417 at 32K.
