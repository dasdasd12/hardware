# RT-Thread Makefile for CH32H417 V5F
# Modeled after H417lib/HAL_SmokeTest/Makefile

PREFIX       ?= /c/riscv-wch-toolchain/bin/riscv-wch-elf-
PROJECT_NAME ?= rtthread_ch32h417
PROJECT_ROOT ?= .
BUILD_ROOT   ?= ../build
CORE         ?= V5F

CC      := $(PREFIX)gcc
AS      := $(PREFIX)gcc -x assembler-with-cpp
LD      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE    := $(PREFIX)size

# Architecture flags: V5F has FPU (rv32imafc), V3F does not (rv32imac)
ARCH_FLAGS := -march=rv32imafc_zba_zbb_zbc_zbs_xw -mabi=ilp32 -msmall-data-limit=8 -msave-restore

COMMON_FLAGS := $(ARCH_FLAGS) -fmax-errors=20 -Os -g -fmessage-length=0 -fsigned-char
COMMON_FLAGS += -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99

ASFLAGS := $(ARCH_FLAGS) -Wall -fmessage-length=0
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs

# H417-local WCH EVT hardware library root
WCH_H417_ROOT ?= ../basic/wch
SRC_ROOT      := $(WCH_H417_ROOT)/SRC
V3F_DRIVER_ROOT ?= ../v3f/drivers
V5F_DRIVER_ROOT ?= $(PROJECT_ROOT)/drivers
LTDC_RGB_ROOT := $(V5F_DRIVER_ROOT)/ltdc_rgb
FLASH_NAND_ROOT := $(V3F_DRIVER_ROOT)/gd5f1g_spi_nand
GPHA_2D_ROOT := $(V5F_DRIVER_ROOT)/gpha_2d
CH585_SPI_LINK_ROOT := $(V3F_DRIVER_ROOT)/ch585_spi_link
V5F_HW_TEST_ROOT ?= ../../../hw_tests/h417/cases/v5f_rtthread
V5F_HW_TEST_SRC_ROOT := $(V5F_HW_TEST_ROOT)/src
V5F_HW_TEST_ASSET_ROOT := $(V5F_HW_TEST_ROOT)/assets
V5F_HW_TEST_MAIN ?= v5f_hw_test.c
V5F_HW_TEST_EXTRA_SRC ?=
V5F_HW_TEST_LINK_AFTER_USB ?= 0
V5F_HW_TEST_TAIL_SRC ?=
V5F_HW_TEST_LINKER_TRANSFORM ?=
V5F_HW_TEST_POST_LINK_PATCH ?=
V5F_HW_TEST_REFERENCE_ELF ?=
PYTHON ?= python
HW_TEST_COMMON_ROOT ?= ../../../hw_tests/common

# RT-Thread root
RTT_ROOT     := $(PROJECT_ROOT)/rt-thread

# Single-hardware test selection for V5F RT-Thread.
#   none  - default product bring-up path
#   ltdc  - full-screen L8 grayscale image LTDC framebuffer test
#   ltdc_l8_palette_image - full-screen 256-color L8 palette image test
#   ltdc_rgb565_diag - RGB565 direct-output diagnostic bands
#   ltdc_ui_frames - three-frame Claude Code style L8 UI demo
#   gpha_r2m_fill - GPHA register-to-memory fill animation
#   gpha_pfc_l8_rgb565 - GPHA L8/CLUT to RGB565 pixel-format conversion
#   gpha_blend_rgb565 - GPHA ARGB4444-over-RGB565 blend to RGB565
#   gpha_l8_ltdc_fullscreen - GPHA writes an 800x480 L8 framebuffer scanned by LTDC
#   flash - GD5F1G read-only ID/status test with LTDC status output
#   flash_l8_assets - erase/write/read GD5F1G L8 image assets and display color L8
#   tick_diag - V5F RT tick versus mcycle visual timing comparison
#   ch585_spi_speed - CH585 SPI1 link SCK sweep with CDC debug output
#   ch585_adc_key_cal - H417 USB CDC to CH585 single-key ADC calibration bridge
#   sdram_official_16bit - WCH official FMC/SDRAM_16bit example init in hw_tests
APP_V5F_HW_TEST ?= none
APP_V5F_HW_TEST_ID := 0

