# CH585 Host Tools

This directory contains host-side helpers for CH585 firmware targets.

## `read_rf8k_usbhs.py`

Reads the USBHS custom HID stats report emitted by the CH585 RF receiver.

Example:

```powershell
python firmware/ch585/tools/read_rf8k_usbhs.py --timeout-ms 1000 --duration 5
```

The script expects the receiver firmware to expose the USBHS custom HID
interface added in this branch.

## `ch585_half_scan_uart_monitor.py`

Fixed-position UART TUI for the `half_scan_*_uart_diag` firmware targets.
It parses the `half_scan start ...`, `hs ...`, and `half_scan rf tx ...`
diagnostic lines and shows scan state, trigger summary, down bits, SPI link state,
and RF TX counters in one screen.

Build a UART diagnostic image:

```powershell
make -C firmware/ch585 half_scan_left_uart_diag
make -C firmware/ch585 half_scan_right_uart_diag
```

Run the TUI:

```powershell
python -m pip install pyserial
python firmware/ch585/tools/ch585_half_scan_uart_monitor.py --port COM7 --baud 921600
```

If Python imports the unrelated `serial` package instead of `pyserial`, clean
that environment and reinstall `pyserial`:

```powershell
python -m pip uninstall serial
python -m pip install --force-reinstall pyserial
```

For log-style capture instead of an alternate-screen TUI:

```powershell
python firmware/ch585/tools/ch585_half_scan_uart_monitor.py --port COM7 --baud 921600 --raw
```
