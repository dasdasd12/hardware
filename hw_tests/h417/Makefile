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
USBFS_HID_NKRO_ROOT := $(V3F_DRIVER_ROOT)/usbfs_hid_nkro
USBFS_HID_NKRO_SRC := $(USBFS_HID_NKRO_ROOT)/src
USBFS_HID_NKRO_INC := $(USBFS_HID_NKRO_ROOT)/include
USBHS_HID_NKRO_ROOT := $(V3F_DRIVER_ROOT)/usbhs_hid_nkro
USBHS_HID_NKRO_SRC := $(USBHS_HID_NKRO_ROOT)/src
USBHS_HID_NKRO_INC := $(USBHS_HID_NKRO_ROOT)/include
WCH_USBHS_COMPAT_HID_ROOT := $(V3F_DRIVER_ROOT)/wch_usbhs_compat_hid
WCH_USBHS_COMPAT_HID_SRC := $(WCH_USBHS_COMPAT_HID_ROOT)/src
WCH_USBHS_COMPAT_HID_INC := $(WCH_USBHS_COMPAT_HID_ROOT)/include
WCH_USBHS_COMPAT_HID_SYSTEM := $(WCH_USBHS_COMPAT_HID_ROOT)/system/v3f/system_ch32h417.c
H417_USBHS_DISABLE_SWD ?= 1
OFFICIAL_USBSS_EXAMPLE_ROOT ?= C:/program1/hardware/WCH/CH32H417/CH32H417EVT/EVT/EXAM/USBSS/DEVICE/CH372Device
OFFICIAL_USBSS_COMMON_ROOT := $(OFFICIAL_USBSS_EXAMPLE_ROOT)/Common
OFFICIAL_USBSS_V3F_USER_ROOT := $(OFFICIAL_USBSS_EXAMPLE_ROOT)/V3F/User
V3F_STANDALONE_ROOT := cases/v3f_standalone
V3F_STANDALONE_SRC_ROOT := $(V3F_STANDALONE_ROOT)/src
H417_V3F_WAKE_STUB_TEST := h417_v5f_wake_stub
H417_DUAL_CORE_TESTS := h417_v5f_ltdc h417_v5f_ltdc_l8_palette_image h417_v5f_ltdc_rgb565_diag h417_v5f_ltdc_ui_frames h417_v5f_gpha_r2m_fill h417_v5f_gpha_pfc_l8_rgb565 h417_v5f_gpha_blend_rgb565 h417_v5f_gpha_l8_ltdc_fullscreen h417_v5f_flash h417_v5f_flash_l8_assets h417_v5f_tick_diag h417_v5f_sdram_memtest h417_v5f_sdram_video h417_v5f_sdram_video_h4v1 h417_v5f_sdram_video_h4v1_120 h417_v5f_sdram_video_h4v1_165 h417_v5f_flash_h4v1_install h417_v5f_flash_h4v1_write_probe h417_v5f_flash_h4v1_165_write_probe h417_v5f_flash_h4v1_165_install h417_v5f_flash_h4v1_165_coldboot h417_v5f_ch585_spi_speed h417_v5f_ch585_adc_key_cal
IS_DUAL_CORE_TEST := $(filter $(HW_TEST),$(H417_DUAL_CORE_TESTS))
APP_V5F_HW_TEST_ID_OVERRIDE :=
APP_V5F_HW_TEST_USB_CDC :=
APP_V5F_HW_TEST_SOURCE_ARGS :=
H417_HW_TEST_BUILD_NAME := $(HW_TEST)
H417_REFERENCE_HW_TEST :=
H417_REFERENCE_EXTEND_V5 :=
H417_REFERENCE_REBUILD_V5_MAIN :=
H417_REFERENCE_V3_BIN_SHA256 :=
H417_REFERENCE_V5_BIN_SHA256 :=
H417_REFERENCE_V5_ELF_SHA256 :=
H417_REFERENCE_V5_OBJECTS_SHA256 :=

