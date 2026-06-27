# Standalone CH32H417 board bring-up tests.

PREFIX ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
HW_TEST ?= h417_gpio_status
PROJECT_ROOT ?= ../..
BUILD_ROOT ?= build
H417_FIRMWARE_ROOT ?= $(PROJECT_ROOT)/firmware/h417
WCH_H417_SRC_ROOT := $(H417_FIRMWARE_ROOT)/basic/wch/SRC
H417_DRIVER_ROOT := $(H417_FIRMWARE_ROOT)/drivers
V3F_DRIVER_ROOT := $(H417_FIRMWARE_ROOT)/v3f/drivers
RGB1W_PIOC_ROOT := $(V3F_DRIVER_ROOT)/rgb1w_pioc
FLASH_NAND_ROOT := $(H417_DRIVER_ROOT)/gd5f1g_spi_nand
LTDC_RGB_ROOT := $(H417_DRIVER_ROOT)/ltdc_rgb
LTDC_RGB_CONTROL_SRC := $(LTDC_RGB_ROOT)/src/ch32h417_lcd_rgb_control.c
LTDC_RGB_LTDC_SRC := $(LTDC_RGB_ROOT)/src/ch32h417_ltdc_rgb.c
V3F_STANDALONE_ROOT := passed/v3f_standalone
V3F_STANDALONE_SRC_ROOT := $(V3F_STANDALONE_ROOT)/src
H417_DUAL_CORE_TESTS := h417_v5f_ltdc h417_v5f_ltdc_l8_palette_image h417_v5f_ltdc_rgb565_diag h417_v5f_gpha_r2m_fill h417_v5f_gpha_pfc_l8_rgb565 h417_v5f_gpha_blend_rgb565 h417_v5f_gpha_l8_ltdc_fullscreen h417_v5f_flash h417_v5f_flash_l8_assets h417_v5f_tick_diag
IS_DUAL_CORE_TEST := $(filter $(HW_TEST),$(H417_DUAL_CORE_TESTS))

ifeq ($(HW_TEST),h417_v5f_ltdc)
APP_V5F_HW_TEST_MODE := ltdc
else ifeq ($(HW_TEST),h417_v5f_ltdc_l8_palette_image)
APP_V5F_HW_TEST_MODE := ltdc_l8_palette_image
else ifeq ($(HW_TEST),h417_v5f_ltdc_rgb565_diag)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
else ifeq ($(HW_TEST),h417_v5f_gpha_r2m_fill)
APP_V5F_HW_TEST_MODE := gpha_r2m_fill
else ifeq ($(HW_TEST),h417_v5f_gpha_pfc_l8_rgb565)
APP_V5F_HW_TEST_MODE := gpha_pfc_l8_rgb565
else ifeq ($(HW_TEST),h417_v5f_gpha_blend_rgb565)
APP_V5F_HW_TEST_MODE := gpha_blend_rgb565
else ifeq ($(HW_TEST),h417_v5f_gpha_l8_ltdc_fullscreen)
APP_V5F_HW_TEST_MODE := gpha_l8_ltdc_fullscreen
else ifeq ($(HW_TEST),h417_v5f_flash)
APP_V5F_HW_TEST_MODE := flash
else ifeq ($(HW_TEST),h417_v5f_flash_l8_assets)
APP_V5F_HW_TEST_MODE := flash_l8_assets
else ifeq ($(HW_TEST),h417_v5f_tick_diag)
APP_V5F_HW_TEST_MODE := tick_diag
endif

DUAL_CORE_BUILD_ROOT := ../../hw_tests/h417/$(BUILD_ROOT)/$(HW_TEST)

CC := $(PREFIX)gcc
AS := $(PREFIX)gcc -x assembler-with-cpp
LD := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE := $(PREFIX)size

ifeq ($(IS_DUAL_CORE_TEST),$(HW_TEST))

.PHONY: all clean size

all:
	@$(MAKE) -C "$(H417_FIRMWARE_ROOT)" PREFIX=$(PREFIX) BUILD_ROOT=$(DUAL_CORE_BUILD_ROOT) APP_V5F_HW_TEST=$(APP_V5F_HW_TEST_MODE)
	@echo "V3F build: $(BUILD_ROOT)/$(HW_TEST)/V3F"
	@echo "V5F build: $(BUILD_ROOT)/$(HW_TEST)/V5F"

