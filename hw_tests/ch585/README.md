# CH585 hw_tests

## ADC/MUX serial TUI

Build and flash the ADC/MUX test for the target half:

```powershell
make -C hw_tests/ch585 TEST=ch585_adc_mux_scan HALF=left
make -C hw_tests/ch585 TEST=ch585_adc_mux_scan HALF=right
```

Open the fixed-position serial view:

```powershell
python hw_tests/ch585/tools/ch585_adc_mux_tui.py --port COM7
```

Use `--demo` to preview the TUI without hardware:

```powershell
python hw_tests/ch585/tools/ch585_adc_mux_tui.py --demo
```
