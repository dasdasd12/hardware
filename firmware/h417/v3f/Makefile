# CH32H417 V3F firmware
# Owns the reset-time V3F flow and starts V5F when the shared setup is ready.

PREFIX       ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
PROJECT_NAME ?= h417_V3F
PROJECT_ROOT ?= .
BUILD_ROOT   ?= ../build/V3F

CC      := "$(PREFIX)gcc"
AS      := "$(PREFIX)gcc" -x assembler-with-cpp
LD      := "$(PREFIX)gcc"
OBJCOPY := "$(PREFIX)objcopy"
OBJDUMP := "$(PREFIX)objdump"
SIZE    := "$(PREFIX)size"

# V3F has no FPU: rv32imac
ARCH_FLAGS := -march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore

# LTO keeps the product image inside the 64KB zero-wait RAM_CODE window.
# Disable linker relaxation because late GP shortening can target constants
# outside the signed 12-bit GP range once that window is nearly full.
COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -flto -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99

ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -flto -nostartfiles -Xlinker --gc-sections -Xlinker --no-relax --specs=nano.specs --specs=nosys.specs

# H417-local WCH EVT hardware library root
WCH_H417_ROOT ?= ../basic/wch
SRC_ROOT      := $(WCH_H417_ROOT)/SRC

# Optional official USBSS examples are not required by the default V3F firmware build.
OFFICIAL_USBSS_ROOT ?= C:/program1/hardware/WCH/CH32H417/CH32H417EVT/EVT/EXAM/USBSS/DEVICE/CH372Device/Common

V3F_USBSS_OFFICIAL_OWNER ?= 0
H417_BOARD_MODEL ?= NEW
V3F_ENABLE_USBFS_CLOCK_INIT ?= 0
V3F_WAKE_V5F ?= 1
V3F_ENABLE_USBHS_8K ?= 1
V3F_USB_REPORT_INTERVAL_US ?= 125
V3F_ENABLE_RGB_STATUS ?= 1
V3F_RGB_LED_COUNT ?= 77
V3F_RGB_MAX_BRIGHTNESS ?= 255
V3F_ENABLE_RF_BRIDGE ?= 1
V3F_ENABLE_SPI_HOST_CMD ?= 1
# Keep the H417 USBHS HID path active while the selected CH585 radio path is
# also running. The switches remain independent for regression/bisect builds.
V3F_ENABLE_USB_RF_COEXIST ?= 1
V3F_ENABLE_USB_BLE_COEXIST ?= 1
# Select output modes through Fn+F1/F2/F3 without changing the hardware model.
# This is intentionally independent from H417_BOARD_MODEL: the production PCB
# still needs the NEW board power/pin setup even when firmware owns the switch.
V3F_ENABLE_FN_OUTPUT_SWITCH ?= 1
V3F_ENABLE_USBFS_CDC_DEBUG ?= 0
# Second USB controller: USBFS runs the CDC config channel while USBHS
# keeps the 8K HID path. Only meaningful with V3F_ENABLE_USBHS_8K=1.
V3F_ENABLE_USBFS_CDC ?= 1
ifeq ($(H417_BOARD_MODEL),NEW)
H417_BOARD_DEFS := -DH417_BOARD_MODEL=H417_BOARD_MODEL_NEW
V3F_OUTPUT_MODE_DEFAULT ?= 1
else ifeq ($(H417_BOARD_MODEL),OLD)
H417_BOARD_DEFS := -DH417_BOARD_MODEL=H417_BOARD_MODEL_OLD
V3F_OUTPUT_MODE_DEFAULT ?= 1
else
$(error Unsupported H417_BOARD_MODEL '$(H417_BOARD_MODEL)'; expected NEW or OLD)
endif
# NEW and OLD share output directories; use -B or clean when switching models.
ifeq ($(V3F_ENABLE_USBFS_CDC),1)
V3F_ENABLE_USBFS_CDC_DEBUG := 1
endif

