# H417 + CH585 Next Steps

Updated: 2026-06-21

This note records the current debug direction after the first SPI/USB bring-up.

## Current Situation

- One CH585 board is connected to H417 through hardware SPI2 plus GPIO CS.
- CH585 sends compact `down_bits[8]` key-state frames.
- H417 pulls CH585 frames and prints `KS` / `SS` / `TR` debug lines through USBFS CDC.
- USBFS CDC is currently the preferred debug port. On the current PC:
  - `COM5` is H417 USBFS CDC.
  - `COM4` is WCH-Link serial.
- ADS7948 / ADC hardware is not available yet.
- With DuPont wires, SPI speed is treated as a stable bring-up setting first; 40 MHz+ tuning should wait for PCB hardware and waveform checks.

## No-ADC Stage Plan

Before ADS7948 hardware arrives, keep the simulated front-end useful and observable:

```text
CH585 simulated ADC / local key algorithm
  -> down_bits[8] short frame
  -> H417 SPI pull
  -> USBFS COM5 KS/SS/TR logs
```

Acceptance criteria:

```text
s0ok keeps increasing
s0fetch = 0
s0crc = 0
s0seq does not keep increasing
KS shows the simulated keys toggling between 1000 and 3000
```

## Next Work

1. Add low-rate debug visibility for CH585 internal key algorithm state:

```text
key_id
sim/raw_adc
filtered_adc
position 0..1000
down
rt state
peak / valley
```

2. Keep the high-rate normal frame compact:

```text
down_bits[8] + flags + seq/ack + crc
```

3. Move CH585 key algorithm parameters into a clear config structure:

```text
released_adc
pressed_adc
press_position
release_position
rt_press_delta
rt_release_delta
filter_shift
rt_enable
```

4. Add H417 -> CH585 config/debug command prototypes:

```text
GET_STATE
GET_DEBUG
GET_CONFIG
SET_CONFIG
```

5. When ADC hardware arrives, replace simulated ADC in small steps:

```text
ADS7948 single channel -> key0
ADS7948 dual channel
single 16:1 MUX
4 lanes / 64 keys per CH585
second CH585
```
