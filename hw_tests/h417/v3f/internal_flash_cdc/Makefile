# CH32H417 V3F internal CodeFlash test with product USBFS CDC output.

PREFIX       ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
PROJECT_NAME ?= h417_internal_flash_cdc
BUILD_ROOT   ?= build

H417_ROOT       := ../../../../firmware/h417
WCH_ROOT        := $(H417_ROOT)/basic/wch
SRC_ROOT        := $(WCH_ROOT)/SRC
V3F_ROOT        := $(H417_ROOT)/v3f
USBFS_NKRO_ROOT := $(V3F_ROOT)/drivers/usbfs_hid_nkro
USBFS_WCH_ROOT  := $(V3F_ROOT)/drivers/wch_usbfs_compat_hid

CC      := "$(PREFIX)gcc"
AS      := "$(PREFIX)gcc" -x assembler-with-cpp
LD      := "$(PREFIX)gcc"
OBJCOPY := "$(PREFIX)objcopy"
OBJDUMP := "$(PREFIX)objdump"
SIZE    := "$(PREFIX)size"

ARCH_FLAGS := -march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32 \
              -msmall-data-limit=8 -msave-restore
COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 \
                -fsigned-char -ffunction-sections -fdata-sections \
                -fno-common -Wall -Wextra -Wno-comment -Wuninitialized \
                -std=gnu99
ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections \
           --specs=nano.specs --specs=nosys.specs

DEFS := -DCore_V3F
DEFS += -DV3F_ENABLE_USBFS_CDC_DEBUG=1
DEFS += -DV3F_ENABLE_USBFS_CDC=1
DEFS += -DV3F_ENABLE_USBHS_8K=0

INC := -Isrc
INC += -I$(V3F_ROOT)/bsp
INC += -I$(USBFS_NKRO_ROOT)/include
INC += -I$(USBFS_WCH_ROOT)/include
INC += -I../../../../firmware/common
INC += -I$(SRC_ROOT)/Debug
INC += -I$(SRC_ROOT)/Core
INC += -I$(SRC_ROOT)/Peripheral/inc

C_SOURCES := src/main.c
C_SOURCES += $(V3F_ROOT)/bsp/system_ch32h417.c
C_SOURCES += $(USBFS_NKRO_ROOT)/src/ch32h417_usbfs_hid_nkro.c
C_SOURCES += $(USBFS_WCH_ROOT)/src/ch32h417_usbfs_device.c
C_SOURCES += $(USBFS_WCH_ROOT)/src/usbfs_desc.c
C_SOURCES += $(SRC_ROOT)/Peripheral/src/ch32h417_rcc.c
C_SOURCES += $(SRC_ROOT)/Peripheral/src/ch32h417_pwr.c
C_SOURCES += $(SRC_ROOT)/Peripheral/src/ch32h417_flash.c
C_SOURCES += $(SRC_ROOT)/Peripheral/src/ch32h417_gpio.c
C_SOURCES += $(SRC_ROOT)/Peripheral/src/ch32h417_dbgmcu.c
C_SOURCES += $(SRC_ROOT)/Peripheral/src/ch32h417_iwdg.c

ASM_SOURCES := $(V3F_ROOT)/bsp/startup_ch32h417_v3f.S
LDSCRIPT := $(V3F_ROOT)/bsp/linker_scripts/Link_v3f.ld

BUILD := $(BUILD_ROOT)
ELF := $(BUILD)/$(PROJECT_NAME).elf
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))
DEPS := $(OBJS:.o=.d)
VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))

.PHONY: all clean size

all: $(ELF) $(BUILD)/$(PROJECT_NAME).hex \
	 $(BUILD)/$(PROJECT_NAME).bin $(BUILD)/$(PROJECT_NAME).lst
	@$(SIZE) "$(ELF)"

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(DEFS) $(INC) -MMD -MP \
		-MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(DEFS) $(INC) $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -T"$(LDSCRIPT)" \
		-Wl,-Map,"$(BUILD)/$(PROJECT_NAME).map" -lm -o $@

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