ifeq ($(HW_TEST),h417_v5f_ltdc)
APP_V5F_HW_TEST_MODE := ltdc
else ifeq ($(HW_TEST),h417_v5f_ltdc_l8_palette_image)
APP_V5F_HW_TEST_MODE := ltdc_l8_palette_image
else ifeq ($(HW_TEST),h417_v5f_ltdc_rgb565_diag)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
else ifeq ($(HW_TEST),h417_v5f_ltdc_ui_frames)
APP_V5F_HW_TEST_MODE := ltdc_ui_frames
APP_V5F_HW_TEST_ID_OVERRIDE := 18
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
APP_V5F_HW_TEST_USB_CDC := 1
else ifeq ($(HW_TEST),h417_v5f_sdram_video)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
APP_V5F_HW_TEST_SOURCE_ARGS := DEFS_EXTRA="-DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1"
else ifeq ($(HW_TEST),h417_v5f_sdram_video_h4v1)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC=h4v1_video.c V5F_HW_TEST_LINK_AFTER_USB=1 DEFS_EXTRA="-DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
else ifeq ($(HW_TEST),h417_v5f_sdram_video_h4v1_120)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
# Isolated 120-frame qualification target.  It deliberately has no Flash
# post-PASS tail.  Freeze every qualified object except the H4V1 test main so
# the timing-sensitive USB/CDC path retains its exact machine code and VMA.
H417_REFERENCE_HW_TEST := h417_v5f_sdram_video_h4v1
H417_REFERENCE_REBUILD_V5_MAIN := 1
H417_REFERENCE_V3_BIN_SHA256 := a78380eee6ddbc7711f7029b85caddfdf23b6f87640a69c993291e751e10897b
H417_REFERENCE_V5_BIN_SHA256 := ddab4a5592a42f592058621f16f0261a14e18719e9f516f9cb156128b380599d
H417_REFERENCE_V5_ELF_SHA256 := 6c9b0fbc7b0fe1413a1f1fcad87897f294b18d9659379a282d147625b11f5133
H417_REFERENCE_V5_OBJECTS_SHA256 := b9ad2fad02774fbe107dd89d1fc7c99d193deebef4a483c1abb9fcca00337730
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC="h4v1_video.c h4v1_chunk120_gate.c" V5F_HW_TEST_LINK_AFTER_USB=1 DEFS_EXTRA="-DV5F_SDRAM_H4V1_CHUNKED_FRAMES=120u -DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
else ifeq ($(HW_TEST),h417_v5f_sdram_video_h4v1_165)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
# Full-source 165-frame qualification target.  Clone the hardware-qualified
# 120-frame image and all 99 V5F objects, then rebuild only the H4V1 main.
H417_REFERENCE_HW_TEST := h417_v5f_sdram_video_h4v1_120
H417_REFERENCE_REBUILD_V5_MAIN := 1
H417_REFERENCE_V3_BIN_SHA256 := a78380eee6ddbc7711f7029b85caddfdf23b6f87640a69c993291e751e10897b
H417_REFERENCE_V5_BIN_SHA256 := 1c9a77f316afec3393c35e30d174bf4d7ce8bb22defdb7cbf6cb08ebb1a4d261
H417_REFERENCE_V5_ELF_SHA256 := 174246c4bd5bed50ee643932041c3db1789eab577eebd91bb71cd720254c430d
H417_REFERENCE_V5_OBJECTS_SHA256 := 94cb62c11376d10ca520642fbf0bc8527e4414f612b801671e4572808980b061
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC="h4v1_video.c h4v1_chunk120_gate.c" V5F_HW_TEST_LINK_AFTER_USB=1 DEFS_EXTRA="-DV5F_SDRAM_H4V1_CHUNKED_FRAMES=165u -DV5F_SDRAM_H4V1_MAX_FRAMES=165u -DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
else ifeq ($(HW_TEST),h417_v5f_flash_h4v1_install)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
# Reuse the qualified H4V1 V3F image and all qualified V5F objects.  The only
# new V5F objects live in an isolated post-image tail and are reached by one
# fail-closed post-link hook after RESULT PASS.
H417_REFERENCE_HW_TEST := h417_v5f_sdram_video_h4v1
H417_REFERENCE_EXTEND_V5 := 1
H417_REFERENCE_V3_BIN_SHA256 := a78380eee6ddbc7711f7029b85caddfdf23b6f87640a69c993291e751e10897b
H417_REFERENCE_V5_BIN_SHA256 := ddab4a5592a42f592058621f16f0261a14e18719e9f516f9cb156128b380599d
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC=h4v1_video.c V5F_HW_TEST_LINK_AFTER_USB=1 V5F_HW_TEST_TAIL_SRC="../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage1.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage2.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage3.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage4.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_hook.c" V5F_HW_TEST_LINKER_TRANSFORM=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_POST_LINK_PATCH=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_REFERENCE_ELF=../../../hw_tests/h417/$(BUILD_ROOT)/h417_v5f_sdram_video_h4v1/V5F/rtthread_ch32h417_V5F.elf DEFS_EXTRA="-DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
else ifeq ($(HW_TEST),h417_v5f_flash_h4v1_write_probe)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
# Explicit destructive qualification target.  It reuses the exact qualified
# H4V1 images/objects, but substitutes a dedicated Stage1..5 tail and hook.
# The read-only h417_v5f_flash_h4v1_install target above remains unchanged.
H417_REFERENCE_HW_TEST := h417_v5f_sdram_video_h4v1
H417_REFERENCE_EXTEND_V5 := 1
H417_REFERENCE_V3_BIN_SHA256 := a78380eee6ddbc7711f7029b85caddfdf23b6f87640a69c993291e751e10897b
H417_REFERENCE_V5_BIN_SHA256 := ddab4a5592a42f592058621f16f0261a14e18719e9f516f9cb156128b380599d
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC=h4v1_video.c V5F_HW_TEST_LINK_AFTER_USB=1 V5F_HW_TEST_TAIL_SRC="../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage1.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage2.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage3.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage4.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage5.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_write_probe_hook.c" V5F_HW_TEST_LINKER_TRANSFORM=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_POST_LINK_PATCH=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_REFERENCE_ELF=../../../hw_tests/h417/$(BUILD_ROOT)/h417_v5f_sdram_video_h4v1/V5F/rtthread_ch32h417_V5F.elf DEFS_EXTRA="-DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
else ifeq ($(HW_TEST),h417_v5f_flash_h4v1_165_write_probe)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
# Destructive Stage5 qualification rebased on the exact 165-frame image that
# passed on hardware.  All existing V3/V5 objects are copied verbatim; only
# isolated post-PASS tail objects are added.
H417_REFERENCE_HW_TEST := h417_v5f_sdram_video_h4v1_165
H417_REFERENCE_EXTEND_V5 := 1
H417_REFERENCE_V3_BIN_SHA256 := a78380eee6ddbc7711f7029b85caddfdf23b6f87640a69c993291e751e10897b
H417_REFERENCE_V5_BIN_SHA256 := 15aa43e5f698ce1a5c63ad30ca9af0fd1462ebf563877ef8cea5a6dfc17931db
H417_REFERENCE_V5_ELF_SHA256 := 60de05b7f53f70315309fd4f3b8967707738c61904e414223fa6665386ef54fb
H417_REFERENCE_V5_OBJECTS_SHA256 := 3290162d0734dd984b2bc1d59c0dffb0df8de1a5107cbb12863d2348cd2a2f58
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC="h4v1_video.c h4v1_chunk120_gate.c" V5F_HW_TEST_LINK_AFTER_USB=1 V5F_HW_TEST_TAIL_SRC="../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage1.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage2.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage3.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage4.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage5.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_write_probe_hook.c" V5F_HW_TEST_LINKER_TRANSFORM=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_POST_LINK_PATCH=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_REFERENCE_ELF=../../../hw_tests/h417/$(BUILD_ROOT)/h417_v5f_sdram_video_h4v1_165/V5F/rtthread_ch32h417_V5F.elf DEFS_EXTRA="-DV5F_SDRAM_H4V1_CHUNKED_FRAMES=165u -DV5F_SDRAM_H4V1_MAX_FRAMES=165u -DH4V1_FLASH_WRITE_PROBE_FRAMES=165u -DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
else ifeq ($(HW_TEST),h417_v5f_flash_h4v1_165_install)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
# Transactional USB -> SDRAM -> NAND installer rebased on the exact 165-frame
# hardware-qualified image.  The 99 qualified V5F objects remain byte exact;
# only Stage1, Stage6 and the dedicated post-PASS hook occupy the isolated tail.
H417_REFERENCE_HW_TEST := h417_v5f_sdram_video_h4v1_165
H417_REFERENCE_EXTEND_V5 := 1
H417_REFERENCE_V3_BIN_SHA256 := a78380eee6ddbc7711f7029b85caddfdf23b6f87640a69c993291e751e10897b
H417_REFERENCE_V5_BIN_SHA256 := 15aa43e5f698ce1a5c63ad30ca9af0fd1462ebf563877ef8cea5a6dfc17931db
H417_REFERENCE_V5_ELF_SHA256 := 60de05b7f53f70315309fd4f3b8967707738c61904e414223fa6665386ef54fb
H417_REFERENCE_V5_OBJECTS_SHA256 := 3290162d0734dd984b2bc1d59c0dffb0df8de1a5107cbb12863d2348cd2a2f58
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC="h4v1_video.c h4v1_chunk120_gate.c" V5F_HW_TEST_LINK_AFTER_USB=1 V5F_HW_TEST_TAIL_SRC="../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage1.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_postpass_stage6.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_install_hook.c" V5F_HW_TEST_LINKER_TRANSFORM=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_POST_LINK_PATCH=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_REFERENCE_ELF=../../../hw_tests/h417/$(BUILD_ROOT)/h417_v5f_sdram_video_h4v1_165/V5F/rtthread_ch32h417_V5F.elf DEFS_EXTRA="-DV5F_SDRAM_H4V1_CHUNKED_FRAMES=165u -DV5F_SDRAM_H4V1_MAX_FRAMES=165u -DH4V1_FLASH_INSTALL_FRAMES=165u -DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
else ifeq ($(HW_TEST),h417_v5f_flash_h4v1_165_coldboot)
APP_V5F_HW_TEST_MODE := ltdc_rgb565_diag
APP_V5F_HW_TEST_ID_OVERRIDE := 19
APP_V5F_HW_TEST_USB_CDC := 1
# Read-only committed-Flash cold boot rebased on the exact hardware-qualified
# 165-frame image.  The 99 qualified objects remain byte exact.  A dedicated
# tail supplies only the fixed config, NAND source and status replay adapters;
# Stage6 and every NAND-mutating command are deliberately absent.
H417_REFERENCE_HW_TEST := h417_v5f_sdram_video_h4v1_165
H417_REFERENCE_EXTEND_V5 := 1
H417_REFERENCE_V3_BIN_SHA256 := a78380eee6ddbc7711f7029b85caddfdf23b6f87640a69c993291e751e10897b
H417_REFERENCE_V5_BIN_SHA256 := 15aa43e5f698ce1a5c63ad30ca9af0fd1462ebf563877ef8cea5a6dfc17931db
H417_REFERENCE_V5_ELF_SHA256 := 60de05b7f53f70315309fd4f3b8967707738c61904e414223fa6665386ef54fb
H417_REFERENCE_V5_OBJECTS_SHA256 := 3290162d0734dd984b2bc1d59c0dffb0df8de1a5107cbb12863d2348cd2a2f58
APP_V5F_HW_TEST_SOURCE_ARGS := V5F_HW_TEST_MAIN=v5f_hw_test_h4v1.c V5F_HW_TEST_EXTRA_SRC="h4v1_video.c h4v1_chunk120_gate.c" V5F_HW_TEST_LINK_AFTER_USB=1 V5F_HW_TEST_TAIL_SRC="../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_coldboot_core.c ../../../hw_tests/h417/cases/v5f_rtthread/src/h4v1_flash_coldboot_hook.c" V5F_HW_TEST_LINKER_TRANSFORM=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_POST_LINK_PATCH=../../../hw_tests/h417/tools/h4v1_postpass_elf.py V5F_HW_TEST_REFERENCE_ELF=../../../hw_tests/h417/$(BUILD_ROOT)/h417_v5f_sdram_video_h4v1_165/V5F/rtthread_ch32h417_V5F.elf DEFS_EXTRA="-DV5F_SDRAM_H4V1_CHUNKED_FRAMES=165u -DV5F_SDRAM_H4V1_MAX_FRAMES=165u -DH4V1_FLASH_COLD_BOOT_FRAMES=165u -DAPP_USB_CDC_RAW_RX_BUFFER_ADDR=0x20160000u -DAPP_USB_CDC_RAW_RX_BUFFER_BYTES=65536u -DAPP_USBFS_STREAM_DIAG=1 -DAPP_USBFS_CLEAR_TRANSFER_BEFORE_CALLBACK=1"
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

