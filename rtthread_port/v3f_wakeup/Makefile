# Minimal V3F baremetal wake-up program for CH32H417
# Wakes V5F core and enters idle loop

PREFIX       ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
PROJECT_NAME ?= v3f_wakeup
PROJECT_ROOT ?= .
BUILD_ROOT   ?= build

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

# EVT SDK root
EVT_ROOT     := C:/program1/hardware/WCH/CH32H417/CH32H417EVT/EVT/EXAM
SRC_ROOT     := $(EVT_ROOT)/SRC

# Defines
DEFS := -DCore_V3F

# Include paths
INC := -I"$(PROJECT_ROOT)"
INC += -I"$(SRC_ROOT)/Debug" -I"$(SRC_ROOT)/Core" -I"$(SRC_ROOT)/Peripheral/inc"

# Source files
APP_SRC := $(PROJECT_ROOT)/main.c
APP_SRC += $(PROJECT_ROOT)/system_ch32h417.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_rcc.c
APP_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_pwr.c

# Startup and linker
STARTUP := $(PROJECT_ROOT)/startup_ch32h417_v3f.S
LDSCRIPT := $(PROJECT_ROOT)/Link_v3f.ld

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
