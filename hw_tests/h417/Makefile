# Standalone CH32H417 board bring-up tests.

PREFIX ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
HW_TEST ?= h417_gpio_status
SPI_SOURCE ?= left
PROJECT_ROOT ?= ../..
BUILD_ROOT ?= build
H417_FIRMWARE_ROOT ?= $(PROJECT_ROOT)/firmware/h417
WCH_H417_SRC_ROOT := $(H417_FIRMWARE_ROOT)/basic/wch/SRC
V3F_DRIVER_ROOT := $(H417_FIRMWARE_ROOT)/v3f/drivers
V5F_DRIVER_ROOT := $(H417_FIRMWARE_ROOT)/v5f_rtthread/drivers
RGB1W_PIOC_ROOT := $(V3F_DRIVER_ROOT)/rgb1w_pioc
FLASH_NAND_ROOT := $(V3F_DRIVER_ROOT)/gd5f1g_spi_nand
LTDC_RGB_ROOT := $(V5F_DRIVER_ROOT)/ltdc_rgb
LTDC_RGB_CONTROL_SRC := $(LTDC_RGB_ROOT)/src/ch32h417_lcd_rgb_control.c
LTDC_RGB_LTDC_SRC := $(LTDC_RGB_ROOT)/src/ch32h417_ltdc_rgb.c
WCH_USBFS_COMPAT_HID_ROOT := $(V3F_DRIVER_ROOT)/wch_usbfs_compat_hid
WCH_USBFS_COMPAT_HID_SRC := $(WCH_USBFS_COMPAT_HID_ROOT)/src
WCH_USBFS_COMPAT_HID_INC := $(WCH_USBFS_COMPAT_HID_ROOT)/include
WCH_USBHS_COMPAT_HID_ROOT := $(V3F_DRIVER_ROOT)/wch_usbhs_compat_hid
WCH_USBHS_COMPAT_HID_SRC := $(WCH_USBHS_COMPAT_HID_ROOT)/src
WCH_USBHS_COMPAT_HID_INC := $(WCH_USBHS_COMPAT_HID_ROOT)/include
WCH_USBHS_COMPAT_HID_SYSTEM := $(WCH_USBHS_COMPAT_HID_ROOT)/system/v3f/system_ch32h417.c
H417_USBHS_DISABLE_SWD ?= 1
V3F_STANDALONE_ROOT := passed/v3f_standalone
V3F_STANDALONE_SRC_ROOT := $(V3F_STANDALONE_ROOT)/src
H417_V3F_WAKE_STUB_TEST := h417_v5f_wake_stub
H417_DUAL_CORE_TESTS := h417_v5f_ltdc h417_v5f_ltdc_l8_palette_image h417_v5f_ltdc_rgb565_diag h417_v5f_gpha_r2m_fill h417_v5f_gpha_pfc_l8_rgb565 h417_v5f_gpha_blend_rgb565 h417_v5f_gpha_l8_ltdc_fullscreen h417_v5f_flash h417_v5f_flash_l8_assets h417_v5f_tick_diag h417_v5f_sdram_memtest h417_v5f_sdram_ltdc_rgb565 h417_v5f_sdram_remap_probe h417_v5f_sdram_dq_probe h417_v5f_sdram_official_16bit h417_v5f_ch585_spi_speed h417_v5f_ch585_adc_key_cal
IS_DUAL_CORE_TEST := $(filter $(HW_TEST),$(H417_DUAL_CORE_TESTS))
APP_V5F_HW_TEST_ID_OVERRIDE :=
APP_V5F_HW_TEST_USB_CDC :=
APP_V5F_HW_TEST_SOURCE_ARGS :=
H417_HW_TEST_BUILD_NAME := $(HW_TEST)

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
else ifeq ($(HW_TEST),h417_v5f_sdram_memtest)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 11
else ifeq ($(HW_TEST),h417_v5f_sdram_ltdc_rgb565)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 12
else ifeq ($(HW_TEST),h417_v5f_sdram_remap_probe)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 13
else ifeq ($(HW_TEST),h417_v5f_sdram_dq_probe)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 14
APP_V5F_HW_TEST_USB_CDC := 1
else ifeq ($(HW_TEST),h417_v5f_sdram_official_16bit)
APP_V5F_HW_TEST_MODE := sdram_official_16bit
APP_V5F_HW_TEST_ID_OVERRIDE := 17
APP_V5F_HW_TEST_USB_CDC := 1
else ifeq ($(HW_TEST),h417_v5f_ch585_spi_speed)
APP_V5F_HW_TEST_MODE := ch585_spi_speed
APP_V5F_HW_TEST_ID_OVERRIDE := 15
APP_V5F_HW_TEST_USB_CDC := 1
H417_HW_TEST_BUILD_NAME := $(HW_TEST)_$(SPI_SOURCE)
ifeq ($(SPI_SOURCE),left)
APP_V5F_HW_TEST_SOURCE_ARGS := DEFS_EXTRA=-DAPP_CH585_SPI_SPEED_SOURCE_LEFT=1
else ifeq ($(SPI_SOURCE),right)
APP_V5F_HW_TEST_SOURCE_ARGS := DEFS_EXTRA=-DAPP_CH585_SPI_SPEED_SOURCE_RIGHT=1
else
$(error SPI_SOURCE must be left/right for h417_v5f_ch585_spi_speed, got '$(SPI_SOURCE)')
endif
else ifeq ($(HW_TEST),h417_v5f_ch585_adc_key_cal)
APP_V5F_HW_TEST_MODE := ch585_adc_key_cal
APP_V5F_HW_TEST_ID_OVERRIDE := 16
APP_V5F_HW_TEST_USB_CDC := 1
H417_HW_TEST_BUILD_NAME := $(HW_TEST)_$(SPI_SOURCE)
ifeq ($(SPI_SOURCE),left)
APP_V5F_HW_TEST_SOURCE_ARGS := DEFS_EXTRA=-DAPP_CH585_ADC_KEY_CAL_SOURCE_LEFT=1
else ifeq ($(SPI_SOURCE),right)
APP_V5F_HW_TEST_SOURCE_ARGS := DEFS_EXTRA=-DAPP_CH585_ADC_KEY_CAL_SOURCE_RIGHT=1
else
$(error SPI_SOURCE must be left/right for h417_v5f_ch585_adc_key_cal, got '$(SPI_SOURCE)')
endif
endif

