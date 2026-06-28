# CH585 Hardware Tests

Standalone CH585 board bring-up tests.

## ADS7948/MUX Probe

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
make -C hw_tests/ch585 TEST=ch585_ads7948_mux_probe DEFS_EXTRA="-DCH585_ADS7948_PROBE_KEY=58"
make -C hw_tests/ch585 TEST=ch585_ads7948_mux_probe DEFS_EXTRA="-DCH585_ADS7948_PROBE_KEY=0xFF"
make -C hw_tests/ch585 TEST=ch585_ads7948_mux_probe DEFS_EXTRA="-DCH585_ADS7948_PROBE_RIGHT_HALF=0"
```

Serial output lines begin with `AP` and include sequence, key, lane, mux,
raw ADC, filtered ADC, down state, flags, ADC error count, and scan status.
