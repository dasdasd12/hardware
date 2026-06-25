# CH585 BLE / 2.4G RF Archive

This folder collects historical CH585M BLE, 2.4G RF, USB FS, USBHS, and KVM
bring-up notes from the outer workspace.

These notes are preserved for traceability. They may mention old paths such as
`F:\嵌赛\CH585M` or `F:\嵌赛\CH585M_RF_TX`. The corresponding source snapshots
inside this repository are now under:

```text
firmware/ch585_legacy/
```

## Note Index

| File | Topic |
| --- | --- |
| `ch585m_readme.md` | Original CH585M work log and project status. |
| `ch585m_ble_hid_status.md` | BLE HID status notes. |
| `ch585m_ble_module_usage.md` | BLE module usage notes. |
| `ch585m_ble_virtual_serial_demo.md` | BLE virtual serial demo notes. |
| `ch585m_usb_cdc_debug.md` | CH585 USB CDC debug notes. |
| `ch585m_kvm_status.md` | KVM / multi-device switching status. |
| `rf_2g_bringup.md` | 2.4G TX/RX bring-up procedure. |
| `rf_2g_success_status.md` | Known successful 2.4G debug status. |
| `ch585m_rf_rx_usb.md` | 2.4G receiver + USB FS notes. |
| `ch585m_rf_rx_usbhs.md` | 2.4G receiver + USBHS notes. |
| `kvm_multi_device_plan.md` | Earlier multi-device/KVM plan. |
| `keyboard_spi_architecture_early.md` | Earlier CH585M -> H417 SPI architecture proposal. |
| `repos_overview_early.md` | Earlier outer-workspace repository overview. |

## Relationship To The Current SPI Work

Current verified chain:

```text
CH585 simulated ADC / local key algorithm
  -> CH585 KEY_STATE + KEY_DEBUG short frames
  -> H417 hardware SPI2 + GPIO CS
  -> H417 USBFS CDC debug output
```

The BLE/RF archive is not connected to that chain yet. It is the reference base
for future wireless reporting or multi-mode work after the wired CH585 front-end
and H417 USB reporting path are stable.