ifeq ($(H417_REFERENCE_HW_TEST),)
all:
	@$(MAKE) HW_TEST=$(H417_V3F_WAKE_STUB_TEST) BUILD_ROOT=$(H417_V3F_WAKE_BUILD_ROOT) H417_LOCAL_V3F_PROJECT=h417_V3F H417_LOCAL_V3F_BUILD_DIR=$(H417_V3F_WAKE_BUILD_ROOT)
ifeq ($(APP_V5F_HW_TEST_ID_OVERRIDE),)
	@$(MAKE) -C "$(H417_FIRMWARE_ROOT)/v5f_rtthread" PREFIX=$(PREFIX) BUILD_ROOT=$(DIRECT_V5F_BUILD_ROOT) CORE=V5F APP_V5F_HW_TEST=$(APP_V5F_HW_TEST_MODE) $(APP_V5F_HW_TEST_USB_CDC_ARGS) $(APP_V5F_HW_TEST_SOURCE_ARGS)
else
	@$(MAKE) -C "$(H417_FIRMWARE_ROOT)/v5f_rtthread" PREFIX=$(PREFIX) BUILD_ROOT=$(DIRECT_V5F_BUILD_ROOT) CORE=V5F APP_V5F_HW_TEST=$(APP_V5F_HW_TEST_MODE) APP_V5F_HW_TEST_ID=$(APP_V5F_HW_TEST_ID_OVERRIDE) $(APP_V5F_HW_TEST_USB_CDC_ARGS) $(APP_V5F_HW_TEST_SOURCE_ARGS)
