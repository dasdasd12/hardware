# CH585 firmware build entry.
# Default target builds the half keyboard scan firmware.

PREFIX       ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
APP          ?= half_scan
PROJECT_NAME ?= ch585_$(APP)
BUILD_ROOT   ?= build
BUILD_VARIANT ?= $(APP)
HALF_SCAN_CLOCK_DEFS := -DSYSCLK_FREQ=CLK_SOURCE_HSE_PLL_78MHz -DFREQ_SYS=78000000
HALF_SCAN_DEBUG_UART_DEFS := -DCH585_HALF_SCAN_DEBUG_UART=1 -DDEBUG=Debug_UART1

CC      := "$(PREFIX)gcc"
AS      := "$(PREFIX)gcc" -x assembler-with-cpp
LD      := "$(PREFIX)gcc"
OBJCOPY := "$(PREFIX)objcopy"
OBJDUMP := "$(PREFIX)objdump"
SIZE    := "$(PREFIX)size"

ARCH_FLAGS := -march=rv32imc_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore

COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
# WCH SDK sources keep unused locals behind compile-time feature gates.
COMMON_FLAGS += -Wno-unused-variable
COMMON_FLAGS += $(DEFS_EXTRA)

ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SDK_ROOT := basic/wch/SRC
LIB_ROOT := basic/wch/LIB

INC := -I"applications/$(APP)"
INC += -I"applications"
INC += -I"../common"
INC += -I"drivers"
INC += -I"drivers/ble"
INC += -I"drivers/rf/include"
INC += -I"drivers/usb/usbhs_keyboard"
INC += -I"bsp/hal/include"
INC += -I"$(LIB_ROOT)"
INC += -I"$(SDK_ROOT)/StdPeriphDriver/inc"
INC += -I"$(SDK_ROOT)/RVMSIS"

APP_C_SOURCES :=
ifeq ($(APP),half_scan)
APP_C_SOURCES += applications/half_scan/main.c
APP_C_SOURCES += applications/half_scan/ch585_half_report.c
APP_C_SOURCES += applications/magnetic_key_engine.c
APP_C_SOURCES += applications/half_scan/ch585_rf_nkro_tx.c
APP_C_SOURCES += applications/ch585_ads7948_mux_acq.c
APP_C_SOURCES += drivers/ch585_spi0_slave_link.c
ifneq (,$(findstring -DCH585_BLE_HID_ENABLE=1,$(DEFS_EXTRA)))
APP_C_SOURCES += drivers/ble/ble_hid.c
APP_C_SOURCES += drivers/ble/battservice.c
APP_C_SOURCES += drivers/ble/devinfoservice.c
APP_C_SOURCES += drivers/ble/hiddev.c
APP_C_SOURCES += drivers/ble/hidkbdservice.c
APP_C_SOURCES += drivers/ble/scanparamservice.c
endif
else ifeq ($(APP),rf_basic)
APP_C_SOURCES += applications/rf_basic/main.c
APP_C_SOURCES += drivers/rf/RF_basic.c
else ifeq ($(APP),rf_receiver_usbhs)
APP_C_SOURCES += applications/rf_receiver_usbhs/main.c
APP_C_SOURCES += applications/rf_receiver_usbhs/rf_receiver.c
APP_C_SOURCES += drivers/usb/usbhs_keyboard/ch585_usbhs_kbd.c
APP_C_SOURCES += drivers/usb/usbhs_keyboard/usb_desc_hid.c
APP_C_SOURCES += drivers/usb/usbhs_keyboard/usb_hid.c
APP_C_SOURCES += drivers/usb/usbhs_keyboard/usb_hs_config_stub.c
else
$(error Unknown APP '$(APP)'; expected half_scan, rf_basic, or rf_receiver_usbhs)
endif

C_SOURCES := $(APP_C_SOURCES)
C_SOURCES += bsp/hal/MCU.c
C_SOURCES += bsp/hal/RTC.c
C_SOURCES += bsp/hal/SLEEP.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_adc.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_clk.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_flash.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_gpio.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_pwr.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_spi0.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_spi1.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_sys.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_timer0.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_timer3.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_uart0.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_uart1.c

ASM_SOURCES := $(SDK_ROOT)/Startup/startup_CH585.S
ASM_SOURCES += $(LIB_ROOT)/ble_task_scheduler.S

LDSCRIPT := $(SDK_ROOT)/Ld/Link.ld

BUILD := $(BUILD_ROOT)/$(BUILD_VARIANT)
ELF := $(BUILD)/$(PROJECT_NAME).elf

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))

VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))
DEPS := $(OBJS:.o=.d)