# Defines
DEFS := -DCore_V3F
DEFS += $(H417_BOARD_DEFS)
DEFS += -DV3F_ENABLE_USBFS_CLOCK_INIT=$(V3F_ENABLE_USBFS_CLOCK_INIT)
DEFS += -DV3F_WAKE_V5F=$(V3F_WAKE_V5F)
DEFS += -DV3F_ENABLE_USBHS_8K=$(V3F_ENABLE_USBHS_8K)
DEFS += -DV3F_USB_REPORT_INTERVAL_US=$(V3F_USB_REPORT_INTERVAL_US)
DEFS += -DV3F_ENABLE_RGB_STATUS=$(V3F_ENABLE_RGB_STATUS)
DEFS += -DV3F_RGB_LED_COUNT=$(V3F_RGB_LED_COUNT)
DEFS += -DV3F_RGB_MAX_BRIGHTNESS=$(V3F_RGB_MAX_BRIGHTNESS)
DEFS += -DV3F_ENABLE_RF_BRIDGE=$(V3F_ENABLE_RF_BRIDGE)
DEFS += -DV3F_ENABLE_SPI_HOST_CMD=$(V3F_ENABLE_SPI_HOST_CMD)
DEFS += -DV3F_ENABLE_USB_RF_COEXIST=$(V3F_ENABLE_USB_RF_COEXIST)
DEFS += -DV3F_ENABLE_USB_BLE_COEXIST=$(V3F_ENABLE_USB_BLE_COEXIST)
DEFS += -DV3F_ENABLE_FN_OUTPUT_SWITCH=$(V3F_ENABLE_FN_OUTPUT_SWITCH)
DEFS += -DV3F_ENABLE_USBFS_CDC_DEBUG=$(V3F_ENABLE_USBFS_CDC_DEBUG)
DEFS += -DV3F_OUTPUT_MODE_DEFAULT=$(V3F_OUTPUT_MODE_DEFAULT)
DEFS += -DV3F_ENABLE_USBFS_CDC=$(V3F_ENABLE_USBFS_CDC)
ifeq ($(V3F_USBSS_OFFICIAL_OWNER),1)
DEFS += -DV3F_USBSS_OFFICIAL_OWNER=1
endif

# Include paths
INC := -I"$(PROJECT_ROOT)/bsp"
INC += -I"$(PROJECT_ROOT)/drivers"
INC += -I"$(PROJECT_ROOT)/applications"
INC += -I"../../common"
INC += -I"$(PROJECT_ROOT)/drivers/ch585_spi_link/include"
INC += -I"$(PROJECT_ROOT)/drivers/rgb1w_pioc/include"
ifeq ($(V3F_ENABLE_USBHS_8K),1)
INC += -I"$(PROJECT_ROOT)/drivers/usbhs_hid_nkro/include"
INC += -I"$(PROJECT_ROOT)/drivers/wch_usbhs_compat_hid/include"
ifeq ($(V3F_ENABLE_USBFS_CDC),1)
INC += -I"$(PROJECT_ROOT)/drivers/usbfs_hid_nkro/include"
INC += -I"$(PROJECT_ROOT)/drivers/wch_usbfs_compat_hid/include"
endif
else
INC += -I"$(PROJECT_ROOT)/drivers/usbfs_hid_nkro/include"
INC += -I"$(PROJECT_ROOT)/drivers/wch_usbfs_compat_hid/include"
endif
INC += -I"$(SRC_ROOT)/Debug" -I"$(SRC_ROOT)/Core" -I"$(SRC_ROOT)/Peripheral/inc"
ifeq ($(V3F_USBSS_OFFICIAL_OWNER),1)
INC += -I"$(OFFICIAL_USBSS_ROOT)"
endif