endif
	@echo "V3F build: $(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F"
	@echo "V5F build: $(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F"
else
ifeq ($(H417_REFERENCE_REBUILD_V5_MAIN),1)
all:
	@ref_v3="$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V3F/h417_V3F.bin"; \
	ref_v5="$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F/rtthread_ch32h417_V5F.bin"; \
	ref_elf="$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F/rtthread_ch32h417_V5F.elf"; \
	test -f "$$ref_v3" -a -f "$$ref_v5" -a -f "$$ref_elf" || { \
		echo "Qualified H4V1 reference artifacts are missing; refusing to rebuild them implicitly."; \
		exit 1; \
	}; \
	actual_v3=$$(sha256sum "$$ref_v3" | awk '{print $$1}'); \
	actual_v5=$$(sha256sum "$$ref_v5" | awk '{print $$1}'); \
	actual_elf=$$(sha256sum "$$ref_elf" | awk '{print $$1}'); \
	actual_objects=$$(cd "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F" && sha256sum *.o | sha256sum | awk '{print $$1}'); \
	test "$$actual_v3" = "$(H417_REFERENCE_V3_BIN_SHA256)" || { \
		echo "Qualified V3F hash mismatch: $$actual_v3"; exit 1; \
	}; \
	test "$$actual_v5" = "$(H417_REFERENCE_V5_BIN_SHA256)" || { \
		echo "Qualified V5F hash mismatch: $$actual_v5"; exit 1; \
	}; \
	test "$$actual_elf" = "$(H417_REFERENCE_V5_ELF_SHA256)" || { \
		echo "Qualified V5F ELF hash mismatch: $$actual_elf"; exit 1; \
	}; \
	test "$$actual_objects" = "$(H417_REFERENCE_V5_OBJECTS_SHA256)" || { \
		echo "Qualified V5F object-set hash mismatch: $$actual_objects"; exit 1; \
	}; \
	echo "Qualified H4V1 reference hashes PASS"
	@mkdir -p "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F" "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F"
	@for ext in bin elf hex lst map; do \
		cp -f "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V3F/h417_V3F.$$ext" "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F/h417_V3F.$$ext"; \
	done
	@rm -f "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/"*.o \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/"*.d \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.elf" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.hex" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.bin" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.lst" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.map"
	@cp -f "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F/"*.o "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/"
	@touch "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/"*.o
	@rm -f "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/v5f_hw_test_h4v1.o"
	@$(MAKE) -C "$(H417_FIRMWARE_ROOT)/v5f_rtthread" PREFIX=$(PREFIX) BUILD_ROOT=$(DIRECT_V5F_BUILD_ROOT) CORE=V5F APP_V5F_HW_TEST=$(APP_V5F_HW_TEST_MODE) APP_V5F_HW_TEST_ID=$(APP_V5F_HW_TEST_ID_OVERRIDE) $(APP_V5F_HW_TEST_USB_CDC_ARGS) $(APP_V5F_HW_TEST_SOURCE_ARGS)
	@echo "Qualified H4V1 objects + rebuilt V5 test main: $(H417_REFERENCE_HW_TEST) -> $(H417_HW_TEST_BUILD_NAME)"