ifeq ($(APP_V5F_HW_TEST),none)
APP_ENABLE_V5F_HW_TEST := 0
else ifeq ($(APP_V5F_HW_TEST),ltdc)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 1
else ifeq ($(APP_V5F_HW_TEST),ltdc_l8_palette_image)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 4
else ifeq ($(APP_V5F_HW_TEST),ltdc_rgb565_diag)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 5
else ifeq ($(APP_V5F_HW_TEST),gpha_r2m_fill)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 2
else ifeq ($(APP_V5F_HW_TEST),gpha_pfc_l8_rgb565)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 6
else ifeq ($(APP_V5F_HW_TEST),gpha_blend_rgb565)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 8
else ifeq ($(APP_V5F_HW_TEST),gpha_l8_ltdc_fullscreen)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 9
else ifeq ($(APP_V5F_HW_TEST),flash)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 3
else ifeq ($(APP_V5F_HW_TEST),flash_l8_assets)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 10
else ifeq ($(APP_V5F_HW_TEST),tick_diag)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 7
else ifeq ($(APP_V5F_HW_TEST),ch585_spi_speed)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 15
else ifeq ($(APP_V5F_HW_TEST),ch585_adc_key_cal)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 16
else ifeq ($(APP_V5F_HW_TEST),sdram_official_16bit)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 17
else ifeq ($(APP_V5F_HW_TEST),ltdc_ui_frames)
APP_ENABLE_V5F_HW_TEST := 1
APP_V5F_HW_TEST_ID := 18
else
$(error Unknown APP_V5F_HW_TEST '$(APP_V5F_HW_TEST)')
endif

ifeq ($(APP_V5F_HW_TEST),none)
APP_ENABLE_V5F_DISPLAY ?= 1
APP_ENABLE_V5F_FLASH_ANIMATION ?= $(APP_ENABLE_V5F_DISPLAY)
else
APP_ENABLE_V5F_DISPLAY ?= 0
APP_ENABLE_V5F_FLASH_ANIMATION ?= 0
endif

ifeq ($(APP_ENABLE_V5F_DISPLAY),1)
ifneq ($(APP_V5F_HW_TEST),none)
$(error APP_ENABLE_V5F_DISPLAY cannot be combined with APP_V5F_HW_TEST)
endif
endif

ifeq ($(APP_ENABLE_V5F_FLASH_ANIMATION),1)
ifneq ($(APP_V5F_HW_TEST),none)
$(error APP_ENABLE_V5F_FLASH_ANIMATION cannot be combined with APP_V5F_HW_TEST)
endif
ifneq ($(APP_ENABLE_V5F_DISPLAY),1)
$(error APP_ENABLE_V5F_FLASH_ANIMATION requires APP_ENABLE_V5F_DISPLAY=1)
endif
# Compile the qualified H4V1 player path while keeping the product selector
# string at "none" and APP_ENABLE_V5F_HW_TEST disabled.
APP_V5F_HW_TEST_ID := 19
endif

APP_ENABLE_USB_TEST ?= 0
APP_ENABLE_CH585_SPI_SCAN ?= 0
APP_ENABLE_SERIAL_HEARTBEAT ?= 0

