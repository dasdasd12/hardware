---
name: wch-mrs-automation
description: CH32H417-dedicated automation for MounRiver Studio 2, WCH-Link, OpenOCD, and riscv-wch-elf/riscv32-wch-elf toolchains. Handles dual-core V3F/V5F build, flash, and debug flows; generates Makefiles and VS Code/MRS tasks; diagnoses WCH-Link, OpenOCD, GDB, or MRS project issues.
---

# WCH MRS Automation (CH32H417)

Use the bundled PowerShell script first:

```powershell
.\scripts\wch-auto.ps1 -Action detect -ProjectDir <project>
.\scripts\wch-auto.ps1 -Action detect -ProjectDir <project> -Diagnose
.\scripts\wch-auto.ps1 -Action init -ProjectDir <project>
.\scripts\wch-auto.ps1 -Action build -ProjectDir <project> -Core both
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <project> -Core both
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <project> -Core both -DryRun
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <project> -Core both -ImagePathV3F <v3f.hex> -ImagePathV5F <v5f.hex>
.\scripts\wch-auto.ps1 -Action debug-check -ProjectDir <project> -Core v3f
.\scripts\wch-auto.ps1 -Action debug -ProjectDir <project> -Core v5f
.\scripts\wch-auto.ps1 -Action recover -ProjectDir <project>
.\scripts\wch-auto.ps1 -Action reset-link
```

Every run writes a session log under `<project>\.wch-skill-logs\session-YYYYMMDD-HHmmss-fff-PID.log` plus a paired `mrs-trace-*.log` with the return code and duration of every MRS DLL call. When flash fails, read those logs first. If `<project>\.wch-skill-logs\flash-failure-lockout.json` exists, do not retry `flash` blindly.

## CH32H417 Dual Core

Prefer the official EVT layout:

```text
<project>\
  Common\
  V3F\User\
  V5F\User\
```

The CH32H417 EVT examples use this boot/debug mapping:

- V3F uses `startup_ch32h417_v3f.S`, `Link_v3f.ld`, flash origin `0x00000000`, OpenOCD target `wch_riscv.cpu.0`, GDB port `3333`.
- V5F uses `startup_ch32h417_v5f.S`, `Link_v5f.ld`, flash origin `0x00010000`, OpenOCD target `wch_riscv.cpu.1`, GDB port `3334`.
- V3F is the boot/wake coordinator. It wakes V5F with `NVIC_WakeUp_V5F(Core_V5F_StartAddr)`.
- Flash both cores with the official MRS order: V3F first at `0x08000000` with Erase All, then V5F at `0x08010000` without Erase All, then reset/run.
- A V5F-only image usually does not start by itself after reset unless a V3F image wakes it.

For more detail, read `references/h417-dual-core.md`.

## Workflow

1. Run `detect`; add `-Diagnose` for a full link/RDP probe.
2. If the project has no command-line Makefile, run `init`. Pass `-EVTRoot` if the EVT package is not at `C:\program1\hardware\WCH\CH32H417\CH32H417EVT\EVT\EXAM`.
3. Run `build`.
4. Run `flash -DryRun` to inspect the command, then run `flash` only when overwriting the connected board is intended. Prefer explicit `-ImagePathV3F` and `-ImagePathV5F` so stale `.hex` files cannot be selected by timestamp search.
5. Use `debug-check` for noninteractive register/connection validation. Use `debug` for an interactive GDB session.
6. If a flash/debug step reports a WCH-Link or RDP communication failure, stop and inspect `.wch-skill-logs`. Do not retry normal flash after a lockout.

## Flash Defaults

- `-ClkSpeed` defaults to `Middle` (~3 MHz SDI). `High` was marginal at 400 MHz core clock on PB8/PB9.
- The MRS operation byte mirrors the MRS GUI EVT defaults: full chip erase + program + verify; read-protect is not touched unless you pass `-DisableCodeProtect`.
- Pass `-NoEraseAll` to skip full erase; the dual-core flow already does this internally for the V5F image so it does not wipe V3F.
- Before programming, `flash` runs an MRS `dump-link-status` preflight. If chip ID/RDP cannot be read safely, it writes `flash-failure-lockout.json` and refuses to program.
- Normal `flash` does not auto-add `clear-code-flash` (`0x40`) after `ret=104`. Treat `104`, `110`, or `120` during flash as a lock-risk signal: stop, inspect logs, and ask the user before recovery.
- `flash` no longer kills a live OpenOCD daemon. MRS DLL flash coexists with a running OpenOCD; killing it forced a fresh OpenOCD on the next `debug-check`, which triggered the second-init lock.
- `flash -RecoverMode` stays on the MRS DLL path: on MRS flash failure it runs `recover`, then retries the same MRS flash sequence once. It does not fall back to OpenOCD automatically. Use this only when the user has explicitly approved destructive recovery.
- `debug-check` no longer runs `Invoke-MRSSetTarget` / `Invoke-MRSReset` immediately before launching OpenOCD. Pass `-AllowMrsReset` only when V3F has entered STOP mode and OpenOCD `init` is failing.
- `mrs-link.ps1` no longer calls `SetSDLineMode` on every flash; it is only invoked by the explicit `set-line-mode` action.

