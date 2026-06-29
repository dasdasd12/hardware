# Standalone CH585 board bring-up tests.

PREFIX ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
HALF ?= left
TEST ?= ch585_left_eeprom_i2c
PROJECT_ROOT ?= ../..
BUILD_ROOT ?= build
ALLOW_EEPROM_WRITE ?= 0
CH585_HALF_ROLE := $(HALF)
CH585_FIRMWARE_ROOT ?= $(PROJECT_ROOT)/firmware/ch585
WCH_CH585_SRC_ROOT := $(CH585_FIRMWARE_ROOT)/basic/wch/SRC
CH585_APPLICATION_ROOT := $(CH585_FIRMWARE_ROOT)/applications
CH585_DRIVER_ROOT := $(CH585_FIRMWARE_ROOT)/drivers

CC := "$(PREFIX)gcc"
AS := "$(PREFIX)gcc" -x assembler-with-cpp
LD := "$(PREFIX)gcc"
OBJCOPY := "$(PREFIX)objcopy"
OBJDUMP := "$(PREFIX)objdump"
SIZE := "$(PREFIX)size"

ARCH_FLAGS := -march=rv32imac_zicsr -mabi=ilp32 -msmall-data-limit=8 -msave-restore
COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
COMMON_FLAGS += -Wno-unused-variable
ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SRC_ROOT := $(WCH_CH585_SRC_ROOT)
DRIVER_ROOT := $(SRC_ROOT)/StdPeriphDriver
INC := -I$(SRC_ROOT)/RVMSIS -I$(DRIVER_ROOT)/inc
INC += -I$(CH585_APPLICATION_ROOT) -I$(CH585_DRIVER_ROOT)
INC += -I$(PROJECT_ROOT)/hw_tests/common -Isrc
DEFS := -DTEST_NAME=\"$(TEST)\" -DCH585_HALF_NAME=\"$(HALF)\"
DEFS += -DALLOW_EEPROM_WRITE=$(ALLOW_EEPROM_WRITE)
DEFS += $(DEFS_EXTRA)

ifeq ($(HALF),right)
DEFS += -DCH585_ADC_MUX_HALF_RIGHT=1
else
DEFS += -DCH585_ADC_MUX_HALF_LEFT=1
endif

COMMON_SRC := src/ch585_common.c src/ch585_test_main.c
TEST_EXTRA_SRC :=
TEST_EXTRA_SDK_SRC :=

ifeq ($(filter $(TEST),ch585_left_eeprom_i2c ch585_u2_eeprom_i2c),$(TEST))
TEST_SRC := src/ch585_u2_eeprom_i2c.c
TEST_EXTRA_SRC += src/ch585_soft_i2c.c
DEFS += -DCH585_TEST_FUNC=ch585_u2_eeprom_i2c_run
else ifeq ($(filter $(TEST),ch585_left_controls_gpio ch585_u2_controls_gpio),$(TEST))
TEST_SRC := src/ch585_u2_controls_gpio.c
DEFS += -DCH585_TEST_FUNC=ch585_u2_controls_gpio_run
else ifeq ($(filter $(TEST),ch585_right_max17048_i2c ch585_u3_max17048_i2c),$(TEST))
TEST_SRC := src/ch585_u3_max17048_i2c.c
TEST_EXTRA_SRC += src/ch585_soft_i2c.c
DEFS += -DCH585_TEST_FUNC=ch585_u3_max17048_i2c_run
else ifeq ($(filter $(TEST),ch585_right_charge_gpio ch585_u3_charge_gpio),$(TEST))
TEST_SRC := src/ch585_u3_charge_gpio.c
DEFS += -DCH585_TEST_FUNC=ch585_u3_charge_gpio_run
else ifeq ($(filter $(TEST),ch585_right_ec11_gpio ch585_u3_ec11_gpio),$(TEST))
TEST_SRC := src/ch585_u3_ec11_gpio.c
DEFS += -DCH585_TEST_FUNC=ch585_u3_ec11_gpio_run
else ifeq ($(TEST),ch585_ads7948_mux_probe)
TEST_SRC := src/ch585_adc_mux_scan.c
TEST_EXTRA_SRC += $(CH585_APPLICATION_ROOT)/ch585_ads7948_mux_acq.c
DEFS += -DCH585_TEST_FUNC=ch585_adc_mux_scan_run
DEFS += -DSYSCLK_FREQ=CLK_SOURCE_HSE_PLL_78MHz
DEFS += -DCH585_SYSCLK_SOURCE=CLK_SOURCE_HSE_PLL_78MHz
DEFS += -DFREQ_SYS=78000000
else ifeq ($(TEST),ch585_spi0_speed_slave)
TEST_SRC := src/ch585_spi0_speed_slave.c
TEST_EXTRA_SRC += $(CH585_DRIVER_ROOT)/ch585_spi0_slave_link.c
TEST_EXTRA_SDK_SRC += $(DRIVER_ROOT)/CH58x_spi0.c
DEFS += -DCH585_TEST_FUNC=ch585_spi0_speed_slave_run
else ifeq ($(TEST),ch585_adc_key_cal_slave)
TEST_SRC := src/ch585_adc_key_cal_slave.c
TEST_EXTRA_SRC += $(CH585_APPLICATION_ROOT)/ch585_ads7948_mux_acq.c
TEST_EXTRA_SRC += $(CH585_DRIVER_ROOT)/ch585_spi0_slave_link.c
TEST_EXTRA_SDK_SRC += $(DRIVER_ROOT)/CH58x_spi0.c
DEFS += -DCH585_TEST_FUNC=ch585_adc_key_cal_slave_run
DEFS += -DSYSCLK_FREQ=CLK_SOURCE_HSE_PLL_78MHz
DEFS += -DCH585_SYSCLK_SOURCE=CLK_SOURCE_HSE_PLL_78MHz
DEFS += -DFREQ_SYS=78000000
else
$(error Unknown TEST '$(TEST)')
endif

SDK_SRC := $(DRIVER_ROOT)/CH58x_clk.c
SDK_SRC += $(DRIVER_ROOT)/CH58x_gpio.c
SDK_SRC += $(DRIVER_ROOT)/CH58x_spi1.c
SDK_SRC += $(DRIVER_ROOT)/CH58x_sys.c
SDK_SRC += $(DRIVER_ROOT)/CH58x_uart1.c
SDK_SRC += $(TEST_EXTRA_SDK_SRC)
SDK_LIB := $(DRIVER_ROOT)/libISP585.a

STARTUP := $(SRC_ROOT)/Startup/startup_CH585.S
LDSCRIPT := $(SRC_ROOT)/Ld/Link.ld

C_SOURCES := $(COMMON_SRC) $(TEST_SRC) $(TEST_EXTRA_SRC) $(SDK_SRC)
ASM_SOURCES := $(STARTUP)

BUILD_NAME := $(CH585_HALF_ROLE)_$(TEST)

BUILD := $(BUILD_ROOT)/$(BUILD_NAME)
ELF := $(BUILD)/$(CH585_HALF_ROLE)_$(TEST).elf
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))
VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean size

all: $(ELF) $(BUILD)/$(CH585_HALF_ROLE)_$(TEST).hex $(BUILD)/$(CH585_HALF_ROLE)_$(TEST).bin $(BUILD)/$(CH585_HALF_ROLE)_$(TEST).lst
	@$(SIZE) "$(ELF)"

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(DEFS) $(INC) -MMD -MP -MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(DEFS) $(INC) $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(SDK_LIB) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(CH585_HALF_ROLE)_$(TEST).map" -lm -o $@

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