# Defines (only board-specific ones; RT-Thread config macros are in rtconfig.h)
DEFS := -DCore_V5F -D__RT_KERNEL_SOURCE__ -D__RT_IPC_SOURCE__
ifeq ($(APP_ENABLE_USB_TEST),1)
DEFS += -DCONFIG_CHERRYUSB
endif
APP_ENABLE_USBSS_CDC ?= 0
APP_USBSS_SKIP_FOR_V3F_OFFICIAL ?= 0
APP_ENABLE_USB2_HS_CDC ?= 0
# USBFS is owned by V3F (CDC config channel); V5F must not drive it.
APP_ENABLE_USB2_FS_CDC ?= 0
APP_ENABLE_USB2_HS_HID ?= 0
APP_ENABLE_CH585_BLE_BRIDGE ?= 0
APP_MAIN_LOOP_DELAY_MS ?=
DEFS += -DAPP_ENABLE_USB_TEST=$(APP_ENABLE_USB_TEST)
DEFS += -DAPP_ENABLE_CH585_SPI_SCAN=$(APP_ENABLE_CH585_SPI_SCAN)
DEFS += -DAPP_ENABLE_SERIAL_HEARTBEAT=$(APP_ENABLE_SERIAL_HEARTBEAT)
DEFS += -DAPP_ENABLE_V5F_HW_TEST=$(APP_ENABLE_V5F_HW_TEST)
DEFS += -DAPP_ENABLE_V5F_DISPLAY=$(APP_ENABLE_V5F_DISPLAY)
DEFS += -DAPP_ENABLE_V5F_FLASH_ANIMATION=$(APP_ENABLE_V5F_FLASH_ANIMATION)
DEFS += -DAPP_V5F_HW_TEST=$(APP_V5F_HW_TEST_ID)
DEFS += -DAPP_V5F_HW_TEST_NAME=\"$(APP_V5F_HW_TEST)\"
DEFS += -DAPP_ENABLE_USBSS_CDC=$(APP_ENABLE_USBSS_CDC)
DEFS += -DAPP_USBSS_SKIP_FOR_V3F_OFFICIAL=$(APP_USBSS_SKIP_FOR_V3F_OFFICIAL)
DEFS += -DAPP_ENABLE_USB2_HS_CDC=$(APP_ENABLE_USB2_HS_CDC)
DEFS += -DAPP_ENABLE_USB2_FS_CDC=$(APP_ENABLE_USB2_FS_CDC)
DEFS += -DAPP_ENABLE_USB2_HS_HID=$(APP_ENABLE_USB2_HS_HID)
DEFS += -DAPP_ENABLE_CH585_BLE_BRIDGE=$(APP_ENABLE_CH585_BLE_BRIDGE)
ifneq ($(APP_MAIN_LOOP_DELAY_MS),)
DEFS += -DAPP_MAIN_LOOP_DELAY_MS=$(APP_MAIN_LOOP_DELAY_MS)
endif
ifeq ($(APP_ENABLE_V5F_FLASH_ANIMATION),1)
DEFS += -DV5F_SDRAM_H4V1_CHUNKED_FRAMES=165u
DEFS += -DV5F_SDRAM_H4V1_MAX_FRAMES=165u
DEFS += -DH4V1_FLASH_COLD_BOOT_FRAMES=165u
DEFS += -DH4V1_FLASH_COLDBOOT_PRODUCT=1
endif
DEFS += $(DEFS_EXTRA)

# Include paths
INC := -I"$(PROJECT_ROOT)" -I"$(PROJECT_ROOT)/bsp" -I"$(PROJECT_ROOT)/drivers" -I"$(PROJECT_ROOT)/libcpu"
INC += -I"$(PROJECT_ROOT)/applications"
INC += -I"../../common"
INC += -I"$(RTT_ROOT)/include" -I"$(RTT_ROOT)/components/drivers/include"
INC += -I"$(RTT_ROOT)/components/libc/compilers/common/include"
INC += -I"$(RTT_ROOT)/components/libc/compilers/newlib"
INC += -I"$(RTT_ROOT)/components/finsh"
INC += -I"$(SRC_ROOT)/Debug" -I"$(SRC_ROOT)/Core" -I"$(SRC_ROOT)/Peripheral/inc"
INC += -I"$(LTDC_RGB_ROOT)/include" -I"$(FLASH_NAND_ROOT)/include" -I"$(GPHA_2D_ROOT)/include" -I"$(CH585_SPI_LINK_ROOT)/include"
ifneq ($(APP_V5F_HW_TEST),none)
INC += -I"$(V5F_HW_TEST_ROOT)/include"
INC += -I"$(HW_TEST_COMMON_ROOT)"
INC += -I"$(V5F_HW_TEST_ASSET_ROOT)/ltdc" -I"$(V5F_HW_TEST_ASSET_ROOT)/flash"
else ifeq ($(APP_ENABLE_V5F_FLASH_ANIMATION),1)
INC += -I"$(V5F_HW_TEST_ROOT)/include"
INC += -I"$(HW_TEST_COMMON_ROOT)"
endif
ifeq ($(APP_V5F_HW_TEST),sdram_official_16bit)
INC += -I"$(V5F_HW_TEST_SRC_ROOT)/official/wch_sdram_16bit"
endif

# CherryUSB include paths
CHERRYUSB_ROOT := $(RTT_ROOT)/components/drivers/usb/cherryusb
INC += -I"$(CHERRYUSB_ROOT)/core"
INC += -I"$(CHERRYUSB_ROOT)/common"
INC += -I"$(CHERRYUSB_ROOT)/class/cdc"
INC += -I"$(CHERRYUSB_ROOT)/class/hid"
INC += -I"$(CHERRYUSB_ROOT)/port/ch32h417"