.PHONY: all firmware half_scan half_scan_left half_scan_right half_scan_left_rf half_scan_left_rf_debug_uart half_scan_left_ble_debug_uart half_scan_left_output_switch_debug_uart half_scan_left_debug_uart half_scan_right_debug_uart rf_basic rf_receiver_usbhs rf_keyboard_tx clean size

all: firmware

firmware: $(ELF) $(BUILD)/$(PROJECT_NAME).hex $(BUILD)/$(PROJECT_NAME).bin $(BUILD)/$(PROJECT_NAME).lst
	@$(SIZE) "$(ELF)"

half_scan:
	@$(MAKE) APP=$@ firmware

half_scan_left:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_left BUILD_VARIANT=half_scan_left DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) -DCH585_HALF_ID=0 -DCH585_RF_TX_ENABLE=0" firmware

half_scan_right:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_right BUILD_VARIANT=half_scan_right DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) -DCH585_HALF_ID=1 -DCH585_RF_TX_ENABLE=0" firmware

half_scan_left_rf:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_left_rf BUILD_VARIANT=half_scan_left_rf DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) -DCH585_HALF_ID=0 -DCH585_RF_TX_ENABLE=1" firmware

half_scan_left_rf_debug_uart:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_left_rf_debug_uart BUILD_VARIANT=half_scan_left_rf_debug_uart DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) $(HALF_SCAN_DEBUG_UART_DEFS) -DCH585_HALF_ID=0 -DCH585_RF_TX_ENABLE=1" firmware

half_scan_left_ble_debug_uart:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_left_ble_debug_uart BUILD_VARIANT=half_scan_left_ble_debug_uart DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) $(HALF_SCAN_DEBUG_UART_DEFS) -DCH585_HALF_ID=0 -DCH585_RF_TX_ENABLE=0 -DCH585_BLE_HID_ENABLE=1 -DCH585_SPI0_SLAVE_LINK_TMOS_HOOK=1 -DCH585_ADS7948_MUX_TMOS_HOOK=1" firmware

half_scan_left_output_switch_debug_uart:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_left_output_switch_debug_uart BUILD_VARIANT=half_scan_left_output_switch_debug_uart DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) $(HALF_SCAN_DEBUG_UART_DEFS) -DCH585_HALF_ID=0 -DCH585_RF_TX_ENABLE=1 -DCH585_BLE_HID_ENABLE=1 -DCH585_SPI0_SLAVE_LINK_TMOS_HOOK=1 -DCH585_ADS7948_MUX_TMOS_HOOK=1" firmware

half_scan_left_debug_uart:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_left_debug_uart BUILD_VARIANT=half_scan_left_debug_uart DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) $(HALF_SCAN_DEBUG_UART_DEFS) -DCH585_HALF_ID=0 -DCH585_RF_TX_ENABLE=0" firmware

half_scan_right_debug_uart:
	@$(MAKE) APP=half_scan PROJECT_NAME=ch585_half_scan_right_debug_uart BUILD_VARIANT=half_scan_right_debug_uart DEFS_EXTRA="$(DEFS_EXTRA) $(HALF_SCAN_CLOCK_DEFS) $(HALF_SCAN_DEBUG_UART_DEFS) -DCH585_HALF_ID=1 -DCH585_RF_TX_ENABLE=0" firmware

rf_basic rf_receiver_usbhs:
	@$(MAKE) APP=$@ firmware

rf_keyboard_tx:
	$(error rf_keyboard_tx is disabled; RF TX is integrated into half_scan_left)

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(INC) -MMD -MP -MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(INC) $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(PROJECT_NAME).map" \
		-L"$(LIB_ROOT)" -L"$(SDK_ROOT)/StdPeriphDriver" -lCH58xBLE -lCH58xBLE_PERI -lISP585 -lm -o $@

$(BUILD)/%.hex: $(ELF)
	@echo "HEX $(notdir $@)"
	@$(OBJCOPY) -O ihex "$<" "$@"

$(BUILD)/%.bin: $(ELF)
	@echo "BIN $(notdir $@)"
	@$(OBJCOPY) -O binary "$<" "$@"

$(BUILD)/%.lst: $(ELF)
	@echo "LST $(notdir $@)"
	@$(OBJDUMP) --all-headers --demangle --disassemble -M xw "$<" > "$@"

$(BUILD):
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "[void](New-Item -ItemType Directory -Force -Path '$@')"
else
	@mkdir -p "$@"
endif

size: $(ELF)
	@$(SIZE) "$(ELF)"

clean:
	rm -rf "$(BUILD_ROOT)"

-include $(DEPS)
