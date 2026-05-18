# CH32H417 Dual-Core Notes

Use this reference when a task involves CH32H417 V3F/V5F build, flash, or debug.

## Source Of Truth

The local EVT examples under `C:\program1\hardware\WCH\CH32H417\CH32H417EVT\EVT\EXAM` define the practical command-line mapping. In particular, compare:

- `GPIO\GPIO_Toggle\V3F\GPIO_Toggle_V3F.wvproj`
- `GPIO\GPIO_Toggle\V5F\GPIO_Toggle_V5F.wvproj`
- `CPU\HSEM\HSEM_CoreSync`
- `CPU\IPC\IPC`
- `SRC\Ld\V3F\Link_v3f.ld`
- `SRC\Ld\V5F\Link_v5f.ld`
- MRS OpenOCD `wch-dual-core.cfg`

## Core Mapping

| Core | EVT role | Startup | Linker | Flash origin | OpenOCD target | GDB port |
| --- | --- | --- | --- | --- | --- | --- |
| V3F | boot/wake coordinator in EVT projects | `startup_ch32h417_v3f.S` | `Link_v3f.ld` | `0x00000000` | `wch_riscv.cpu.0` | `3333` |
| V5F | secondary core in EVT projects | `startup_ch32h417_v5f.S` | `Link_v5f.ld` | `0x00010000` | `wch_riscv.cpu.1` | `3334` |

This mapping follows the EVT `.wvproj` debug settings: V3F has `isMaster: true` and uses `masterGDBPort: 3333`; V5F uses `slaveGDBPort: 3334`.

## Build Requirements

Use the same compiler flags as MRS-generated H417 examples:

```make
-march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32
-msmall-data-limit=8 -msave-restore
-Os -fsigned-char -ffunction-sections -fdata-sections -fno-common
```

Use core-specific defines:

```make
-DCore_V3F
-DCore_V5F
```

Build each core with its own startup and linker script. Do not compile both startup files into one ELF.

## Flash Sequence

**Recommended:** Use the MRS DLL for all flash operations. It handles dual-core addressing correctly and does not trigger the WCH-Link second-init bug.

```powershell
.\wch-auto.ps1 -Action flash -ProjectDir <project> -Chip CH32H417 -Core both
```

The skill defaults to MRS DLL flash. For dual-core it flashes V3F first (address `0x08000000`, erase), then V5F (address `0x08010000`, no erase, reset run). The V5F reset boots V3F, which calls `NVIC_WakeUp_V5F` to start the secondary core cleanly.

### MRS DLL Flash Addresses

The skill passes the correct base addresses for dual-core:
- V3F image: `0x08000000`
- V5F image: `0x08010000`

This matches the Intel hex segment base in the V5F hex file (`:020000021000EC`).

### OpenOCD Flash (fallback only)

Only use OpenOCD flash if MRS DLL is unavailable (`-FlashTool openocd`). Be aware that OpenOCD flash exits the process, which triggers the second-init bug described below.

For a complete dual-core firmware via OpenOCD:

```powershell
openocd.exe -s <openocd-bin> -f wch-dual-core.cfg `
  -c "init" `
  -c "targets wch_riscv.cpu.1" -c "halt" -c "program <v5f.elf> verify" `
  -c "targets wch_riscv.cpu.0" -c "halt" -c "program <v3f.elf> verify" `
  -c "resume" -c "exit"
```

## Debug Sequence

The safe flow treats OpenOCD as a single long-lived daemon:

```powershell
.\wch-auto.ps1 -Action debug-check -ProjectDir <project> -Chip CH32H417 -Core v3f
.\wch-auto.ps1 -Action debug-check -ProjectDir <project> -Chip CH32H417 -Core v5f
```

- If an OpenOCD daemon is already running, the skill **reuses** it instead of starting a second instance.
- If no daemon is running, start it outside the script and keep it open, or pass `-StartOpenOCD` explicitly.
- The skill uses `monitor halt` by default. `reset halt` requires `-AllowTargetReset`.

For manual GDB connection to an already-running daemon:

```powershell
riscv-wch-elf-gdb.exe <v3f.elf> -ex "target remote localhost:3333"
riscv-wch-elf-gdb.exe <v5f.elf> -ex "target remote localhost:3334"
```

For noninteractive validation, avoid `continue`; it blocks forever in normal embedded loops. Prefer:

```gdb
target remote localhost:<port>
monitor halt
info registers pc sp gp
quit
```

Check V3F and V5F sequentially, not in parallel, because both sessions share one WCH-Link and one OpenOCD server port set. Avoid V3F `reset halt`; it can stop the boot core before it wakes V5F. If V5F PC is `0x00000000`, resume V3F or re-run the board so V3F can wake the secondary core, then check V5F with `monitor halt`.

## MounRiver IDE Debug Options

The EVT `.wvproj` files show how MounRiver configures OpenOCD per core:

| Core | OpenOCD extra options | skipDownloadBeforeDebug |
|------|----------------------|------------------------|
| V3F  | `-c noload`          | `true`                 |
| V5F  | `-c page_erase`      | `false`                |