# Source files
# RT-Thread kernel
RTT_SRC := $(RTT_ROOT)/src/clock.c
RTT_SRC += $(RTT_ROOT)/src/components.c
RTT_SRC += $(RTT_ROOT)/src/cpu_up.c
RTT_SRC += $(RTT_ROOT)/src/defunct.c
RTT_SRC += $(RTT_ROOT)/src/idle.c
RTT_SRC += $(RTT_ROOT)/src/ipc.c
RTT_SRC += $(RTT_ROOT)/src/irq.c
RTT_SRC += $(RTT_ROOT)/src/kservice.c
RTT_SRC += $(RTT_ROOT)/src/mem.c
RTT_SRC += $(RTT_ROOT)/src/mempool.c
RTT_SRC += $(RTT_ROOT)/src/object.c
RTT_SRC += $(RTT_ROOT)/src/scheduler_comm.c
RTT_SRC += $(RTT_ROOT)/src/scheduler_up.c
RTT_SRC += $(RTT_ROOT)/src/thread.c
RTT_SRC += $(RTT_ROOT)/src/timer.c
RTT_SRC += $(RTT_ROOT)/src/klibc/kstdio.c
RTT_SRC += $(RTT_ROOT)/src/klibc/kerrno.c
RTT_SRC += $(RTT_ROOT)/src/klibc/rt_vsnprintf_tiny.c
RTT_SRC += $(RTT_ROOT)/src/klibc/kstring.c

# RT-Thread components
RTT_SRC += $(RTT_ROOT)/components/drivers/core/device.c
RTT_SRC += $(RTT_ROOT)/components/drivers/pin/dev_pin.c
RTT_SRC += $(RTT_ROOT)/components/drivers/ipc/completion_comm.c
RTT_SRC += $(RTT_ROOT)/components/drivers/ipc/completion_up.c
RTT_SRC += $(RTT_ROOT)/components/drivers/ipc/dataqueue.c
RTT_SRC += $(RTT_ROOT)/components/libc/compilers/newlib/syscalls.c
RTT_SRC += $(RTT_ROOT)/components/libc/compilers/common/ctime.c
RTT_SRC += $(RTT_ROOT)/components/libc/compilers/common/cunistd.c
RTT_SRC += $(RTT_ROOT)/components/libc/compilers/common/cstdlib.c
RTT_SRC += $(RTT_ROOT)/components/finsh/shell.c
RTT_SRC += $(RTT_ROOT)/components/finsh/msh.c
RTT_SRC += $(RTT_ROOT)/components/finsh/cmd.c
RTT_SRC += $(RTT_ROOT)/components/finsh/msh_parse.c
RTT_SRC += $(RTT_ROOT)/components/finsh/msh_file.c

# libcpu
LIBCPU_SRC := $(PROJECT_ROOT)/libcpu/cpuport.c
LIBCPU_SRC += $(PROJECT_ROOT)/libcpu/trap_common.c
LIBCPU_SRC += $(PROJECT_ROOT)/libcpu/atomic_riscv.c
LIBCPU_ASM := $(PROJECT_ROOT)/libcpu/context_gcc.S
LIBCPU_ASM += $(PROJECT_ROOT)/libcpu/interrupt_gcc.S

# BSP and drivers
BSP_SRC := $(PROJECT_ROOT)/bsp/board.c
BSP_SRC += $(PROJECT_ROOT)/bsp/software_irq.c
BSP_SRC += $(PROJECT_ROOT)/bsp/system_ch32h417.c
DRV_SRC := $(PROJECT_ROOT)/drivers/drv_gpio.c

ifneq ($(APP_V5F_HW_TEST),none)
DRV_SRC += $(LTDC_RGB_ROOT)/src/ch32h417_lcd_rgb_control.c
DRV_SRC += $(LTDC_RGB_ROOT)/src/ch32h417_ltdc_rgb.c
else ifeq ($(APP_ENABLE_V5F_DISPLAY),1)
DRV_SRC += $(LTDC_RGB_ROOT)/src/ch32h417_lcd_rgb_control.c
DRV_SRC += $(LTDC_RGB_ROOT)/src/ch32h417_ltdc_rgb.c
endif

