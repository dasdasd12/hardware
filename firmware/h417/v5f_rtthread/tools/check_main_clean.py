#!/usr/bin/env python
from __future__ import print_function

import os
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir, os.pardir, os.pardir))
FIRMWARE_ROOT = "firmware/h417/v5f_rtthread"


REQUIRED_PATHS = [
    "firmware/h417/basic/wch/SRC/Core/core_riscv.h",
    "firmware/h417/basic/wch/SRC/Peripheral/inc/ch32h417.h",
    "firmware/ch585/applications/magnetic_key_engine.c",
    "firmware/ch585/applications/magnetic_key_engine.h",
    "firmware/ch585/applications/ble_kvm/main.c",
    "firmware/ch585/applications/rf_basic/main.c",
    "firmware/ch585/Makefile",
    "firmware/ch585/bsp/hal/MCU.c",
    "firmware/ch585/bsp/hal/include/HAL.h",
    "firmware/ch585/drivers/ads7948.c",
    "firmware/ch585/drivers/ads7948.h",
    "firmware/ch585/drivers/ble/ble_hid.c",
    "firmware/ch585/drivers/rf/RF_basic.c",
    "firmware/ch585/drivers/rf/include/rf_test.h",
    "firmware/ch585/drivers/usb/cdc_debug/usb_cdc_debug.c",
    "firmware/ch585/drivers/usb/usbhs_keyboard/usb_hid.c",
    "firmware/ch585/drivers/ch585_ads7948_mux_scan.c",
    "firmware/ch585/drivers/ch585_ads7948_mux_scan.h",
    FIRMWARE_ROOT + "/applications/ch585_spi_scan.c",
    FIRMWARE_ROOT + "/applications/ch585_spi_scan.h",
    FIRMWARE_ROOT + "/applications/keyboard_engine.c",
    FIRMWARE_ROOT + "/applications/keyboard_engine.h",
    FIRMWARE_ROOT + "/applications/usb_cdc_dual.c",
    FIRMWARE_ROOT + "/rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417_usbfs.c",
    FIRMWARE_ROOT + "/rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417_usbfs.h",
    FIRMWARE_ROOT + "/rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417_usbhs.c",
    FIRMWARE_ROOT + "/rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417_usbhs.h",
]


BANNED_PATHS = [
    "third_party/wch_ch32h417_evt",
    "ADS7948_MUX_MAGNETIC_AGENT_README.md",
    "H417_CH585_CURRENT_DEBUG_README.md",
    "H417_CH585_HW_SPI_GPIO_CS_README.md",
    "H417_CH585_NEXT_STEPS_README.md",
    "H417_CH585_SHORT_FRAME_PROTOCOL_README.md",
    "H417_CH585_SOFT_SPI_WIRING_README.md",
    "H417_CH585_SPI_TRAINING_STATUS_README.md",
    "H417_MOUNRIVER_DUAL_CORE_FLASH_README.md",
    "H417_SPI_STATUS_AND_USBHS_DEBUG_README.md",
    "H417_USBFS_SCAN_REPORT_README.md",
    "PROJECT_ARCHITECTURE_README.md",
    "README_H417_CH585_SPI_SCAN.md",
    "SPI_SPEED_DMA_DEBUG_README.md",
    "doc/usb3-init-failure-investigation.md",
    "docs/architecture/keyboard_engine_v3f.md",
    FIRMWARE_ROOT + "/doc/step4-v3f-usbss-pll-debug.md",
    "firmware/ch585_frontend/ads7948.c",
    "firmware/ch585_frontend/ads7948.h",
    "firmware/ch585_frontend/ch585_ads7948_mux_scan.c",
    "firmware/ch585_frontend/ch585_ads7948_mux_scan.h",
    "firmware/common/magnetic_key_engine.c",
    "firmware/common/magnetic_key_engine.h",
    "firmware/ch585/app",
    "firmware/ch585/common",
    "firmware/ch585/frontend",
    "firmware/ch585/legacy",
    "firmware/ch585/spi_slave_test",
    "firmware/h417/v3f/drivers/hall_adc",
    "skills/wch-mrs-automation/failure-logs/usbss-flash-lockout-retry-20260609-131731/serial-capture-latest.txt",
    "skills/wch-mrs-automation/failure-logs/usbss-flash-lockout-retry-20260609-131731/flash-failure-lockout.json",
    "skills/wch-mrs-automation/failure-logs/20260609-170426-v5f-wrong-address/flash-failure-lockout.json",
    "skills/wch-mrs-automation/failure-logs/usbss-flash-lockout-20260609-130545/flash-failure-lockout.json",
    "skills/wch-mrs-automation/failure-logs/usbss-flash-lockout-20260609-130545/serial-capture-latest.txt",
]


def repo_path(relative_path):
    return os.path.join(REPO_ROOT, *relative_path.split("/"))


def main():
    errors = []

    for relative_path in REQUIRED_PATHS:
        if not os.path.isfile(repo_path(relative_path)):
            errors.append("missing required file: %s" % relative_path)

    for relative_path in BANNED_PATHS:
        if os.path.exists(repo_path(relative_path)):
            errors.append("banned cleanup leftover: %s" % relative_path)

    if errors:
        for error in errors:
            print(error)
        return 1

    print("main cleanup checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
