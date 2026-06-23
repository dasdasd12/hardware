# Standalone CH585 board bring-up tests.

PREFIX ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
CH585_EVT_ROOT ?= C:/program1/hardware/WCH/CH585EVT/EVT/EXAM
HALF ?= u2
TEST ?= ch585_u2_eeprom_i2c
BUILD_ROOT ?= build
ALLOW_EEPROM_WRITE ?= 0

CC := $(PREFIX)gcc
AS := $(PREFIX)gcc -x assembler-with-cpp
LD := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE := $(PREFIX)size

ARCH_FLAGS := -march=rv32imac_zicsr -mabi=ilp32 -msmall-data-limit=8 -msave-restore
COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SRC_ROOT := $(CH585_EVT_ROOT)/SRC
DRIVER_ROOT := $(SRC_ROOT)/StdPeriphDriver
INC := -I$(SRC_ROOT)/RVMSIS -I$(DRIVER_ROOT)/inc -Isrc
DEFS := -DTEST_NAME=\"$(TEST)\" -DCH585_HALF_NAME=\"$(HALF)\"
DEFS += -DALLOW_EEPROM_WRITE=$(ALLOW_EEPROM_WRITE)

COMMON_SRC := src/ch585_common.c src/ch585_test_main.c
TEST_EXTRA_SRC :=

ifeq ($(TEST),ch585_u2_eeprom_i2c)
TEST_SRC := src/ch585_u2_eeprom_i2c.c
TEST_EXTRA_SRC += src/ch585_soft_i2c.c
DEFS += -DCH585_TEST_FUNC=ch585_u2_eeprom_i2c_run
else ifeq ($(TEST),ch585_u2_controls_gpio)
TEST_SRC := src/ch585_u2_controls_gpio.c
DEFS += -DCH585_TEST_FUNC=ch585_u2_controls_gpio_run
else ifeq ($(TEST),ch585_u3_max17048_i2c)
TEST_SRC := src/ch585_u3_max17048_i2c.c
TEST_EXTRA_SRC += src/ch585_soft_i2c.c
DEFS += -DCH585_TEST_FUNC=ch585_u3_max17048_i2c_run
else ifeq ($(TEST),ch585_u3_charge_gpio)
TEST_SRC := src/ch585_u3_charge_gpio.c
DEFS += -DCH585_TEST_FUNC=ch585_u3_charge_gpio_run
else ifeq ($(TEST),ch585_u3_ec11_gpio)
TEST_SRC := src/ch585_u3_ec11_gpio.c
DEFS += -DCH585_TEST_FUNC=ch585_u3_ec11_gpio_run
else
$(error Unknown TEST '$(TEST)')
endif

SDK_SRC := $(DRIVER_ROOT)/CH58x_clk.c
SDK_SRC += $(DRIVER_ROOT)/CH58x_gpio.c
SDK_SRC += $(DRIVER_ROOT)/CH58x_sys.c
SDK_SRC += $(DRIVER_ROOT)/CH58x_uart1.c
SDK_LIB := $(DRIVER_ROOT)/libISP585.a

STARTUP := $(SRC_ROOT)/Startup/startup_CH585.S
LDSCRIPT := $(SRC_ROOT)/Ld/Link.ld

C_SOURCES := $(COMMON_SRC) $(TEST_SRC) $(TEST_EXTRA_SRC) $(SDK_SRC)
ASM_SOURCES := $(STARTUP)

BUILD := $(BUILD_ROOT)/$(HALF)_$(TEST)
ELF := $(BUILD)/$(HALF)_$(TEST).elf
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))
VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean size

all: $(ELF) $(BUILD)/$(HALF)_$(TEST).hex $(BUILD)/$(HALF)_$(TEST).bin $(BUILD)/$(HALF)_$(TEST).lst
	@$(SIZE) "$(ELF)"

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(DEFS) $(INC) -MMD -MP -MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(DEFS) $(INC) $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(SDK_LIB) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(HALF)_$(TEST).map" -lm -o $@

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