ifneq ($(filter $(APP_V5F_HW_TEST),gpha_r2m_fill gpha_pfc_l8_rgb565 gpha_blend_rgb565 gpha_l8_ltdc_fullscreen),)
DRV_SRC += $(GPHA_2D_ROOT)/src/ch32h417_gpha_2d.c
endif

ifneq ($(filter $(APP_V5F_HW_TEST),flash flash_l8_assets),)
DRV_SRC += $(FLASH_NAND_ROOT)/src/gd5f1g_spi_nand.c
DRV_SRC += $(FLASH_NAND_ROOT)/src/ch32h417_gd5f1g_spi1.c
endif

ifneq ($(filter $(APP_V5F_HW_TEST),ch585_spi_speed ch585_adc_key_cal),)
DRV_SRC += $(CH585_SPI_LINK_ROOT)/src/ch32h417_ch585_spi_link.c
endif

# Application
APP_SRC := $(PROJECT_ROOT)/applications/main.c
APP_ASM :=
ifeq ($(APP_ENABLE_V5F_DISPLAY),1)
APP_SRC += $(PROJECT_ROOT)/applications/v5f_display.c
APP_SRC += $(PROJECT_ROOT)/applications/v5f_ui_theme.c
APP_SRC += $(PROJECT_ROOT)/applications/v5f_competition_ui.c
APP_SRC += $(PROJECT_ROOT)/applications/v5f_claude_ui.c
APP_SRC += $(PROJECT_ROOT)/applications/v5f_approval_ui.c
endif
ifeq ($(APP_ENABLE_V5F_FLASH_ANIMATION),1)
APP_SRC += $(PROJECT_ROOT)/applications/v5f_flash_animation.c
APP_SRC += $(PROJECT_ROOT)/applications/v5f_spi1_lease.c
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/v5f_hw_test_h4v1.c
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/h4v1_video.c
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/h4v1_chunk120_gate.c
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/h4v1_flash_coldboot_core.c
endif
ifeq ($(APP_ENABLE_CH585_SPI_SCAN),1)
APP_SRC += $(PROJECT_ROOT)/applications/ch585_spi_scan.c
APP_SRC += $(PROJECT_ROOT)/applications/keyboard_engine.c
endif
ifeq ($(APP_ENABLE_CH585_BLE_BRIDGE),1)
APP_SRC += $(PROJECT_ROOT)/applications/ch585_ble_bridge.c
endif
ifeq ($(APP_ENABLE_USB_TEST),1)
APP_SRC += $(PROJECT_ROOT)/applications/usb_cdc_dual.c
endif
ifeq ($(APP_ENABLE_USB2_HS_HID),1)
APP_SRC += $(PROJECT_ROOT)/applications/usb_hs_hid_keyboard.c
endif

V5F_HW_TEST_POST_USB_SRC :=
ifneq ($(APP_V5F_HW_TEST),none)
ifeq ($(V5F_HW_TEST_LINK_AFTER_USB),1)
V5F_HW_TEST_POST_USB_SRC += $(V5F_HW_TEST_SRC_ROOT)/$(V5F_HW_TEST_MAIN)
ifneq ($(strip $(V5F_HW_TEST_EXTRA_SRC)),)
V5F_HW_TEST_POST_USB_SRC += $(addprefix $(V5F_HW_TEST_SRC_ROOT)/,$(V5F_HW_TEST_EXTRA_SRC))
endif
else
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/$(V5F_HW_TEST_MAIN)
ifneq ($(strip $(V5F_HW_TEST_EXTRA_SRC)),)
APP_SRC += $(addprefix $(V5F_HW_TEST_SRC_ROOT)/,$(V5F_HW_TEST_EXTRA_SRC))
endif
endif
endif
ifeq ($(APP_V5F_HW_TEST),sdram_official_16bit)
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/official/wch_sdram_16bit/hardware.c
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/official/wch_sdram_16bit/h417_v5f_sdram_official_16bit_board.c
endif
ifeq ($(APP_V5F_HW_TEST),flash_l8_assets)
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/gd5f1g_l8_asset_store.c
endif
ifeq ($(APP_V5F_HW_TEST),ltdc_ui_frames)
APP_SRC += $(V5F_HW_TEST_SRC_ROOT)/v5f_ltdc_ui_frames.c
endif
ifeq ($(APP_V5F_HW_TEST),ltdc)
APP_ASM += $(V5F_HW_TEST_ASSET_ROOT)/ltdc/v5f_ltdc_gray_image.S
endif
ifeq ($(APP_V5F_HW_TEST),ltdc_l8_palette_image)
APP_ASM += $(V5F_HW_TEST_ASSET_ROOT)/ltdc/v5f_ltdc_palette_image.S
endif
ifneq ($(APP_V5F_HW_TEST),none)
ifeq ($(APP_V5F_HW_TEST_ID),19)
APP_ASM += $(V5F_HW_TEST_ASSET_ROOT)/ltdc/v5f_ltdc_argb8888_probe.S
endif
endif
ifeq ($(APP_V5F_HW_TEST),flash_l8_assets)
APP_ASM += $(V5F_HW_TEST_ASSET_ROOT)/flash/v5f_ltdc_flash_assets.S
endif

