# CH32H417 V3F firmware
# Owns the reset-time V3F flow and starts V5F when the shared setup is ready.

PREFIX       ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
PROJECT_NAME ?= h417_V3F
PROJECT_ROOT ?= .
BUILD_ROOT   ?= ../build/V3F

CC      := $(PREFIX)gcc
AS      := $(PREFIX)gcc -x assembler-with-cpp
LD      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE    := $(PREFIX)size

# V3F has no FPU: rv32imac
ARCH_FLAGS := -march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore

COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99

ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

# WCH EVT hardware library root
WCH_H417_ROOT ?= ../../../basic/ch32h417/wch
SRC_ROOT      := $(WCH_H417_ROOT)/SRC

# Optional official USBSS examples are not required by the default V3F firmware build.
OFFICIAL_USBSS_ROOT ?= C:/program1/hardware/WCH/CH32H417/CH32H417EVT/EVT/EXAM/USBSS/DEVICE/CH372Device/Common

V3F_USBSS_OFFICIAL_OWNER ?= 0
V3F_ENABLE_USBFS_CLOCK_INIT ?= 0

# Defines
DEFS := -DCore_V3F
DEFS += -DV3F_ENABLE_USBFS_CLOCK_INIT=$(V3F_ENABLE_USBFS_CLOCK_INIT)
ifeq ($(V3F_USBSS_OFFICIAL_OWNER),1)
DEFS += -DV3F_USBSS_OFFICIAL_OWNER=1
endif

# Include paths
INC := -I"$(PROJECT_ROOT)/bsp"
INC += -I"$(PROJECT_ROOT)/drivers"
INC += -I"$(SRC_ROOT)/Debug" -I"$(SRC_ROOT)/Core" -I"$(SRC_ROOT)/Peripheral/inc"
ifeq ($(V3F_USBSS_OFFICIAL_OWNER),1)
INC += -I"$(OFFICIAL_USBSS_ROOT)"
endif

# Source files
APP_SRC := $(PROJECT_ROOT)/applications/main.c
APP_SRC += $(PROJECT_ROOT)/bsp/system_ch32h417.c
APP_SRC += $(PROJECT_ROOT)/drivers/adc.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_rcc.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_pwr.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_flash.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_gpio.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_dbgmcu.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_adc.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_dma.c

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
