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

# RT-Thread root
RTT_ROOT     := $(PROJECT_ROOT)/rt-thread

# Defines (only board-specific ones; RT-Thread config macros are in rtconfig.h)
DEFS := -DCore_V5F -DBSP_USING_UART8 -D__RT_KERNEL_SOURCE__ -D__RT_IPC_SOURCE__ -DCONFIG_CHERRYUSB
DEFS += -DAPP_ENABLE_USB_TEST=1
APP_ENABLE_USBSS_CDC ?= 0
APP_USBSS_SKIP_FOR_V3F_OFFICIAL ?= 0
APP_ENABLE_USB2_HS_CDC ?= 0
APP_ENABLE_USB2_FS_CDC ?= 1
APP_ENABLE_USB2_HS_HID ?= 0
APP_ENABLE_CH585_BLE_BRIDGE ?= 0
APP_MAIN_LOOP_DELAY_MS ?=
DEFS += -DAPP_ENABLE_USBSS_CDC=$(APP_ENABLE_USBSS_CDC)
DEFS += -DAPP_USBSS_SKIP_FOR_V3F_OFFICIAL=$(APP_USBSS_SKIP_FOR_V3F_OFFICIAL)
DEFS += -DAPP_ENABLE_USB2_HS_CDC=$(APP_ENABLE_USB2_HS_CDC)
DEFS += -DAPP_ENABLE_USB2_FS_CDC=$(APP_ENABLE_USB2_FS_CDC)
DEFS += -DAPP_ENABLE_USB2_HS_HID=$(APP_ENABLE_USB2_HS_HID)
DEFS += -DAPP_ENABLE_CH585_BLE_BRIDGE=$(APP_ENABLE_CH585_BLE_BRIDGE)
ifneq ($(APP_MAIN_LOOP_DELAY_MS),)
DEFS += -DAPP_MAIN_LOOP_DELAY_MS=$(APP_MAIN_LOOP_DELAY_MS)
endif
DEFS += $(DEFS_EXTRA)

# Include paths
INC := -I"$(PROJECT_ROOT)" -I"$(PROJECT_ROOT)/bsp" -I"$(PROJECT_ROOT)/drivers" -I"$(PROJECT_ROOT)/libcpu"
INC += -I"$(PROJECT_ROOT)/applications"
INC += -I"$(RTT_ROOT)/include" -I"$(RTT_ROOT)/components/drivers/include"
INC += -I"$(RTT_ROOT)/components/libc/compilers/common/include"
INC += -I"$(RTT_ROOT)/components/libc/compilers/newlib"
INC += -I"$(RTT_ROOT)/components/finsh"
INC += -I"$(SRC_ROOT)/Debug" -I"$(SRC_ROOT)/Core" -I"$(SRC_ROOT)/Peripheral/inc"

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
RTT_SRC += $(RTT_ROOT)/components/drivers/serial/dev_serial.c
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
DRV_SRC := $(PROJECT_ROOT)/drivers/drv_usart.c
DRV_SRC += $(PROJECT_ROOT)/drivers/drv_gpio.c

# Application
APP_SRC := $(PROJECT_ROOT)/applications/main.c
APP_SRC += $(PROJECT_ROOT)/applications/ch585_spi_scan.c
APP_SRC += $(PROJECT_ROOT)/applications/ch585_ble_bridge.c
APP_SRC += $(PROJECT_ROOT)/applications/keyboard_engine.c
APP_SRC += $(PROJECT_ROOT)/applications/usb_cdc_dual.c
APP_SRC += $(PROJECT_ROOT)/applications/usb_hs_hid_keyboard.c

# CherryUSB stack
CHERRYUSB_SRC := $(CHERRYUSB_ROOT)/core/usbd_core.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/class/cdc/usbd_cdc_acm.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/class/hid/usbd_hid.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/osal/usb_osal_rtthread.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417_usbss.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417_usbfs.c
CHERRYUSB_SRC += $(CHERRYUSB_ROOT)/port/ch32h417/usb_dc_ch32h417_usbhs.c

# EVT SDK
SDK_SRC := $(wildcard $(SRC_ROOT)/Debug/*.c)
SDK_SRC += $(wildcard $(SRC_ROOT)/Core/*.c)
SDK_SRC += $(wildcard $(SRC_ROOT)/Peripheral/src/*.c)

# Startup and linker
STARTUP := $(PROJECT_ROOT)/bsp/startup_ch32h417_v5f.S
LDSCRIPT := $(PROJECT_ROOT)/bsp/linker_scripts/Link_v5f.ld

# All sources
C_SOURCES := $(RTT_SRC) $(LIBCPU_SRC) $(BSP_SRC) $(DRV_SRC) $(APP_SRC) $(SDK_SRC) $(CHERRYUSB_SRC)
ASM_SOURCES := $(LIBCPU_ASM) $(STARTUP)

# Build directory
BUILD := $(BUILD_ROOT)/$(CORE)
ELF := $(BUILD)/$(PROJECT_NAME)_$(CORE).elf

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

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "LD  $(notdir $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -T"$(LDSCRIPT)" -Wl,-Map,"$(BUILD)/$(PROJECT_NAME)_$(CORE).map" -lm -o $@

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
