# CH32H417 dual-core firmware build entry.

PREFIX ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
BUILD_ROOT ?= build

.PHONY: all v3f V3F v5f V5F clean flash

all: v3f v5f

v3f V3F:
	@$(MAKE) -C v3f PREFIX=$(PREFIX) BUILD_ROOT=../$(BUILD_ROOT)/V3F

v5f V5F:
	@$(MAKE) -C v5f_rtthread PREFIX=$(PREFIX) BUILD_ROOT=../$(BUILD_ROOT) CORE=V5F

clean:
	@$(MAKE) -C v3f BUILD_ROOT=../$(BUILD_ROOT)/V3F clean
	@$(MAKE) -C v5f_rtthread BUILD_ROOT=../$(BUILD_ROOT) CORE=V5F clean
	rm -rf "$(BUILD_ROOT)"

flash: all
	powershell -ExecutionPolicy Bypass -File flash_dualcore.ps1
