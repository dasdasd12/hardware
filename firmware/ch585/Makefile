# CH585 firmware build entry.
# Default target builds the RF basic smoke firmware.

PREFIX       ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
PROJECT_NAME ?= ch585_rf_basic
BUILD_ROOT   ?= build

CC      := $(PREFIX)gcc
AS      := $(PREFIX)gcc -x assembler-with-cpp
LD      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE    := $(PREFIX)size

ARCH_FLAGS := -march=rv32imc_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore

COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
# WCH SDK sources keep unused locals behind compile-time feature gates.
COMMON_FLAGS += -Wno-unused-variable

ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SDK_ROOT := basic/wch/SRC
LIB_ROOT := basic/wch/LIB

INC := -I"applications/rf_basic"
INC += -I"drivers/rf/include"
INC += -I"bsp/hal/include"
INC += -I"$(LIB_ROOT)"
INC += -I"$(SDK_ROOT)/StdPeriphDriver/inc"
INC += -I"$(SDK_ROOT)/RVMSIS"

C_SOURCES := applications/rf_basic/main.c
C_SOURCES += drivers/rf/RF_basic.c
C_SOURCES += bsp/hal/MCU.c
C_SOURCES += bsp/hal/RTC.c
C_SOURCES += bsp/hal/SLEEP.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_adc.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_clk.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_flash.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_gpio.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_pwr.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_sys.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_timer0.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_timer3.c
C_SOURCES += $(SDK_ROOT)/StdPeriphDriver/CH58x_uart0.c

ASM_SOURCES := $(SDK_ROOT)/Startup/startup_CH585.S
ASM_SOURCES += $(LIB_ROOT)/ble_task_scheduler.S

LDSCRIPT := $(SDK_ROOT)/Ld/Link.ld

BUILD := $(BUILD_ROOT)/rf_basic
ELF := $(BUILD)/$(PROJECT_NAME).elf

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))

VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))
DEPS := $(OBJS:.o=.d)

.PHONY: all rf_basic clean size

all: rf_basic

rf_basic: $(ELF) $(BUILD)/$(PROJECT_NAME).hex $(BUILD)/$(PROJECT_NAME).bin $(BUILD)/$(PROJECT_NAME).lst
	@$(SIZE) "$(ELF)"

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
	@mkdir -p "$@"

size: $(ELF)
	@$(SIZE) "$(ELF)"

clean:
	rm -rf "$(BUILD_ROOT)"

-include $(DEPS)
