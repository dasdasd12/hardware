# RF 8K, H417 SPI1, and CH585 ADC Probe Port - 2026-06-26

This note records the second local-code port onto the latest `hardware/main`
tree. The source worktree was `F:/嵌赛/hardware`, branch
`codex/h417-ch585-bringup-v2`. The target worktree is
`F:/嵌赛/hardware_rf8k_latest`, branch
`codex/rf8k-spi-adc-latest-20260626`, based on `origin/main` commit
`4b08421`.

## What Was Ported

- H417 V5F RT-Thread SPI scanner changes:
  - PCB SPI1 backend for final board pins.
  - `PD9`/`PF2` source select support.
  - Source0 `down_bits` treated as real key bits for the short frame.
  - SCK telemetry returned through the short command reserved bytes.
  - Debug print additions for key lane/mux and raw slot 58.
- CH585 USBHS RF receiver changes:
  - 2.4G stress frame `0xA8` parser.
  - 2.4G key-state frame `0xB1` parser.
  - Right-half `down_bits[8]` to USBHS NKRO conversion.
  - Double-buffered RF RX DMA handoff.
  - EP3 custom HID stats report for host-side rate/loss observation.
- CH585 RF keyboard transmitter changes:
  - 8 kHz key-state and stress frame modes.
  - H417-to-CH585 SPI bridge frame parser.
  - Right-half HID report to RF `down_bits[8]` conversion.
  - UART1 debug output on `PA9/PA8`.
- CH585 SPI slave / ADS7948 probe:
  - Kept as `firmware/ch585/applications/spi_slave_adc_probe`.
  - Uses `firmware/ch585/drivers/ads7948.c`.
  - Supports UART ADC probe and SPI short-frame telemetry modes.
- Tools and docs:
  - `firmware/ch585/tools/read_rf8k_usbhs.py`.
  - RF 8K USBHS bring-up notes.
  - ADS7948 single-key probe notes.
  - Right-half BLE bridge plan.

## New Layout

```text
firmware/h417/v5f_rtthread/
  Makefile
  applications/ch585_spi_scan.c
  applications/main.c

firmware/ch585/
  Makefile
  applications/rf_keyboard_tx/
  applications/rf_receiver_usbhs/
  applications/spi_slave_adc_probe/
  drivers/usb/usbhs_keyboard/
  tools/read_rf8k_usbhs.py
```

## CH585 Build Entry Points

The CH585 Makefile now keeps `rf_basic` as the default app and adds selectable
debug apps:

```bash
make -C firmware/ch585
make -C firmware/ch585 APP=rf_receiver_usbhs
make -C firmware/ch585 APP=rf_keyboard_tx
make -C firmware/ch585 APP=spi_slave_adc_probe
```

Use `DEFS_EXTRA=...` for debug-mode overrides, for example:

```bash
make -C firmware/ch585 APP=spi_slave_adc_probe DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1 -DCH585_ADC_PROBE_UART_MODE=1"
make -C firmware/ch585 APP=rf_keyboard_tx DEFS_EXTRA="-DRF_TX_MODE=RF_TX_MODE_STRESS_8K"
```

## Intentionally Not Reintroduced

The old `firmware/ch585_legacy/ble_kvm` application was not reintroduced as an
application because latest `origin/main` removed the KVM app and split reusable
CH585 code into `applications/`, `drivers/`, `bsp/`, and `basic/`. The BLE HID
driver-side debug name / clear-bonds changes were already present in latest
main. The removed `Main.c`, `broadcaster.c`, and
`Main_broadcaster_selftest.c` remain historical local experiments rather than
active firmware entry points.

## Checks

- No build output directories were copied.
- Old paths such as `firmware/ch585_legacy`, `firmware/ch585_spi_slave_test`,
  and `rtthread_port` were removed from the newly added bring-up notes.
- `read_rf8k_usbhs.py` should be checked with Python 3 (`py -3` on this
  Windows machine); plain `python` currently points to Python 2.7.
