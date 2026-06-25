# H417 + CH585 Bring-up Index

This folder contains the active H417 / CH585 keyboard front-end bring-up notes.

## Read First

1. [Current debug status](h417_ch585_current_debug.md)
2. [Next steps](h417_ch585_next_steps.md)
3. [CH32H417 dual-core flashing guide](h417_mounriver_dual_core_flash.md)

## Current Verified Point

The current verified debug chain is:

```text
CH585 simulated ADC / local key algorithm, or ADS7948 probe mode
  -> CH585 KEY_STATE + low-rate KEY_DEBUG
  -> H417 PCB hardware SPI1 + GPIO CS
  -> H417 USBFS CDC COM5 logs
```

Current PCB SPI wiring from the contest report:

```text
H417 PB3/SPI1_SCK  -> CH585 PA13/SPI0_SCK
H417 PB5/SPI1_MOSI -> CH585 PA14/SPI0_MOSI
H417 PB4/SPI1_MISO <- CH585 PA15/SPI0_MISO
H417 PD9 GPIO CS   -> right CH585 PA12/SPI0_CS
H417 PF2 GPIO CS   -> left CH585 PA12/SPI0_CS, held high during right-only test
```

The old dev-board SPI2 wiring remains as a compile-time fallback only.

## Debug Without H417 USB

If H417 USB is not connected, use CH585 UART1 as a telemetry bridge. Build
the CH585 SPI slave firmware with:

```powershell
make DEFS_EXTRA="-DCH585_SPI_UART_TELEMETRY=1"
```

Then flash:

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

UART wiring:

```text
CH585 PA9 / UART1 TX -> USB-TTL RX
CH585 PA8 / UART1 RX <- USB-TTL TX, optional
CH585 GND            -> USB-TTL GND
115200 8N1
```

The telemetry closes the loop through SPI:

```text
CH585 sends frame seq=N
  -> H417 accepts it
  -> H417 sends next command with ack_seq=N
  -> CH585 prints ack_ok=1 over UART1
```

Useful UART lines:

```text
SP ST  ... ack_ok=... ack_miss=... sent=N host=H host_ack=N ack=1 sck=50.00MHz ...
SP BAD ... bad command or CRC on the H417->CH585 command path
SP KEY ... down_bits changed
```

`sck=` is returned by H417 in the short command reserved bytes. The unit on
the wire is 10 kHz, and CH585 UART prints it as MHz. This lets us confirm the
actual H417-selected SPI training speed without connecting H417 USB.

Current stable SPI fallback mode is command-response short frame:

```text
H417: APP_CH585_SPI_REQUEST_ONLY_SHORT=0
H417: APP_CH585_SPI_HW_SPI2_PRESCALER=SPI_BaudRatePrescaler_Mode4
H417: APP_CH585_SPI_HW_SPI2_HIGHSPEED=2
H417: CPHA=1Edge
CH585: CH585_USE_REQUEST_ONLY_SHORT=0
```

This reports `sck=16.67MHz` on CH585 UART and was observed stable on the PCB
with `cmd_bad=0`, continuous `ack=1`, and no new `ack_miss`. Higher-speed
observations: `25.00MHz`, `20.00MHz/HSRX2`, and `20.00MHz/HSRX1` kept the
H417->CH585 command path mostly clean but missed most CH585->H417 ACKs.
`20.00MHz/HSRX1/CPHA2` made the command path worse. The request-only short
path is not currently considered validated: CH585 captured `0xFF` on the
command bytes during the first PCB test.

Expected COM5 lines include:

```text
KD n=2 seq=112 k=2 raw=999 filt=1055 pos=27 peak=490 down=0 rt=1
SS hb=9 s0ok=9 s0fetch=0 s0crc=0 s0seq=0 sck=16000.0 ...
KS f=0 i=000 1000 1000 3000 3000 ...
```

## Current Build Setting

For USBFS debug output, the H417 RT-Thread build currently uses:

```text
APP_ENABLE_USB2_HS_CDC=0
APP_ENABLE_USB2_FS_CDC=1
```

If COM5 does not appear, check `firmware/h417/v5f_rtthread/Makefile` first.

## Reference Notes

- [ADS7948 / MUX / magnetic notes](ads7948_mux_magnetic_notes.md)
- [CH585 ADS7948 single-key probe](ch585_ads7948_single_key_probe.md)
- [Right half to BLE bridge plan](right_half_to_ble_bridge_plan.md)
- [CH585 SPI slave test firmware](../../firmware/ch585/spi_slave_test/README.md)
- [CH585 legacy BLE / RF snapshots](../../firmware/ch585_legacy/README.md)
- [CH585 BLE / 2.4G RF archive notes](archive/ch585_ble_rf/README.md)
- [Archive of older SPI / USB experiments](archive/)

## Archive Policy

Files in `archive/` are old stage notes. They are preserved for traceability, but may describe wiring, protocol variants, or USBHS experiments that are not the current default path.
