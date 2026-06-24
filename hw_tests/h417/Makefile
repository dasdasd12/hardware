# Standalone CH32H417 board bring-up tests.

PREFIX ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
HW_TEST ?= h417_gpio_status
PROJECT_ROOT ?= ../..
BUILD_ROOT ?= build
BASIC_H417_ROOT ?= $(PROJECT_ROOT)/basic/ch32h417
WCH_H417_SRC_ROOT := $(BASIC_H417_ROOT)/wch/SRC
RGB1W_PIOC_ROOT := $(BASIC_H417_ROOT)/drivers/rgb1w_pioc

CC := $(PREFIX)gcc
AS := $(PREFIX)gcc -x assembler-with-cpp
LD := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE := $(PREFIX)size

ARCH_FLAGS := -march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore
COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SRC_ROOT := $(WCH_H417_SRC_ROOT)
INC := -I$(SRC_ROOT)/Core -I$(SRC_ROOT)/Peripheral/inc -I$(RGB1W_PIOC_ROOT)/include -Isrc
DEFS := -DCore_V3F -DHW_TEST_NAME=\"$(HW_TEST)\"

COMMON_SRC := src/system_ch32h417.c src/h417_common.c src/h417_test_main.c

ifeq ($(HW_TEST),h417_gpio_status)
TEST_SRC := src/h417_gpio_status.c
DEFS += -DHW_TEST_FUNC=h417_gpio_status_run -DHW_TEST_ID=1
else ifeq ($(HW_TEST),h417_pf13_probe)
TEST_SRC := src/h417_pf13_probe.c
DEFS += -DHW_TEST_FUNC=h417_pf13_probe_run -DHW_TEST_ID=20
else ifeq ($(HW_TEST),h417_ws2812)
TEST_SRC := src/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=2 -DWS2812_EFFECT=WS2812_EFFECT_SOLID
else ifeq ($(HW_TEST),h417_ws2812_breath)
TEST_SRC := src/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=22 -DWS2812_EFFECT=WS2812_EFFECT_BREATH
else ifeq ($(HW_TEST),h417_ws2812_chase)
TEST_SRC := src/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=23 -DWS2812_EFFECT=WS2812_EFFECT_CHASE
else ifeq ($(HW_TEST),h417_ws2812_rainbow_band)
TEST_SRC := src/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=24 -DWS2812_EFFECT=WS2812_EFFECT_RAINBOW_BAND
else ifeq ($(HW_TEST),h417_lcd_signal)
TEST_SRC := src/h417_lcd_signal.c
DEFS += -DHW_TEST_FUNC=h417_lcd_signal_run -DHW_TEST_ID=3
else
$(error Unknown HW_TEST '$(HW_TEST)')
endif

SDK_SRC := $(SRC_ROOT)/Peripheral/src/ch32h417_gpio.c
SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_pwr.c
SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_rcc.c

STARTUP := $(SRC_ROOT)/Startup/startup_ch32h417_v3f.S
LDSCRIPT := Link_h417_v3f.ld

C_SOURCES := $(COMMON_SRC) $(TEST_SRC) $(SDK_SRC)
ASM_SOURCES := $(STARTUP)

BUILD := $(BUILD_ROOT)/$(HW_TEST)
ELF := $(BUILD)/$(HW_TEST).elf
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))

VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean size

all: $(ELF) $(BUILD)/$(HW_TEST).hex $(BUILD)/$(HW_TEST).bin $(BUILD)/$(HW_TEST).lst
	@$(SIZE) "$(ELF)"

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(DEFS) $(INC) -MMD -MP -MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(DEFS) $(INC) $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(HW_TEST).map" -lm -o $@

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
