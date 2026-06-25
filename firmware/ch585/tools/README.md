# CH585 Debug Tools

## `usb_cdc_console.py`

Interactive CH585 USB CDC console copied from the earlier `CH585M` bring-up
workspace.

Usage:

```powershell
python firmware/ch585/tools/usb_cdc_console.py COM5
```

Install `pyserial` first if Python cannot import `serial`:

```powershell
python -m pip install pyserial
```

Useful local commands:

```text
localhelp
after 3 tap b
afterfast 3 tap b; tap 1; tap enter
```

This tool is for CH585 CDC command experiments. For the current H417 USBFS CDC
SPI scan logs, use:

```powershell
python firmware/h417/v5f_rtthread/tools/read_usbfs_scan.py --port COM5
```
