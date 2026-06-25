# Port Local Tuned Code To Current Hardware Layout

Date: 2026-06-25

Branch:

```text
codex/port-local-tuned-code-20260625
```

Base:

```text
origin/main @ 8294028 feat(h417): add display and flash drivers
```

Source workspace:

```text
F:\嵌赛\hardware
branch: codex/h417-ch585-bringup-v2
```

Target workspace:

```text
F:\嵌赛\hardware_origin_main_latest
```

This branch ports the local H417/CH585 bring-up code into the latest `hardware`
repository layout. The original `F:\嵌赛\hardware` working tree was not modified.

## H417 V5F RT-Thread

Old path:

```text
rtthread_port/
```

New path:

```text
firmware/h417/v5f_rtthread/
```

Ported files:

```text
rtthread_port/applications/main.c
  -> firmware/h417/v5f_rtthread/applications/main.c

rtthread_port/applications/ch585_spi_scan.c
  -> firmware/h417/v5f_rtthread/applications/ch585_spi_scan.c

rtthread_port/applications/ch585_spi_scan.h
  -> firmware/h417/v5f_rtthread/applications/ch585_spi_scan.h

rtthread_port/applications/usb_cdc_dual.c
  -> firmware/h417/v5f_rtthread/applications/usb_cdc_dual.c

rtthread_port/applications/keyboard_engine.c
  -> firmware/h417/v5f_rtthread/applications/keyboard_engine.c

rtthread_port/applications/keyboard_engine.h
  -> firmware/h417/v5f_rtthread/applications/keyboard_engine.h

rtthread_port/applications/usb_hs_hid_keyboard.c
  -> firmware/h417/v5f_rtthread/applications/usb_hs_hid_keyboard.c

rtthread_port/applications/usb_hs_hid_keyboard.h
  -> firmware/h417/v5f_rtthread/applications/usb_hs_hid_keyboard.h

rtthread_port/applications/ch585_ble_bridge.c
  -> firmware/h417/v5f_rtthread/applications/ch585_ble_bridge.c

rtthread_port/applications/ch585_ble_bridge.h
  -> firmware/h417/v5f_rtthread/applications/ch585_ble_bridge.h
```

The latest `firmware/h417/v5f_rtthread/Makefile` was kept as the base because
its relative paths match the current tree. It was updated to include:

```text
APP_ENABLE_USB2_HS_HID
APP_ENABLE_CH585_BLE_BRIDGE
APP_MAIN_LOOP_DELAY_MS
DEFS_EXTRA
applications/ch585_ble_bridge.c
applications/keyboard_engine.c
applications/usb_hs_hid_keyboard.c
CherryUSB HID class source/include
```

## H417 V3F ADC Carry-Forward

Old path:

```text
rtthread_port/v3f_wakeup/adc.c
rtthread_port/v3f_wakeup/adc.h
```

New path:

```text
firmware/h417/v3f/drivers/hall_adc/adc.c
firmware/h417/v3f/drivers/hall_adc/adc.h
```

These files are preserved so the old V3F Hall ADC prototype is not lost. They
are not yet wired into the current V3F Makefile.

## CH585 Frontend And Magnetic Algorithm

Old paths:

```text
firmware/ch585_frontend/
firmware/common/
```

New paths:

```text
firmware/ch585/frontend/
firmware/ch585/common/
```

Ported files:

```text
firmware/ch585_frontend/ads7948.c
  -> firmware/ch585/frontend/ads7948.c

firmware/ch585_frontend/ads7948.h
  -> firmware/ch585/frontend/ads7948.h

firmware/ch585_frontend/ch585_ads7948_mux_scan.c
  -> firmware/ch585/frontend/ch585_ads7948_mux_scan.c

firmware/ch585_frontend/ch585_ads7948_mux_scan.h
  -> firmware/ch585/frontend/ch585_ads7948_mux_scan.h

firmware/common/magnetic_key_engine.c
  -> firmware/ch585/common/magnetic_key_engine.c

firmware/common/magnetic_key_engine.h
  -> firmware/ch585/common/magnetic_key_engine.h
```

Architecture note:

```text
Magnetic switch algorithm ownership is CH585-side.
H417 should normally receive key state/events/debug data, not high-rate raw ADC.
```

## CH585 SPI Slave Test

Old path:

```text
firmware/ch585_spi_slave_test/
```

New path:

```text
firmware/ch585/spi_slave_test/
```

Ported files:

```text
firmware/ch585_spi_slave_test/Makefile
firmware/ch585_spi_slave_test/README.md
firmware/ch585_spi_slave_test/src/main.c
```

Makefile path changes:

```text
SDK_ROOT      ?= ../basic/wch/SRC
INC          += -I../frontend
C_SOURCES    += ../frontend/ads7948.c
```

Copied build artifacts under `firmware/ch585/spi_slave_test/build/` are ignored
by `.gitignore` and should not be committed.

## CH585 Legacy Projects

Old path:

```text
firmware/ch585_legacy/
```

New path:

```text
firmware/ch585/legacy/
```

This preserves the local BLE/KVM/RF bring-up assets and the dirty local changes
from the old branch, including:

```text
firmware/ch585/legacy/ble_kvm/src/BLE/ble_hid.c
firmware/ch585/legacy/ble_kvm/src/Main.c
firmware/ch585/legacy/ble_kvm/src/broadcaster.c
firmware/ch585/legacy/ble_kvm/src/Main_broadcaster_selftest.c
firmware/ch585/legacy/rf_rx/
firmware/ch585/legacy/rf_rx_usbfs/
firmware/ch585/legacy/rf_rx_usbhs/
firmware/ch585/legacy/rf_tx/
```

Ignored local build output under `obj*/` was copied into the worktree but is
ignored and should not be committed.

## CH585 Host Tools

Old path:

```text
tools/ch585/
```

New path:

```text
firmware/ch585/tools/
```

Ported files:

```text
firmware/ch585/tools/README.md
firmware/ch585/tools/usb_cdc_console.py
```

## Bring-Up Docs

Old path:

```text
docs/bringup/
```

New path:

```text
docs/bringup/
```

The latest `origin/main` did not keep the old bring-up docs, so the local
bring-up docs were restored under `docs/bringup/`. The old root README content
from the local branch was preserved here:

```text
docs/bringup/archive/h417_ch585_branch_root_readme.md
```

Additional architecture carry-forward:

```text
docs/architecture/project_architecture.md
docs/architecture/profile_config_skeleton.md
```

## Main Cleanup Check

`firmware/h417/v5f_rtthread/tools/check_main_clean.py` is a mainline cleanup
guard. This branch intentionally contains bring-up/test code, so that script is
not expected to pass without further policy changes.

Before pushing, use:

```powershell
git status --short
git diff --stat
```

Push target should be a feature branch, not `main`.
