# 32K Profile Path Feasibility Tests

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
