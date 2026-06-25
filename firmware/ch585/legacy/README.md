# CH585 Legacy BLE / RF Snapshots

This folder archives the earlier CH585M BLE, 2.4G RF, USB FS, USBHS, and KVM
experiments. They are useful references, but they are not the current
H417 <-> CH585 SPI front-end firmware.

## Current Role

The current active CH585 path is:

```text
firmware/ch585/spi_slave_test
  -> H417 hardware SPI2 + GPIO CS
  -> H417 USBFS CDC debug output
```

The projects in this folder are historical/reference snapshots for wireless and
USB-side experiments.

## Contents

| Folder | Source snapshot | Purpose |
| --- | --- | --- |
| `ble_kvm/` | `F:\嵌赛\CH585M` | BLE HID, USB CDC debug console, and KVM command experiments. |
| `rf_tx/` | `F:\嵌赛\CH585M_RF_TX` | 2.4G RF_Basic transmit-side experiment. |
| `rf_rx/` | `F:\嵌赛\CH585M_RF_RX` | 2.4G RF_Basic receive-side experiment. |
| `rf_rx_usbfs/` | `F:\嵌赛\CH585M_RF_RX_USB` | 2.4G receive path with USB FS reporting experiment. |
| `rf_rx_usbhs/` | `F:\嵌赛\CH585M_RF_RX_USBHS` | 2.4G receive path with USBHS HID experiment. |

## What Was Kept

- MounRiver project metadata: `.project`, `.cproject`, `.template`, launch or
  project files where present.
- Hand-written source under `APP/`, `src/`, `HAL/`, `Startup/`,
  `StdPeriphDriver/`, `RVMSIS/`, `Ld/`, and `LIB/` where present.
- The CH585M USB CDC Python debug console.

## What Was Excluded

- `obj/`, `build/`, `.mrs/`, generated logs, and generated firmware images.
- Old compiled hex images should not be trusted; rebuild from source before
  flashing.

## Build Notes

`ble_kvm/` is a mostly self-contained CH585M working snapshot and includes the
local BLE support files that were present in the original directory.

The RF projects use MounRiver linked resources. Their `.project` files expect
the WCH CH585 SDK to be available next to the project workspace, for example:

```text
CHC585/EXAM/BLE/HAL
CHC585/EXAM/BLE/LIB
CHC585/EXAM/SRC/Ld
CHC585/EXAM/SRC/RVMSIS
CHC585/EXAM/SRC/Startup
CHC585/EXAM/SRC/StdPeriphDriver
```

If MounRiver shows invalid linked resources after importing these snapshots,
either place `CHC585` at the expected relative location or edit the linked
resource paths in MounRiver.

## Debug Tools

- `firmware/ch585/tools/usb_cdc_console.py`
  - Interactive CH585 USB CDC console.
- `firmware/h417/v5f_rtthread/tools/read_usbfs_scan.py`
  - Current H417 USBFS CDC reader for SPI scan/debug logs.

## Related Notes

Historical notes were copied to:

```text
docs/bringup/archive/ch585_ble_rf/
```
