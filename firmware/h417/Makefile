# CH32H417 dual-core firmware build entry.

PREFIX ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
BUILD_ROOT ?= build
V5F_MAKE_ARGS :=

ifneq ($(origin APP_V5F_HW_TEST),undefined)
V5F_MAKE_ARGS += APP_V5F_HW_TEST=$(APP_V5F_HW_TEST)
endif
ifneq ($(origin APP_ENABLE_USB_TEST),undefined)
V5F_MAKE_ARGS += APP_ENABLE_USB_TEST=$(APP_ENABLE_USB_TEST)
endif
ifneq ($(origin APP_ENABLE_CH585_SPI_SCAN),undefined)
V5F_MAKE_ARGS += APP_ENABLE_CH585_SPI_SCAN=$(APP_ENABLE_CH585_SPI_SCAN)
endif
ifneq ($(origin APP_ENABLE_SERIAL_HEARTBEAT),undefined)
V5F_MAKE_ARGS += APP_ENABLE_SERIAL_HEARTBEAT=$(APP_ENABLE_SERIAL_HEARTBEAT)
endif

.PHONY: all v3f V3F v3f_output_switch V3F_OUTPUT_SWITCH v5f V5F clean flash

all: v3f

v3f V3F:
	@$(MAKE) -C v3f PREFIX="$(PREFIX)" BUILD_ROOT=../$(BUILD_ROOT)/V3F

v3f_output_switch V3F_OUTPUT_SWITCH:
	@$(MAKE) -C v3f PREFIX="$(PREFIX)" BUILD_ROOT=../$(BUILD_ROOT)/V3F_output_switch PROJECT_NAME=h417_V3F_output_switch V3F_ENABLE_USBHS_8K=1 V3F_USB_REPORT_INTERVAL_US=125 V3F_ENABLE_RGB_STATUS=1 V3F_RGB_LED_COUNT=77 V3F_ENABLE_RF_BRIDGE=1 V3F_ENABLE_SPI_HOST_CMD=1 V3F_OUTPUT_MODE_DEFAULT=1

v5f V5F:
	@$(MAKE) -C v5f_rtthread PREFIX="$(PREFIX)" BUILD_ROOT=../$(BUILD_ROOT) CORE=V5F $(V5F_MAKE_ARGS)

clean:
	@$(MAKE) -C v3f BUILD_ROOT=../$(BUILD_ROOT)/V3F clean
	@$(MAKE) -C v5f_rtthread BUILD_ROOT=../$(BUILD_ROOT) CORE=V5F clean
	rm -rf "$(BUILD_ROOT)"

flash: all
	powershell -ExecutionPolicy Bypass -File flash_dualcore.ps1