size:
	@$(SIZE) "$(BUILD_ROOT)/$(HW_TEST)/V3F/h417_V3F.elf"
	@$(SIZE) "$(BUILD_ROOT)/$(HW_TEST)/V5F/rtthread_ch32h417_V5F.elf"

clean:
	rm -rf "$(BUILD_ROOT)"

else

ARCH_FLAGS := -march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore
COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SRC_ROOT := $(WCH_H417_SRC_ROOT)
INC := -I$(SRC_ROOT)/Core -I$(SRC_ROOT)/Peripheral/inc -I$(RGB1W_PIOC_ROOT)/include -I$(FLASH_NAND_ROOT)/include -I$(LTDC_RGB_ROOT)/include -I$(V3F_STANDALONE_SRC_ROOT)
DEFS := -DCore_V3F -DHW_TEST_NAME=\"$(HW_TEST)\"

COMMON_SRC := $(V3F_STANDALONE_SRC_ROOT)/system_ch32h417.c
COMMON_SRC += $(V3F_STANDALONE_SRC_ROOT)/h417_common.c
COMMON_SRC += $(V3F_STANDALONE_SRC_ROOT)/h417_test_main.c
EXTRA_SDK_SRC :=

ifeq ($(HW_TEST),h417_gpio_status)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_gpio_status.c
DEFS += -DHW_TEST_FUNC=h417_gpio_status_run -DHW_TEST_ID=1
else ifeq ($(HW_TEST),h417_pf13_probe)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_pf13_probe.c
DEFS += -DHW_TEST_FUNC=h417_pf13_probe_run -DHW_TEST_ID=20
else ifeq ($(HW_TEST),h417_ws2812)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=2 -DWS2812_EFFECT=WS2812_EFFECT_SOLID
else ifeq ($(HW_TEST),h417_ws2812_breath)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=22 -DWS2812_EFFECT=WS2812_EFFECT_BREATH
else ifeq ($(HW_TEST),h417_ws2812_chase)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=23 -DWS2812_EFFECT=WS2812_EFFECT_CHASE
else ifeq ($(HW_TEST),h417_ws2812_rainbow_band)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_ws2812.c $(RGB1W_PIOC_ROOT)/src/ch32h417_pioc_rgb1w.c
DEFS += -DHW_TEST_FUNC=h417_ws2812_run -DHW_TEST_ID=24 -DWS2812_EFFECT=WS2812_EFFECT_RAINBOW_BAND
else ifeq ($(HW_TEST),h417_lcd_signal)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_lcd_signal.c
DEFS += -DHW_TEST_FUNC=h417_lcd_signal_run -DHW_TEST_ID=3
else ifeq ($(HW_TEST),h417_lcd_backlight)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_lcd_backlight.c $(LTDC_RGB_CONTROL_SRC)
DEFS += -DHW_TEST_FUNC=h417_lcd_backlight_run -DHW_TEST_ID=5
else ifeq ($(HW_TEST),h417_ltdc)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_ltdc.c $(LTDC_RGB_CONTROL_SRC) $(LTDC_RGB_LTDC_SRC)
DEFS += -DHW_TEST_FUNC=h417_ltdc_run -DHW_TEST_ID=4
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_ltdc.c
else ifeq ($(HW_TEST),h417_flash_mosi_probe)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_flash_mosi_probe.c
DEFS += -DHW_TEST_FUNC=h417_flash_mosi_probe_run -DHW_TEST_ID=7
else ifeq ($(HW_TEST),h417_flash_image)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_flash_image.c $(LTDC_RGB_CONTROL_SRC) $(LTDC_RGB_LTDC_SRC)
TEST_SRC += $(FLASH_NAND_ROOT)/src/gd5f1g_spi_nand.c
TEST_SRC += $(FLASH_NAND_ROOT)/src/ch32h417_gd5f1g_spi1.c
DEFS += -DHW_TEST_FUNC=h417_flash_image_run -DHW_TEST_ID=6
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_ltdc.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_spi.c
else
$(error Unknown HW_TEST '$(HW_TEST)')
endif

SDK_SRC := $(SRC_ROOT)/Peripheral/src/ch32h417_gpio.c
SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_pwr.c
SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_rcc.c
SDK_SRC += $(EXTRA_SDK_SRC)

STARTUP := $(SRC_ROOT)/Startup/startup_ch32h417_v3f.S
LDSCRIPT := $(V3F_STANDALONE_ROOT)/Link_h417_v3f.ld

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

endif
