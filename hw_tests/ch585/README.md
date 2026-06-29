# CH585 Hardware Tests

Standalone CH585 board bring-up tests.

## ADS7948/MUX Scan

Build:

```powershell
make -C hw_tests/ch585 TEST=ch585_ads7948_mux_probe
```

Default probe wiring:

```text
ADS7948 SPI1 SCK  -> PA0
ADS7948 SPI1 MOSI -> PA1
ADS7948 SPI1 MISO -> PA2
MUX SEL0..3       -> PB0..PB3
ADS7948 CH SEL    -> PB18
MUX/ADC PDEN      -> PB19
ADS7948 CS        -> PB14/PB15
UART log          -> UART1 PA9 TX / PA8 RX
```

Useful build overrides:

```powershell
make -C hw_tests/ch585 TEST=ch585_ads7948_mux_probe DEFS_EXTRA="-DCH585_ADC_MUX_HALF_RIGHT=1"
```

Serial output includes the active half profile, ADS7948 lane mapping, raw ADC
codes, travel percentage, and a compact text bar for each active MUX slot.