else
ifeq ($(H417_REFERENCE_EXTEND_V5),1)
all:
	@ref_v3="$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V3F/h417_V3F.bin"; \
	ref_v5="$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F/rtthread_ch32h417_V5F.bin"; \
	test -f "$$ref_v3" -a -f "$$ref_v5" || { \
		echo "Qualified H4V1 reference artifacts are missing; refusing to rebuild them implicitly."; \
		exit 1; \
	}; \
	actual_v3=$$(sha256sum "$$ref_v3" | awk '{print $$1}'); \
	actual_v5=$$(sha256sum "$$ref_v5" | awk '{print $$1}'); \
	test "$$actual_v3" = "$(H417_REFERENCE_V3_BIN_SHA256)" || { \
		echo "Qualified V3F hash mismatch: $$actual_v3"; exit 1; \
	}; \
	test "$$actual_v5" = "$(H417_REFERENCE_V5_BIN_SHA256)" || { \
		echo "Qualified V5F hash mismatch: $$actual_v5"; exit 1; \
	}; \
	if test -n "$(H417_REFERENCE_V5_ELF_SHA256)"; then \
		ref_elf="$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F/rtthread_ch32h417_V5F.elf"; \
		test -f "$$ref_elf" || { echo "Qualified V5F ELF is missing"; exit 1; }; \
		actual_elf=$$(sha256sum "$$ref_elf" | awk '{print $$1}'); \
		test "$$actual_elf" = "$(H417_REFERENCE_V5_ELF_SHA256)" || { \
			echo "Qualified V5F ELF hash mismatch: $$actual_elf"; exit 1; \
		}; \
	fi; \
	if test -n "$(H417_REFERENCE_V5_OBJECTS_SHA256)"; then \
		actual_objects=$$(cd "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F" && sha256sum *.o | sha256sum | awk '{print $$1}'); \
		test "$$actual_objects" = "$(H417_REFERENCE_V5_OBJECTS_SHA256)" || { \
			echo "Qualified V5F object-set hash mismatch: $$actual_objects"; exit 1; \
		}; \
	fi; \
	echo "Qualified H4V1 reference hashes PASS"
	@mkdir -p "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F" "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F"
	@for ext in bin elf hex lst map; do \
		cp -f "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V3F/h417_V3F.$$ext" "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F/h417_V3F.$$ext"; \
	done
	@cp -f "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F/"*.o "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/"
	@rm -f "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.elf" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.hex" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.bin" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.lst" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.map" \
		"$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/Link_v5f_h4v1_postpass.ld"
	@$(MAKE) -C "$(H417_FIRMWARE_ROOT)/v5f_rtthread" PREFIX=$(PREFIX) BUILD_ROOT=$(DIRECT_V5F_BUILD_ROOT) CORE=V5F APP_V5F_HW_TEST=$(APP_V5F_HW_TEST_MODE) APP_V5F_HW_TEST_ID=$(APP_V5F_HW_TEST_ID_OVERRIDE) $(APP_V5F_HW_TEST_USB_CDC_ARGS) $(APP_V5F_HW_TEST_SOURCE_ARGS)
	@echo "Reference V3F + isolated post-PASS V5F: $(H417_REFERENCE_HW_TEST) -> $(H417_HW_TEST_BUILD_NAME)"
