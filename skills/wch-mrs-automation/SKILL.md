---
name: wch-mrs-automation
description: CH32H417-dedicated automation for MounRiver Studio 2, WCH-Link, OpenOCD, and riscv-wch-elf/riscv32-wch-elf toolchains. Handles dual-core V3F/V5F build, flash, and debug flows; generates Makefiles and VS Code/MRS tasks; diagnoses WCH-Link, OpenOCD, GDB, or MRS project issues.
---

# WCH MRS Automation (CH32H417)

Use the bundled PowerShell script first:

```powershell
.\scripts\wch-auto.ps1 -Action detect -ProjectDir <project>
.\scripts\wch-auto.ps1 -Action init -ProjectDir <project>
.\scripts\wch-auto.ps1 -Action build -ProjectDir <project> -Core both
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <project> -Core both
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <project> -Core both -DryRun
.\scripts\wch-auto.ps1 -Action debug-check -ProjectDir <project> -Core v3f
.\scripts\wch-auto.ps1 -Action debug -ProjectDir <project> -Core v5f
```

The script discovers MounRiver Studio 2 under `C:\MounRiver\MounRiver_Studio2`, its WCH OpenOCD, GCC12/GCC15 toolchains, GDB, and bundled `make.exe`. It defaults to `CH32H417`; you do not need to pass `-Chip` unless overriding auto-detection.

## CH32H417 Dual Core

Prefer the official EVT layout:

```text
<project>\
  Common\
  V3F\User\
  V5F\User\
```

The CH32H417 EVT examples show this boot/debug mapping:

- V3F uses `startup_ch32h417_v3f.S`, `Link_v3f.ld`, flash origin `0x00000000`, OpenOCD target `wch_riscv.cpu.0`, GDB port `3333`.
- V5F uses `startup_ch32h417_v5f.S`, `Link_v5f.ld`, flash origin `0x00010000`, OpenOCD target `wch_riscv.cpu.1`, GDB port `3334`.
- V3F is the boot/wake coordinator in the EVT examples. It wakes V5F with `NVIC_WakeUp_V5F(Core_V5F_StartAddr)`.
- Flash both cores as two ELF files (V5F first, then V3F, then resume), then reset/run.
- A V5F-only image usually does not start by itself after reset unless a V3F image wakes it.

For more detail, read `references/h417-dual-core.md`.

## Workflow

## Safe Hardware Policy

The previous automation flow could deadlock WCH-LinkE by combining these actions: killing an existing OpenOCD daemon before MRS flash, falling back from MRS DLL flash to OpenOCD programming, starting/exiting OpenOCD repeatedly, using `reset halt` on V3F, and trying `reset-link` after failures. The default workflow now avoids that chain.

Safe defaults:

- MRS DLL is the only default flash path.
- MRS flash preserves any already-running OpenOCD/GDB processes.
- `debug` / `debug-check` attach to an already-running OpenOCD daemon and use `monitor halt`.
- `flash -DryRun` and `debug-check -DryRun` never touch the target.

Explicit opt-in only:

- OpenOCD flash or fallback requires `-AllowOpenOCDFlash`.
- Starting OpenOCD from the script requires `-StartOpenOCD`.
- Target reset / `reset halt` requires `-AllowTargetReset`.
- WCH-Link software reset requires `-AllowLinkReset`.
- Killing stale debug processes requires `-KillStaleDebuggers`.
- Stopping OpenOCD after debug requires `-StopOpenOCDAfterDebug`.

If MRS flash reports error `104`, stop. Do not run `reset-link`, `RecoverMode`, or OpenOCD fallback as automatic recovery. Recover the WCH-Link/target from MounRiver or another known-good external tool, then continue with MRS DLL flash and a single long-lived OpenOCD daemon.

1. Run `detect`.
2. If the project has no command-line Makefile, run `init`. Pass `-EVTRoot` if the EVT package is not at `C:\program1\hardware\WCH\CH32H417\CH32H417EVT\EVT\EXAM`.
3. Run `build`.
4. Run `flash -DryRun` to inspect the command, then run `flash` only when overwriting the connected board is intended.
5. Start one OpenOCD daemon outside the script if register checks are needed, and keep it open.
6. Use `debug-check` for noninteractive register/connection validation. It attaches and issues `halt`, not `reset halt`.

## Reference Commands

Build both H417 cores:

```powershell
.\scripts\wch-auto.ps1 -Action build -ProjectDir C:\program1\Program\H417lib\HSEM_CoreSync -Core both
```

Flash both H417 cores:

```powershell
.\scripts\wch-auto.ps1 -Action flash -ProjectDir C:\program1\Program\H417lib\HSEM_CoreSync -Core both
```

Attach/check V3F and V5F:

```powershell
.\scripts\wch-auto.ps1 -Action debug-check -ProjectDir <project> -Core v3f
.\scripts\wch-auto.ps1 -Action debug-check -ProjectDir <project> -Core v5f
```

Run core checks sequentially against one already-running OpenOCD daemon. The safe default uses `monitor halt`; use `-AllowTargetReset` only when a reset is intentionally required.

## Known Behaviours

- **Default flash uses MRS DLL** - The skill defaults to MRS DLL flash and does not fall back to OpenOCD unless `-AllowOpenOCDFlash` is explicitly passed.
- **OpenOCD daemon reuse** - `debug-check` reuses an existing OpenOCD daemon. It does not start OpenOCD unless `-StartOpenOCD` is explicitly passed.
- **OpenOCD `Unsupported register` warning** - During OpenOCD flash you may see `Error: Unsupported register (enum gdb_regno)(8)`. This is a benign OpenOCD quirk; programming and verification can still succeed.
- **V5F debug-check needs V3F awake** - If V5F PC is `0x00000000`, let V3F run so it can call `NVIC_WakeUp_V5F`; do not fix this with `reset halt`.
- **OpenOCD second-init bug** - After any OpenOCD process exits, a subsequent OpenOCD start can fail with `WCH-Link failed to connect with riscvchip`. Keep one daemon alive and avoid automatic reset/recovery paths.

## Common Fixes

- If OpenOCD cannot open WCH-Link, check the cable, target power, and WCH-Link mode.
- If OpenOCD fails with `WCH-Link failed to connect with riscvchip`, do not repeatedly restart it. Recover with MounRiver or another known-good external tool.
- If port `3333` or `3334` is busy, reuse the existing daemon when possible. Stop it only if you intentionally accept the second-init risk.
- If a copied EVT project cannot find `SRC`, run `init` with the correct `-EVTRoot`.
- If an H417 V5F image verifies but does not run, also flash/run the V3F image that wakes V5F.
- Use official WCH startup and linker scripts. Do not replace H417 startup with a minimal XIP startup.