## Reset Behaviour

- `reset-link` resets or rehandshakes the WCH-Link adapter. It is not a target-chip reset.
- `mrs-link.ps1 -Action reset` calls the MRS DLL `McuCompiler_ResetB` command. A return code of 0 only proves the DLL command completed; verify the target really restarted using heartbeat output, NRST voltage, or debug state.
- `mrs-link.ps1 -Action hold-nrst` / `release-nrst` controls the WCH-LinkE target RST output. It only resets CH32H417 if the Link RST pin is physically connected to the target NRST net.
- The normal `flash` path does not use `hold-nrst` / `release-nrst` as an automatic retry. Use target NRST as a manual diagnostic step, then run `detect -Diagnose`; do not keep flashing after a lockout.

## Lock Recovery

If `flash-failure-lockout.json` exists, normal `flash` will refuse to run. Read the paired session and trace logs, report the last MRS return code, and ask before running `recover`. Do not delete the lockout file unless the hardware was recovered manually or `recover` completed successfully.

Recovery sequence after user approval:

```powershell
.\scripts\wch-auto.ps1 -Action detect -ProjectDir <project> -Diagnose
.\scripts\wch-auto.ps1 -Action recover -ProjectDir <project>
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <project> -Core both
```

`-Action recover` runs: MRS rehandshake, USB PnP cycle only if rehandshake fails, `query-rprotect`, `disable-rprotect`, forced full-chip erase via a 4-byte stub `.bin`, and post-erase link/RDP verification. Its full call sequence is captured in the trace log.

Use `recover` only for lock/recovery cases. It is intentionally destructive: it clears read protection when possible and performs a full-chip erase.

## Known Behaviours

- Default flash uses MRS DLL, not OpenOCD, for all cores including dual-core.
- `debug-check` reuses an existing OpenOCD daemon or starts one and leaves it running. Do not manually kill OpenOCD between flash and debug unless needed.
- During dual-core OpenOCD work, `Error: Unsupported register (enum gdb_regno)(8)` is usually benign when programming and verification still succeed.
- V5F debug-check needs V3F awake. After a V3F `reset halt`, V5F can sit at `0x00000000` because it was never woken.
- OpenOCD flash is fallback only. Use `-FlashTool openocd` only when the MRS DLL path is unavailable.
- High-risk switches: do not use `-DisableCodeProtect`, `-ClearCodeFlash`, `-DisablePowerOut`, `-NoVerify`, or `-NoReset` unless the hardware state requires it and the consequence is understood.

## Common Fixes

- If OpenOCD cannot open WCH-Link, check the cable, target power, and WCH-Link mode.
- If OpenOCD fails with `WCH-Link failed to connect with riscvchip`, run `detect -Diagnose` and inspect logs before recovery.
- If port `3333` or `3334` is busy, stop old OpenOCD/GDB processes.
- If a copied EVT project cannot find `SRC`, run `init` with the correct `-EVTRoot`.
- If an H417 V5F image verifies but does not run, also flash/run the V3F image that wakes V5F.
- Use official WCH startup and linker scripts. Do not replace H417 startup with a minimal XIP startup.
- After any unexpected lock, attach the matching `.wch-skill-logs/session-*.log`, `mrs-trace-*.log`, and any `flash-failure-lockout.json` to the report.

## Maintenance Checks

After editing this skill, run:

```powershell
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\tests\verify-wch-skill.ps1
```

The check guards known lock-risk regressions: stale V5F-first wording, unchecked rehandshake success, `RecoverMode` falling back to OpenOCD, recover claiming success after failed erase, normal flash auto-adding `clear-code-flash`, missing flash lockout, and MRS debug template drift.