else
all:
	@$(MAKE) HW_TEST=$(H417_REFERENCE_HW_TEST) BUILD_ROOT=$(BUILD_ROOT)
	@mkdir -p "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F" "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F"
	@for ext in bin elf hex lst map; do \
		cp -f "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V3F/h417_V3F.$$ext" "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F/h417_V3F.$$ext"; \
		cp -f "$(BUILD_ROOT)/$(H417_REFERENCE_HW_TEST)/V5F/rtthread_ch32h417_V5F.$$ext" "$(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V5F/rtthread_ch32h417_V5F.$$ext"; \
	done
	@echo "Reference clone: $(H417_REFERENCE_HW_TEST) -> $(H417_HW_TEST_BUILD_NAME)"
endif
endif
endif

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
TEST_SRC += $(WCH_USBFS_COMPAT_HID_SRC)/usbfs_desc.c
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
else ifeq ($(HW_TEST),h417_v3f_usbss_ch372)
ifeq ($(wildcard $(OFFICIAL_USBSS_COMMON_ROOT)/ch32h417_usbss_device.c),)
$(error WCH CH372Device example not found at '$(OFFICIAL_USBSS_EXAMPLE_ROOT)'; override OFFICIAL_USBSS_EXAMPLE_ROOT)
endif
LOCAL_V3F_USE_TEST_MAIN := 0
SYSTEM_SRC := $(OFFICIAL_USBSS_V3F_USER_ROOT)/system_ch32h417.c
TEST_SRC := $(OFFICIAL_USBSS_V3F_USER_ROOT)/main.c
TEST_SRC += $(OFFICIAL_USBSS_COMMON_ROOT)/hardware.c
TEST_SRC += $(OFFICIAL_USBSS_COMMON_ROOT)/ch32h417_usbss_device.c
TEST_SRC += $(OFFICIAL_USBSS_COMMON_ROOT)/ch32h417_usbss_it.c
TEST_SRC += $(OFFICIAL_USBSS_COMMON_ROOT)/ch32h417_usbhs_device.c
TEST_SRC += $(OFFICIAL_USBSS_COMMON_ROOT)/usb_desc.c
INC := -I$(OFFICIAL_USBSS_V3F_USER_ROOT) -I$(OFFICIAL_USBSS_COMMON_ROOT) $(INC)
DEFS += -DRun_Core=Run_Core_V3F
EXTRA_SDK_SRC += $(SRC_ROOT)/Debug/debug.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_dbgmcu.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_flash.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_tim.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_usart.c
else ifeq ($(HW_TEST),h417_v3f_usbss_fs_diag)
ifeq ($(wildcard $(OFFICIAL_USBSS_COMMON_ROOT)/ch32h417_usbss_device.c),)
$(error WCH CH372Device example not found at '$(OFFICIAL_USBSS_EXAMPLE_ROOT)'; override OFFICIAL_USBSS_EXAMPLE_ROOT)
endif
LOCAL_V3F_USE_TEST_MAIN := 0
SYSTEM_SRC := $(H417_FIRMWARE_ROOT)/v3f/bsp/system_ch32h417.c
TEST_SRC := $(WCH_USBFS_COMPAT_HID_SRC)/ch32h417_usbfs_device.c
TEST_SRC += $(WCH_USBFS_COMPAT_HID_SRC)/usbfs_desc.c
TEST_SRC += $(USBFS_HID_NKRO_SRC)/ch32h417_usbfs_hid_nkro.c
TEST_SRC += $(USBHS_HID_NKRO_SRC)/ch32h417_usbhs_hid_nkro.c
TEST_SRC += $(H417_FIRMWARE_ROOT)/v3f/applications/board_init.c
TEST_SRC += $(H417_FIRMWARE_ROOT)/v3f/applications/approval_mailbox.c
TEST_SRC += $(V3F_STANDALONE_SRC_ROOT)/h417_product_usbhs_baseline.c
TEST_SRC += $(V3F_STANDALONE_SRC_ROOT)/h417_usbfs_cdc_diag.c
TEST_SRC += $(V3F_STANDALONE_SRC_ROOT)/h417_usbss_fs_diag.c
TEST_SRC += $(V3F_STANDALONE_SRC_ROOT)/h417_usbss_diag_official.c
TEST_SRC += $(OFFICIAL_USBSS_COMMON_ROOT)/ch32h417_usbss_it.c
TEST_SRC += $(OFFICIAL_USBSS_COMMON_ROOT)/usb_desc.c
INC := -I$(H417_FIRMWARE_ROOT)/v3f/bsp -I$(H417_FIRMWARE_ROOT)/v3f/applications -I$(OFFICIAL_USBSS_V3F_USER_ROOT) -I$(OFFICIAL_USBSS_COMMON_ROOT) $(INC)
INC := -I$(WCH_USBHS_COMPAT_HID_SRC) -I$(WCH_USBHS_COMPAT_HID_INC) $(INC)
INC += -I$(USBFS_HID_NKRO_INC) -I$(WCH_USBFS_COMPAT_HID_INC) -I$(USBHS_HID_NKRO_INC)
INC += -I$(H417_FIRMWARE_ROOT)/common
INC += -I$(H417_FIRMWARE_ROOT)/../common
DEFS += -DRun_Core=Run_Core_V3F
DEFS += -DV3F_ENABLE_USBFS_CDC=1 -DV3F_ENABLE_USBFS_CDC_DEBUG=1
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_dbgmcu.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_flash.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_iwdg.c
EXTRA_SDK_SRC += $(SRC_ROOT)/Peripheral/src/ch32h417_tim.c
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
ifeq ($(HW_TEST),h417_v3f_usbss_fs_diag)
STARTUP := $(H417_FIRMWARE_ROOT)/v3f/bsp/startup_ch32h417_v3f.S
LDSCRIPT := $(V3F_STANDALONE_ROOT)/Link_h417_v3f_usbss_diag.ld
else ifeq ($(HW_TEST),h417_v3f_usbss_ch372)
LDSCRIPT := $(SRC_ROOT)/Ld/V3F/Link_v3f.ld
endif

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