APP_V5F_HW_TEST_USB_CDC_ARGS :=
ifeq ($(APP_V5F_HW_TEST_USB_CDC),1)
APP_V5F_HW_TEST_USB_CDC_ARGS := APP_ENABLE_USB_TEST=1 APP_ENABLE_USB2_FS_CDC=1 APP_ENABLE_USB2_HS_CDC=0 APP_ENABLE_USBSS_CDC=0
endif

DUAL_CORE_BUILD_ROOT := ../../hw_tests/h417/$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)
DIRECT_V5F_BUILD_ROOT := ../../../hw_tests/h417/$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)
H417_V3F_WAKE_BUILD_ROOT := $(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F

CC := $(PREFIX)gcc
AS := $(PREFIX)gcc -x assembler-with-cpp
LD := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE := $(PREFIX)size

ifeq ($(IS_DUAL_CORE_TEST),$(HW_TEST))

.PHONY: all clean size

all:
ifeq ($(APP_V5F_HW_TEST_ID_OVERRIDE),)
	@$(MAKE) -C "$(H417_FIRMWARE_ROOT)" PREFIX=$(PREFIX) BUILD_ROOT=$(DUAL_CORE_BUILD_ROOT) APP_V5F_HW_TEST=$(APP_V5F_HW_TEST_MODE)
else
	@$(MAKE) HW_TEST=$(H417_V3F_WAKE_STUB_TEST) BUILD_ROOT=$(H417_V3F_WAKE_BUILD_ROOT) H417_LOCAL_V3F_PROJECT=h417_V3F H417_LOCAL_V3F_BUILD_DIR=$(H417_V3F_WAKE_BUILD_ROOT)
	@$(MAKE) -C "$(H417_FIRMWARE_ROOT)/v5f_rtthread" PREFIX=$(PREFIX) BUILD_ROOT=$(DIRECT_V5F_BUILD_ROOT) CORE=V5F APP_V5F_HW_TEST=$(APP_V5F_HW_TEST_MODE) APP_V5F_HW_TEST_ID=$(APP_V5F_HW_TEST_ID_OVERRIDE) $(APP_V5F_HW_TEST_USB_CDC_ARGS) $(APP_V5F_HW_TEST_SOURCE_ARGS)
endif
	@echo "V3F build: $(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F"
	@echo "V5F build: $(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F"

size:
	@$(SIZE) "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F/h417_V3F.elf"
	@$(SIZE) "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.elf"

clean:
	rm -rf "$(BUILD_ROOT)"

else

ARCH_FLAGS := -march=rv32imac_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore
COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

SRC_ROOT := $(WCH_H417_SRC_ROOT)
INC := -I$(SRC_ROOT)/Core -I$(SRC_ROOT)/Peripheral/inc -I$(SRC_ROOT)/Debug
INC += -I$(RGB1W_PIOC_ROOT)/include -I$(FLASH_NAND_ROOT)/include -I$(LTDC_RGB_ROOT)/include
INC += -I$(V3F_STANDALONE_SRC_ROOT)
DEFS := -DCore_V3F -DHW_TEST_NAME=\"$(HW_TEST)\"
LOCAL_V3F_USE_TEST_MAIN := 1
H417_LOCAL_V3F_PROJECT ?= $(HW_TEST)
H417_LOCAL_V3F_BUILD_DIR ?= $(BUILD_ROOT)/$(HW_TEST)

SYSTEM_SRC := $(V3F_STANDALONE_SRC_ROOT)/system_ch32h417.c
COMMON_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_common.c
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
else ifeq ($(HW_TEST),h417_usbfs_official_hid)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_usbfs_official_hid.c
TEST_SRC += $(WCH_USBFS_COMPAT_HID_SRC)/ch32h417_usbfs_device.c
TEST_SRC += $(WCH_USBFS_COMPAT_HID_SRC)/usb_desc.c
INC += -I$(WCH_USBFS_COMPAT_HID_INC)
DEFS += -DHW_TEST_FUNC=h417_usbfs_official_hid_run -DHW_TEST_ID=30
else ifeq ($(HW_TEST),h417_usbhs_official_hid)
SYSTEM_SRC := $(WCH_USBHS_COMPAT_HID_SYSTEM)
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_usbhs_official_hid.c
TEST_SRC += $(WCH_USBHS_COMPAT_HID_SRC)/ch32h417_usbhs_device.c
TEST_SRC += $(WCH_USBHS_COMPAT_HID_SRC)/usb_desc.c
INC += -I$(WCH_USBHS_COMPAT_HID_INC)
DEFS += -DHW_TEST_FUNC=h417_usbhs_official_hid_run -DHW_TEST_ID=31
DEFS += -DH417_USBHS_DISABLE_SWD=$(H417_USBHS_DISABLE_SWD)
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_flash.c
else ifeq ($(HW_TEST),h417_v5f_wake_stub)
LOCAL_V3F_USE_TEST_MAIN := 0
TEST_SRC := $(V3F_STANDALONE_SRC_ROOT)/h417_v5f_wake_stub.c
DEFS += -DHW_TEST_ID=90
else
$(error Unknown HW_TEST '$(HW_TEST)')
endif

ifeq ($(LOCAL_V3F_USE_TEST_MAIN),1)
COMMON_SRC := $(SYSTEM_SRC) $(COMMON_SRC)
else
COMMON_SRC := $(SYSTEM_SRC)
endif

SDK_SRC := $(SRC_ROOT)/Peripheral/src/ch32h417_gpio.c
SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_pwr.c
SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_rcc.c
SDK_SRC += $(EXTRA_SDK_SRC)

STARTUP := $(SRC_ROOT)/Startup/startup_ch32h417_v3f.S
LDSCRIPT := $(V3F_STANDALONE_ROOT)/Link_h417_v3f.ld

C_SOURCES := $(COMMON_SRC) $(TEST_SRC) $(SDK_SRC)
ASM_SOURCES := $(STARTUP)

BUILD := $(H417_LOCAL_V3F_BUILD_DIR)
ELF := $(BUILD)/$(H417_LOCAL_V3F_PROJECT).elf
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))

VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean size

all: $(ELF) $(BUILD)/$(H417_LOCAL_V3F_PROJECT).hex $(BUILD)/$(H417_LOCAL_V3F_PROJECT).bin $(BUILD)/$(H417_LOCAL_V3F_PROJECT).lst
	@$(SIZE) "$(ELF)"

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(DEFS) $(INC) -MMD -MP -MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(DEFS) $(INC) $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(H417_LOCAL_V3F_PROJECT).map" -lm -o $@

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
