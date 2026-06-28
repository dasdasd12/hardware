# CH585 Debug Tools

This directory contains host-side helpers for the CH585 firmware targets.

## `read_rf8k_usbhs.py`

Reads the USBHS custom HID stats report emitted by the CH585 RF receiver.

Example:

```powershell
python firmware/ch585/tools/read_rf8k_usbhs.py --timeout-ms 1000 --duration 5
```

The script expects the receiver firmware to expose the USBHS custom HID
interface added in this branch.

## `adc_text_scope.py`

Text-mode ADC dashboard for CH585 ADS7948 UART probe logs. It reads `AP ...`
lines from the CH585 UART probe and draws all 64 local ADC channels as
1024-normalized progress bars.

Run:

```powershell
python firmware/ch585/tools/adc_text_scope.py COM4 --side right
```

If Python does not have `pyserial`, use the dependency-free PowerShell version:

```powershell
powershell -ExecutionPolicy Bypass -File firmware/ch585/tools/adc_text_scope.ps1 -Port COM4 -Side right
```

The mapped-key view shows the local key number, lane/MUX channel, mapped label,
raw ADC value, `raw/1023` percentage, a 1024-normalized progress bar, baseline
drop, min/max, and `down`.

Numbering note:

- `Hxx` is the global Hall/key number from the hardware report.
- `Kxx` is the firmware-local physical MUX slot:
  `(MUX index - 1) * 16 + (D - 1)`.
- `LxDxx` is the physical MUX lane and data input. `D` is always `D01..D16`.
- The left half uses only `D01..D09` on each MUX. For example, left MUX1 maps
  `F5 F4 F3 F2 F1 Esc 6 5 4` to `L1D01..L1D09`, which are global
  `H42..H50` but local `K00..K08`.
- The right half uses `D01..D10` on MUX1..MUX3 and `D01..D11` on MUX4.

Use the all-channel view when searching for an unknown or missing route:

```powershell
powershell -ExecutionPolicy Bypass -File firmware/ch585/tools/adc_text_scope.ps1 -Port COM4 -Side right -View all
```

For a timed capture that exits by itself:

```powershell
python firmware/ch585/tools/adc_text_scope.py COM4 --side right --duration 10
powershell -ExecutionPolicy Bypass -File firmware/ch585/tools/adc_text_scope.ps1 -Port COM4 -Side right -Duration 10
```

The dashboard shows `drop = baseline - raw`; pressing a Hall key usually makes
the raw ADC value smaller, so the correct channel should jump to the top of the
`Top drops` list.