When `-StartOpenOCD` is explicitly used, the skill applies these same options when it starts OpenOCD for each core:
- **V3F**: `-c noload` prevents OpenOCD from reloading the image before debug (the V3F image is already in flash).
- **V5F**: `-c page_erase` enables page erase for the secondary core.

MounRiver debug settings also use:
- V3F: `initResetType: init`, `runResetType: halt`, `setBreakAt: BP`
- V5F: `initResetType: init`, `runResetType: halt`, `setBreakAt: handle_reset`

## Known Behaviours

### OpenOCD `Unsupported register` warning

During dual-core flash you may see:

```
Error: Unsupported register (enum gdb_regno)(8)
```

This is a benign OpenOCD quirk on the WCH RISC-V target. Programming and verification still succeed (`Programming Finished`, `Verified OK`). Do not treat this as a failure.

### V5F shows PC = 0x00000000

If you connect to V5F before V3F has run, V5F has not yet been woken. Its PC reads `0x00000000` and SP/GP may show default reset values. This is expected. Resume V3F or re-run the board so V3F executes `NVIC_WakeUp_V5F`, then check V5F again.

### V3F STOP mode destabilises OpenOCD

When V3F enters `PWR_EnterSTOPMode` (which uses `__WFE`), the debug module may become unresponsive. Specific symptoms:

- `openocd ... -c "reset run"` crashes with assertion failure or `dmstatus=0xffffffff`
- Sequential GDB connections to V3F then V5F can trigger "Debugger is not authenticated" errors
- `monitor reset halt` on a sleeping V3F may report a stale PC instead of the reset vector

**Workarounds:**
- Prefer MRS DLL flash (`-FlashTool mrs`) over OpenOCD when V3F uses STOP mode; the MRS helper recovers from bad debug-module states automatically
- Keep one OpenOCD daemon alive; do not repeatedly start and stop it.
- For debug-check after STOP mode, expect `info registers` to show the halted execution address (e.g. `__WFE`) rather than the reset vector. Use `halt`, not `reset halt`, unless `-AllowTargetReset` is intentional.

### OpenOCD cannot be restarted after exit

A severe WCH-LinkE firmware limitation affects OpenOCD 0.11.0+dev-snapshot shipped with MRS2:

- The **first** OpenOCD process after a fresh WCH-Link plug-in can `init` and operate normally.
- Once that OpenOCD process **exits**, a subsequent OpenOCD process will fail during `wlink_init()` with `WCH-Link failed to connect with riscvchip`.
- This happens **even if V3F is running** (not in STOP mode) and even after stale-process cleanup and multi-second delays.
- Only **re-plugging WCH-Link** restores the ability to start a new OpenOCD session.
- MRS DLL `set-line-mode` / `reset` cannot recover from this state either.

**Practical impact:**
- `flash` (OpenOCD) followed by `debug-check` (OpenOCD) will always fail the second step.
- MRS DLL flash does not suffer from this, but MRS DLL itself can later fail with error 104 if OpenOCD has already triggered the bug.

**Skill workaround:**
The skill defaults to MRS DLL for all flash operations and does not fall back to OpenOCD unless `-AllowOpenOCDFlash` is explicitly passed. For `debug-check`, it reuses an existing OpenOCD daemon; starting OpenOCD from the script requires `-StartOpenOCD`.

If OpenOCD does fail to start or the GDB port does not open, stop the automation flow. Do not auto-run `reset-link`, `RecoverMode`, or OpenOCD fallback. Recover with MounRiver or another known-good external tool, then continue with one long-lived OpenOCD daemon.

**Recommended workflow:**
1. Flash with MRS DLL: `.\wch-auto.ps1 -Action flash -Core both`
2. Start one OpenOCD daemon externally and keep it open.
3. Debug-check reuses the daemon: `.\wch-auto.ps1 -Action debug-check -Core v3f`
4. Subsequent debug-checks connect to the same daemon directly.

For projects where V3F uses STOP mode, MRS flash is essential because an OpenOCD daemon that loses connection to a sleeping V3F may also become unusable.

## Startup Caveat

Keep the official WCH startup files. H417 startup copies code into RAM_CODE, configures flash acceleration, initializes global pointer/CSR state, and then enters C code. Minimal XIP startup is not a safe replacement for these examples.

## Clock Configuration Caveat

On some boards, `SystemInit()` with HSE-based PLL configuration hangs waiting for HSE ready. If this occurs, switch to the HSI-based variant in `system_ch32h417.c`:

```c
// #define SYSCLK_400M_CoreCLK_V5F_400M_V3F_100M_HSE    400000000
#define SYSCLK_400M_CoreCLK_V5F_400M_V3F_100M_HSI    400000000
```

Removing `SystemInit()` entirely is unsafe because Flash wait states (`FLASH_ACTLR_LATENCY_HCLK_DIV2`) are not configured, causing instruction-fetch corruption at 100 MHz PLL speed.
