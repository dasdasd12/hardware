# CH585 RF 8K Tools

This directory contains host-side helpers for the `rf_receiver_usbhs`
firmware target.

## `read_rf8k_usbhs.py`

Reads the USBHS custom HID stats report emitted by the CH585 RF receiver.

Example:

```powershell
python firmware/ch585/tools/read_rf8k_usbhs.py --timeout-ms 1000 --duration 5
```

The script expects the receiver firmware to expose the USBHS custom HID
interface added in this branch.