# CherryUSB stack
CHERRYUSB_SRC :=
ifeq ($(APP_ENABLE_USB_TEST),1)
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/core/usbd_core.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/class/cdc/usbd_cdc_acm.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/class/hid/usbd_hid.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/osal/usb_osal_rtthread.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417_usbss.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417_usbfs.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417_usbhs.c
endif

# EVT SDK
SDK_SRC := $(wildcard $(SRC_ROOT)/Debug/*.c)
SDK_SRC += $(wildcard $(SRC_ROOT)/Core/*.c)
SDK_SRC += $(wildcard $(SRC_ROOT)/Peripheral/src/*.c)

# Startup and linker
STARTUP := $(PROJECT_ROOT)/bsp/startup_ch32h417_v5f.S
BASE_LDSCRIPT := $(PROJECT_ROOT)/bsp/linker_scripts/Link_v5f.ld
LDSCRIPT := $(BASE_LDSCRIPT)

# All sources
C_SOURCES := $(RTT_SRC) $(LIBCPU_SRC) $(BSP_SRC) $(DRV_SRC) $(APP_SRC) $(SDK_SRC) $(CHERRYUSB_SRC) $(V5F_HW_TEST_POST_USB_SRC) $(V5F_HW_TEST_TAIL_SRC)
ASM_SOURCES := $(LIBCPU_ASM) $(APP_ASM) $(STARTUP)

# Build directory
BUILD := $(BUILD_ROOT)/$(CORE)
ELF := $(BUILD)/$(PROJECT_NAME)_$(CORE).elf
ifneq ($(strip $(V5F_HW_TEST_LINKER_TRANSFORM)),)
LDSCRIPT := $(BUILD)/Link_v5f_h4v1_postpass.ld
endif

# Object file generation
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SOURCES)))
OBJS += $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SOURCES)))

# VPATH for source lookup
VPATH := $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))

# Dependency files
DEPS := $(OBJS:.o=.d)

.PHONY: all clean size

all: $(ELF) $(BUILD)/$(PROJECT_NAME)_$(CORE).hex $(BUILD)/$(PROJECT_NAME)_$(CORE).bin $(BUILD)/$(PROJECT_NAME)_$(CORE).lst
	@$(SIZE) "$(ELF)"

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "CC  $(notdir $<)"
	@$(CC) -c $(COMMON_FLAGS) $(DEFS) $(INC) -MMD -MP -MF"$(@:.o=.d)" $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	@echo "AS  $(notdir $<)"
	@$(AS) -c $(ASFLAGS) $(DEFS) $(INC) $< -o $@

ifneq ($(strip $(V5F_HW_TEST_LINKER_TRANSFORM)),)
$(LDSCRIPT): $(BASE_LDSCRIPT) $(V5F_HW_TEST_LINKER_TRANSFORM) | $(BUILD)
	@echo "LDS $(notdir $@)"
	@$(PYTHON) "$(V5F_HW_TEST_LINKER_TRANSFORM)" linker --input "$(BASE_LDSCRIPT)" --output "$@"
endif

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(PROJECT_NAME)_$(CORE).map" -lm -o $@
ifneq ($(strip $(V5F_HW_TEST_POST_LINK_PATCH)),)
	@$(PYTHON) "$(V5F_HW_TEST_POST_LINK_PATCH)" patch --elf "$@" --reference-elf "$(V5F_HW_TEST_REFERENCE_ELF)"
endif

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