# Source files
APP_SRC := $(PROJECT_ROOT)/applications/main.c
APP_SRC += $(PROJECT_ROOT)/applications/board_init.c
APP_SRC += $(PROJECT_ROOT)/applications/approval_mailbox.c
APP_SRC += $(PROJECT_ROOT)/applications/ch585_link.c
APP_SRC += $(PROJECT_ROOT)/applications/half_state.c
APP_SRC += $(PROJECT_ROOT)/applications/default_profile.c
APP_SRC += $(PROJECT_ROOT)/applications/factory_profile_image.c
APP_SRC += $(PROJECT_ROOT)/applications/profile_store.c
APP_SRC += $(PROJECT_ROOT)/applications/profile_runtime.c
APP_SRC += $(PROJECT_ROOT)/applications/profile_activate.c
APP_SRC += $(PROJECT_ROOT)/applications/profile_sync.c
APP_SRC += $(PROJECT_ROOT)/applications/pc_link.c
APP_SRC += $(PROJECT_ROOT)/applications/rf_report_bridge.c
APP_SRC += $(PROJECT_ROOT)/applications/rgb_status.c
APP_SRC += $(PROJECT_ROOT)/bsp/system_ch32h417.c
APP_SRC += $(PROJECT_ROOT)/drivers/ch585_spi_link/src/ch32h417_ch585_spi_link.c
APP_SRC += $(PROJECT_ROOT)/drivers/rgb1w_pioc/src/ch32h417_pioc_rgb1w.c
ifeq ($(V3F_ENABLE_USBHS_8K),1)
APP_SRC += $(PROJECT_ROOT)/drivers/usbhs_hid_nkro/src/ch32h417_usbhs_hid_nkro.c
APP_SRC += $(PROJECT_ROOT)/drivers/wch_usbhs_compat_hid/src/ch32h417_usbhs_device.c
APP_SRC += $(PROJECT_ROOT)/drivers/wch_usbhs_compat_hid/src/usb_desc.c
ifeq ($(V3F_ENABLE_USBFS_CDC),1)
APP_SRC += $(PROJECT_ROOT)/drivers/usbfs_hid_nkro/src/ch32h417_usbfs_hid_nkro.c
APP_SRC += $(PROJECT_ROOT)/drivers/wch_usbfs_compat_hid/src/ch32h417_usbfs_device.c
APP_SRC += $(PROJECT_ROOT)/drivers/wch_usbfs_compat_hid/src/usbfs_desc.c
endif
else
APP_SRC += $(PROJECT_ROOT)/drivers/usbfs_hid_nkro/src/ch32h417_usbfs_hid_nkro.c
APP_SRC += $(PROJECT_ROOT)/drivers/wch_usbfs_compat_hid/src/ch32h417_usbfs_device.c
APP_SRC += $(PROJECT_ROOT)/drivers/wch_usbfs_compat_hid/src/usbfs_desc.c
endif
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_rcc.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_pwr.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_flash.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_gpio.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_dbgmcu.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_spi.c

ifeq ($(V3F_USBSS_OFFICIAL_OWNER),1)
APP_SRC += $(SRC_ROOT)/Debug/debug.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_tim.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_usart.c
APP_SRC += $(OFFICIAL_USBSS_ROOT)/ch32h417_usbss_device.c
APP_SRC += $(OFFICIAL_USBSS_ROOT)/ch32h417_usbss_it.c
APP_SRC += $(OFFICIAL_USBSS_ROOT)/ch32h417_usbhs_device.c
APP_SRC += $(OFFICIAL_USBSS_ROOT)/usb_desc.c
endif

# Startup and linker
STARTUP := $(PROJECT_ROOT)/bsp/startup_ch32h417_v3f.S
LDSCRIPT := $(PROJECT_ROOT)/bsp/linker_scripts/Link_v3f.ld

# All sources
C_SOURCES := $(APP_SRC)
ASM_SOURCES := $(STARTUP)

# Build directory
BUILD := $(BUILD_ROOT)
ELF := $(BUILD)/$(PROJECT_NAME).elf

# Object file generation
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))

# VPATH for source lookup
VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))

# Dependency files
DEPS := $(OBJS:.o=.d)

.PHONY: all clean size flash

all: $(ELF) $(BUILD)/$(PROJECT_NAME).hex $(BUILD)/$(PROJECT_NAME).bin $(BUILD)/$(PROJECT_NAME).lst
	@$(SIZE) "$(ELF)"

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(DEFS) $(INC) -MMD -MP -MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(DEFS) $(INC) $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(PROJECT_NAME).map" -lm -o $@

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
	@mkdir -p "$@"

size: $(ELF)
	@$(SIZE) "$(ELF)"

clean:
	rm -rf "$(BUILD_ROOT)"

-include $(DEPS)
