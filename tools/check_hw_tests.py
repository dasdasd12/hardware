import io
import os
import re
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
H417_ROOT = os.path.join(ROOT, "hw_tests", "h417")
CH585_ROOT = os.path.join(ROOT, "hw_tests", "ch585")
H417_FIRMWARE_ROOT = os.path.join(ROOT, "firmware", "h417")
CH585_FIRMWARE_ROOT = os.path.join(ROOT, "firmware", "ch585")
H417_WCH_ROOT = os.path.join(H417_FIRMWARE_ROOT, "basic", "wch", "SRC")
CH585_WCH_ROOT = os.path.join(CH585_FIRMWARE_ROOT, "basic", "wch", "SRC")
H417_DRIVER_ROOT = os.path.join(H417_FIRMWARE_ROOT, "drivers")
H417_V3F_DRIVER_ROOT = os.path.join(H417_FIRMWARE_ROOT, "v3f", "drivers")
H417_RGB1W_ROOT = os.path.join(H417_V3F_DRIVER_ROOT, "rgb1w_pioc")
H417_FLASH_NAND_ROOT = os.path.join(H417_DRIVER_ROOT, "gd5f1g_spi_nand")
H417_LTDC_RGB_ROOT = os.path.join(H417_DRIVER_ROOT, "ltdc_rgb")
H417_GPHA_2D_ROOT = os.path.join(H417_DRIVER_ROOT, "gpha_2d")
H417_SDRAM_DRIVER_ROOT = os.path.join(H417_DRIVER_ROOT, "sdram")
H417_V3F_TEST_ROOT = os.path.join(H417_ROOT, "passed", "v3f_standalone")
H417_V3F_TEST_SRC_ROOT = os.path.join(H417_V3F_TEST_ROOT, "src")
H417_V5F_TEST_ROOT = os.path.join(H417_ROOT, "passed", "v5f_rtthread")
H417_V5F_TEST_SRC = os.path.join(H417_V5F_TEST_ROOT, "src", "v5f_hw_test.c")
H417_USB_CDC_SOURCE = os.path.join(H417_FIRMWARE_ROOT, "v5f_rtthread", "applications", "usb_cdc_dual.c")


def fail(message):
    print("FAIL: {0}".format(message))
    sys.exit(1)


def read_text(path):
    if not os.path.exists(path):
        fail("missing {0}".format(os.path.relpath(path, ROOT)))
    with io.open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def assert_contains(path, pattern, description):
    text = read_text(path)
    if re.search(pattern, text, flags=re.MULTILINE) is None:
        fail("{0} missing {1}".format(os.path.relpath(path, ROOT), description))


def assert_not_contains(path, pattern, description):
    text = read_text(path)
    if re.search(pattern, text, flags=re.MULTILINE) is not None:
        fail("{0} contains forbidden {1}".format(os.path.relpath(path, ROOT), description))


def assert_exists(path, description):
    if not os.path.exists(path):
        fail("missing {0}: {1}".format(os.path.relpath(path, ROOT), description))


def assert_not_exists(path, description):
    if os.path.exists(path):
        fail("{0} should not exist: {1}".format(os.path.relpath(path, ROOT), description))


def scan_tree(path, suffixes):
    if not os.path.exists(path):
        fail("missing {0}".format(os.path.relpath(path, ROOT)))
    data = []
    for base, _dirs, files in os.walk(path):
        for name in sorted(files):
            child = os.path.join(base, name)
            _root, ext = os.path.splitext(child)
            if ext.lower() in suffixes:
                data.append("\n/* {0} */\n".format(os.path.relpath(child, ROOT)))
                with io.open(child, "r", encoding="utf-8", errors="ignore") as handle:
                    data.append(handle.read())
    if not data:
        fail("no source files under {0}".format(os.path.relpath(path, ROOT)))
    return "".join(data)


def scan_paths(path):
    if not os.path.exists(path):
        fail("missing {0}".format(os.path.relpath(path, ROOT)))
    paths = []
    for base, dirs, files in os.walk(path):
        for name in sorted(dirs + files):
            child = os.path.join(base, name)
            paths.append(os.path.relpath(child, ROOT).replace(os.sep, "/"))
    return paths


def assert_ch585_firmware_has_no_test_residue():
    allowed_main_paths = {
        "firmware/ch585/drivers/rf/include/rf_test.h",
    }
    test_residue_patterns = (
        r"(^|/)tests?(/|$)",
        r"(^|/)spi_slave_test(/|$)",
        r"(^|/)[^/]*(selftest|_test|test_|bringup|probe|example|demo)[^/]*($|/|\.)",
        r"(^|/)EVT/EXAM(/|$)",
    )

    for relpath in scan_paths(CH585_FIRMWARE_ROOT):
        if relpath in allowed_main_paths:
            continue
        for pattern in test_residue_patterns:
            if re.search(pattern, relpath, flags=re.IGNORECASE):
                fail("CH585 firmware test residue: {0}".format(relpath))


def main():
    h417_makefile = os.path.join(H417_ROOT, "Makefile")
    ch585_makefile = os.path.join(CH585_ROOT, "Makefile")

    assert_contains(h417_makefile, r"\bHW_TEST\s*\?=", "HW_TEST selection")
    assert_contains(h417_makefile, r"firmware/h417", "H417 firmware-owned dependency root")
    assert_contains(h417_makefile, r"WCH_H417_SRC_ROOT\s*:=\s*\$\(H417_FIRMWARE_ROOT\)/basic/wch/SRC", "H417-local WCH source tree")
    assert_contains(h417_makefile, r"H417_DRIVER_ROOT\s*:=\s*\$\(H417_FIRMWARE_ROOT\)/drivers", "H417 shared driver root")
    assert_contains(h417_makefile, r"V3F_DRIVER_ROOT\s*:=\s*\$\(H417_FIRMWARE_ROOT\)/v3f/drivers", "V3F driver root")
    assert_contains(h417_makefile, r"RGB1W_PIOC_ROOT\s*:=\s*\$\(V3F_DRIVER_ROOT\)/rgb1w_pioc", "V3F RGB1W PIOC driver tree")
    assert_contains(h417_makefile, r"FLASH_NAND_ROOT\s*:=\s*\$\(H417_DRIVER_ROOT\)/gd5f1g_spi_nand", "H417 shared GD5F1G driver tree")
    assert_contains(h417_makefile, r"LTDC_RGB_ROOT\s*:=\s*\$\(H417_DRIVER_ROOT\)/ltdc_rgb", "H417 shared LTDC RGB driver tree")
    assert_contains(h417_makefile, r"V3F_STANDALONE_ROOT\s*:=\s*passed/v3f_standalone", "H417 passed V3F standalone test root")
    assert_contains(h417_makefile, r"H417_DUAL_CORE_TESTS\s*:=", "H417 dual-core test wrapper list")
    assert_contains(h417_makefile, r"H417_HW_TEST_BUILD_NAME\s*:=\s*\$\(HW_TEST\)", "H417 default build name")
    assert_contains(h417_makefile, r"DUAL_CORE_BUILD_ROOT\s*:=\s*\.\./\.\./hw_tests/h417/\$\(BUILD_ROOT\)/\$\(H417_HW_TEST_BUILD_NAME\)", "dual-core test build root")
    assert_contains(h417_makefile, r"APP_V5F_HW_TEST=\$\(APP_V5F_HW_TEST_MODE\)", "V5F test mode forwarding")
    assert_not_contains(h417_makefile, r"third_party|EVT_ROOT", "external third_party EVT dependency")
    assert_contains(ch585_makefile, r"\bTEST\s*\?=", "TEST selection")
    assert_contains(ch585_makefile, r"\bHALF\s*\?=", "HALF selection")
    assert_contains(ch585_makefile, r"firmware/ch585", "CH585 firmware-owned dependency root")
    assert_contains(ch585_makefile, r"WCH_CH585_SRC_ROOT\s*:=\s*\$\(CH585_FIRMWARE_ROOT\)/basic/wch/SRC", "CH585-local WCH source tree")
    assert_not_contains(ch585_makefile, r"CH585_EVT_ROOT|EVT/EXAM|C:/program1/hardware", "external CH585 EVT dependency")
    for path, description in (
        (os.path.join(CH585_WCH_ROOT, "RVMSIS", "core_riscv.h"), "CH585 RVMSIS core header"),
        (os.path.join(CH585_WCH_ROOT, "StdPeriphDriver", "inc", "CH585SFR.h"), "CH585 SFR header"),
        (os.path.join(CH585_WCH_ROOT, "StdPeriphDriver", "CH58x_clk.c"), "CH585 clock driver"),
        (os.path.join(CH585_WCH_ROOT, "StdPeriphDriver", "CH58x_gpio.c"), "CH585 GPIO driver"),
        (os.path.join(CH585_WCH_ROOT, "StdPeriphDriver", "CH58x_sys.c"), "CH585 system driver"),
        (os.path.join(CH585_WCH_ROOT, "StdPeriphDriver", "CH58x_uart1.c"), "CH585 UART1 driver"),
        (os.path.join(CH585_WCH_ROOT, "StdPeriphDriver", "libISP585.a"), "CH585 ISP support library"),
        (os.path.join(CH585_WCH_ROOT, "Startup", "startup_CH585.S"), "CH585 startup"),
        (os.path.join(CH585_WCH_ROOT, "Ld", "Link.ld"), "CH585 linker script"),
    ):
        assert_exists(path, description)
    assert_contains(h417_makefile, r"Core_V3F", "H417 V3F-only build define")
    assert_contains(h417_makefile, r"startup_ch32h417_v3f\.S", "official H417 V3F startup")
    assert_not_contains(h417_makefile, r"_dual\.hex|Core_V5F|startup_h417_v5f|Link_h417_v5f", "H417 V5F or dual-core test flow")
    assert_contains(
        os.path.join(H417_V3F_TEST_SRC_ROOT, "h417_ws2812.c"),
        r"#define\s+WS2812_LED_COUNT\s+77u",
        "WS2812 per-key LED count",
    )
    assert_contains(
        os.path.join(H417_V3F_TEST_SRC_ROOT, "h417_ws2812.c"),
        r"#define\s+WS2812_TEST_LEVEL\s+0x08u",
        "low-brightness WS2812 test level",
    )
    assert_contains(
        os.path.join(H417_V3F_TEST_SRC_ROOT, "h417_ws2812.c"),
        r"ch32h417_pioc_rgb1w_send_ram\(",
        "V3F PIOC RGB1W RAM-mode full-frame sender",
    )
    for effect in ("breath", "chase", "rainbow_band"):
        assert_contains(
            h417_makefile,
            r"h417_ws2812_{0}".format(effect),
            "separate WS2812 {0} build".format(effect),
        )
        assert_contains(
            os.path.join(H417_V3F_TEST_SRC_ROOT, "h417_ws2812.c"),
            r"ws_effect_{0}\(".format(effect),
            "WS2812 {0} effect implementation".format(effect),
        )
        assert_contains(
            os.path.join(H417_V3F_TEST_SRC_ROOT, "h417_ws2812.c"),
            r"WS2812_EFFECT_{0}".format(effect.upper()),
            "WS2812 {0} effect selector".format(effect),
        )
    assert_contains(
        os.path.join(H417_RGB1W_ROOT, "include", "ch32h417_pioc_rgb1w.h"),
        r"ch32h417_pioc_rgb1w_pin_pf13",
        "PF13 RGB1W pin descriptor",
    )
    assert_contains(
        os.path.join(H417_RGB1W_ROOT, "src", "ch32h417_pioc_rgb1w.c"),
        r"GPIOF,\s*RCC_HB2Periph_GPIOF,\s*GPIO_Pin_13,\s*GPIO_PinSource13,\s*GPIO_AF5",
        "PF13 PIOC AF5 descriptor",
    )
    assert_contains(
        os.path.join(H417_RGB1W_ROOT, "src", "ch32h417_pioc_rgb1w.c"),
        r"GPIO_PinAFConfig\(pin->port,\s*pin->pin_source,\s*pin->alternate_function\)",
        "descriptor-driven PIOC AF configuration",
    )
    assert_not_contains(
        os.path.join(H417_RGB1W_ROOT, "src", "ch32h417_pioc_rgb1w.c"),
        r"\bmemcpy\b",
        "libc memcpy dependency",
    )
    assert_contains(
        os.path.join(H417_V3F_TEST_SRC_ROOT, "system_ch32h417.c"),
        r"SystemCoreClock\s*=\s*100000000u",
        "100 MHz V3F clock for WCH PIOC RGB1W timing",
    )
    assert_contains(
        h417_makefile,
        r"h417_flash_image",
        "separate GD5F1G image write/read build",
    )
    assert_contains(
        os.path.join(H417_FLASH_NAND_ROOT, "include", "gd5f1g_spi_nand.h"),
        r"GD5F1G_PAGE_SIZE\s+2048u",
        "GD5F1G SPI-NAND geometry",
    )
    assert_contains(
        os.path.join(H417_FLASH_NAND_ROOT, "src", "ch32h417_gd5f1g_spi1.c"),
        r"GPIO_PinSource7,\s*GPIO_AF3",
        "PF7 SPI1 clock mapping",
    )
    assert_contains(
        os.path.join(H417_FLASH_NAND_ROOT, "src", "ch32h417_gd5f1g_spi1.c"),
        r"GPIO_PinSource8,\s*GPIO_AF3",
        "PF8 SPI1 data-out mapping",
    )
    assert_contains(
        os.path.join(H417_FLASH_NAND_ROOT, "src", "ch32h417_gd5f1g_spi1.c"),
        r"GPIO_PinSource9,\s*GPIO_AF3",
        "PF9 SPI1 data-in mapping",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "include", "ch32h417_ltdc_rgb.h"),
        r"CH32H417_LCD_RGB_WIDTH\s+800u",
        "H417 RGB LCD panel width",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "src", "ch32h417_lcd_rgb_control.c"),
        r"GPIOA,\s*GPIO_Pin_9",
        "LCD DISP PA9 control mapping",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "src", "ch32h417_lcd_rgb_control.c"),
        r"GPIOA,\s*GPIO_Pin_10",
        "LCD backlight CTRL PA10 control mapping",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "src", "ch32h417_ltdc_rgb.c"),
        r"LTDC_Pixelformat_RGB565",
        "RGB565 LTDC layer support",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "include", "ch32h417_ltdc_rgb.h"),
        r"ch32h417_ltdc_rgb_start_layer1",
        "shared LTDC layer1 startup API",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "include", "ch32h417_ltdc_rgb.h"),
        r"ch32h417_ltdc_rgb_layer1_clut_enable",
        "shared LTDC L8 CLUT enable API",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "include", "ch32h417_ltdc_rgb.h"),
        r"ch32h417_ltdc_rgb_pack_rgb565",
        "shared RGB565 color packing API",
    )
    assert_contains(
        os.path.join(H417_LTDC_RGB_ROOT, "include", "ch32h417_ltdc_rgb.h"),
        r"ch32h417_ltdc_rgb_fb_plot_l8_rot180",
        "shared rotated L8 framebuffer helper",
    )
    assert_contains(
        os.path.join(H417_GPHA_2D_ROOT, "include", "ch32h417_gpha_2d.h"),
        r"ch32h417_gpha_2d_fill_l8_quad",
        "shared GPHA L8 byte-fill helper",
    )
    assert_contains(
        os.path.join(H417_GPHA_2D_ROOT, "include", "ch32h417_gpha_2d.h"),
        r"does not provide native L8 output",
        "documented GPHA L8 limitation",
    )
    assert_not_exists(
        os.path.join(H417_FLASH_NAND_ROOT, "include", "gd5f1g_l8_asset_store.h"),
        "test-only L8 asset store header in shared SPI-NAND driver tree",
    )
    assert_not_exists(
        os.path.join(H417_FLASH_NAND_ROOT, "src", "gd5f1g_l8_asset_store.c"),
        "test-only L8 asset store source in shared SPI-NAND driver tree",
    )
    assert_not_exists(
        H417_SDRAM_DRIVER_ROOT,
        "firmware/h417/drivers/sdram: SDRAM bring-up must stay in hw_tests until driver cleanup",
    )
    assert_not_contains(
        H417_V5F_TEST_SRC,
        r"rt_kprintf\(\"SDRAM",
        "UART console SDRAM status output",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_status_lcd_start",
        "LCD-visible SDRAM status entry",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_status_show",
        "LCD-visible SDRAM status updates",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_status_fail_show",
        "LCD-visible SDRAM failure stage display",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_status_error_count",
        "LCD-visible SDRAM error code display",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_status_word_bits_show",
        "LCD-visible SDRAM expected/actual bit display",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_probe_data_bus_show",
        "LCD-visible SDRAM data bus probe display",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_probe_window_show",
        "LCD-visible SDRAM address-window probe display",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_dq_probe_matrix_show",
        "LCD-visible SDRAM 16-bit DQ one-hot matrix probe",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_dqm_byte_probe_show",
        "LCD-visible SDRAM DQM byte-lane readback probe",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_dq_probe_revision_marker_show",
        "LCD-visible SDRAM DQ probe revision marker",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_phase_probe_show",
        "LCD-visible SDRAM FMC read-phase scan probe",
    )
    assert_contains(
        h417_makefile,
        r"APP_V5F_HW_TEST_USB_CDC",
        "test-scoped V5F USB CDC debug build selector",
    )
    assert_contains(
        h417_makefile,
        r"h417_v5f_sdram_dq_probe[\s\S]*?APP_V5F_HW_TEST_USB_CDC\s*:=\s*1",
        "USBFS CDC enabled only for SDRAM DQ probe debug build",
    )
    assert_contains(
        h417_makefile,
        r"APP_ENABLE_USB_TEST=1\s+APP_ENABLE_USB2_FS_CDC=1\s+APP_ENABLE_USB2_HS_CDC=0\s+APP_ENABLE_USBSS_CDC=0",
        "SDRAM DQ probe USBFS-only CDC debug arguments",
    )
    assert_contains(
        H417_USB_CDC_SOURCE,
        r"ch32h417_usb_cdc_read_line",
        "USB CDC line-command RX API",
    )
    assert_contains(
        H417_USB_CDC_SOURCE,
        r"#if APP_ENABLE_USBSS_CDC",
        "USBSS CDC build gate respected by dual CDC init",
    )
    assert_contains(
        H417_USB_CDC_SOURCE,
        r"cdc_queue_rx_byte",
        "USB CDC RX byte-to-line queue",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_init",
        "SDRAM DQ probe USB CDC debug initialization",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_handle_command",
        "SDRAM DQ probe USB CDC command parser",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_regs",
        "SDRAM DQ probe USB CDC pinmux register dump",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_rcc",
        "SDRAM DQ probe USB CDC RCC/PWR register dump",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_pad",
        "SDRAM DQ probe USB CDC PD0/PD1 pad drive test",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_bias",
        "SDRAM DQ probe USB CDC PD0/PD1 pull-bias SDRAM read test",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_wlow",
        "SDRAM DQ probe USB CDC PD0/PD1 forced-low SDRAM write test",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_hslv",
        "SDRAM DQ probe USB CDC raw IO-domain HSLV diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_uport",
        "SDRAM DQ probe USB CDC UHSIF port remap diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_dq",
        "SDRAM DQ probe USB CDC one-hot DQ text map",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_usb_debug_addr",
        "SDRAM DQ probe USB CDC address-window readback diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"PWR_GetVIO18InitialStatus",
        "SDRAM DQ probe VIO18 initial-state visibility",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"AFIO->PCFR1",
        "SDRAM DQ probe AFIO remap register visibility",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"AFIO->GPIOD_AFLR",
        "SDRAM DQ probe GPIOD alternate-function register visibility",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_REMAP_ADDR",
        "test-only SDRAM 0x60000000 remap probe address",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_FMC_SDRAM_REMAP_TO_0X60000000",
        "test-only SDRAM remap diagnostic bit",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"GPIO_Remap_VIO1V8_IO_HSLV",
        "SDRAM VIO18 high-speed IO domain enable",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"GPIO_Remap_VIO3V3_IO_HSLV",
        "SDRAM VIO3V3 high-speed IO domain enable",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"GPIO_Remap_VDD3V3_IO_HSLV",
        "SDRAM VDD3V3 high-speed IO domain enable",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"GPIO_Remap_PD0PD1",
        "SDRAM PD0/PD1 XI/XO remap enable for DQ10/DQ11",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"FMC_ENHANCE_READ_MODE_Disable",
        "official FMC SDRAM enhance-read field configuration",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"FMC_BCR1_FMCEN",
        "FMC global enable for SDRAM transactions",
    )
    assert_not_contains(
        H417_V5F_TEST_SRC,
        r"FMC_Bank5_6->MISC\s*\|=",
        "raw FMC SDRAM MISC bit write",
    )
    assert_not_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_STATUS_FAIL",
        "unencoded all-red SDRAM failure state",
    )
    assert_not_contains(
        H417_V5F_TEST_SRC,
        r"sdram_select_clock_period[\s\S]*?return\s+1u\s*;[\s\S]*?sdram_refresh_count",
        "non-official SDRAM 1HCLK clock period",
    )
    assert_contains(
        os.path.join(H417_V5F_TEST_ROOT, "src", "gd5f1g_l8_asset_store.h"),
        r"gd5f1g_l8_asset_write_manifest",
        "V5F flash asset test manifest writer",
    )
    assert_not_contains(
        os.path.join(H417_GPHA_2D_ROOT, "include", "ch32h417_gpha_2d.h"),
        r"rtthread|rt_[a-z0-9_]*",
        "RT-Thread dependency in GPHA driver API",
    )
    assert_not_contains(
        os.path.join(H417_V5F_TEST_ROOT, "src", "gd5f1g_l8_asset_store.h"),
        r"#\s*include.*(rtthread|ltdc|gpha)|\brt_[a-z0-9_]*",
        "display or RT-Thread dependency in V5F flash asset test helper API",
    )
    assert_not_contains(
        H417_V5F_TEST_SRC,
        r"LTDC_CLUT(StructInit|Init|Cmd)",
        "direct LTDC CLUT register access in V5F tests",
    )
    assert_not_contains(
        H417_V5F_TEST_SRC,
        r"GPHA_Output(Blue|Green|Red)\s*=\s*(color\s*&\s*0x1Fu|\(color\s*>>\s*5\)\s*&\s*0x3Fu|\(color\s*>>\s*11\)\s*&\s*0x1Fu)",
        "RGB565 bit-field values as GPHA R2M OCOLR components",
    )
    for name in (
        "v5f_hw_test.c",
        "v5f_hw_test.h",
        "v5f_ltdc_flash_assets.S",
        "v5f_ltdc_gray_800x480.raw",
        "v5f_ltdc_palette_800x480.raw",
    ):
        if os.path.exists(os.path.join(H417_FIRMWARE_ROOT, "v5f_rtthread", "applications", name)):
            fail("V5F application tree still contains test-only file {0}".format(name))

    h417_text = (
        read_text(h417_makefile) +
        read_text(os.path.join(H417_V3F_TEST_ROOT, "Link_h417_v3f.ld")) +
        scan_tree(H417_V3F_TEST_SRC_ROOT, (".c", ".h", ".S", ".ld", ".mk", "")) +
        scan_tree(H417_V5F_TEST_ROOT, (".c", ".h", ".S", ".ld", ".mk", ""))
    )
    h417_standalone_text = (
        read_text(os.path.join(H417_V3F_TEST_ROOT, "Link_h417_v3f.ld")) +
        scan_tree(H417_V3F_TEST_SRC_ROOT, (".c", ".h", ".S", ".ld", ".mk", ""))
    )
    pioc_driver_text = scan_tree(H417_RGB1W_ROOT, (".c", ".h"))
    ltdc_rgb_driver_text = scan_tree(H417_LTDC_RGB_ROOT, (".c", ".h"))
    combined_h417_text = h417_standalone_text + pioc_driver_text + ltdc_rgb_driver_text
    ch585_text = scan_tree(CH585_ROOT, (".c", ".h", ".S", ".ld", ".mk", ""))

    forbidden_h417 = {
        r"\bUSART\b|\bUART\b|USART_|UART_": "H417 UART/USART use",
        r"\bADC\b|ADC_": "H417 ADC use",
        r"\bUSB\b|USBHS|USBFS|OTG": "H417 USB use",
        r"rtthread|RT-Thread|\brt_[a-z0-9_]*": "RT-Thread dependency",
        r"\bPB3\b|\bPB4\b|\bPB5\b|SCK0|MOSI0|MISO0": "H417-CH585 reserved SPI nets",
    }
    for pattern, description in forbidden_h417.items():
        flags = 0 if description == "RT-Thread dependency" else re.IGNORECASE
        if re.search(pattern, combined_h417_text, flags=flags):
            fail("h417 sources contain forbidden {0}".format(description))

    for pattern, description in {
        r"Core_V5F|Core_V3F|Func_Run_V3F|Run_Core": "V3F/V5F core-selection dependency",
        r"PIOC_IRQHandler|WCH-Interrupt-fast": "PIOC IRQ dependency",
    }.items():
        if re.search(pattern, pioc_driver_text):
            fail("V3F PIOC driver contains forbidden {0}".format(description))

    required_h417_tests = (
        "h417_gpio_status",
        "h417_ws2812",
        "h417_lcd_signal",
        "h417_lcd_backlight",
        "h417_ltdc",
        "h417_flash_image",
        "h417_v5f_ltdc",
        "h417_v5f_ltdc_l8_palette_image",
        "h417_v5f_ltdc_rgb565_diag",
        "h417_v5f_gpha_r2m_fill",
        "h417_v5f_gpha_pfc_l8_rgb565",
        "h417_v5f_gpha_blend_rgb565",
        "h417_v5f_gpha_l8_ltdc_fullscreen",
        "h417_v5f_flash",
        "h417_v5f_flash_l8_assets",
        "h417_v5f_sdram_memtest",
        "h417_v5f_sdram_ltdc_rgb565",
        "h417_v5f_sdram_remap_probe",
        "h417_v5f_sdram_dq_probe",
        "h417_v5f_ch585_spi_speed",
    )
    for name in required_h417_tests:
        if name not in h417_text:
            fail("h417 sources missing {0}".format(name))

    for stale_name in (
        "h417_v5f_gpha ",
        "h417_v5f_gpha_l8_clut_bars_diag",
        "APP_V5F_HW_TEST_GPHA_L8_CLUT_BARS_DIAG",
    ):
        if stale_name in h417_text:
            fail("h417 sources still contain stale GPHA test name {0}".format(stale_name.strip()))

    required_ch585_tests = (
        "ch585_left_eeprom_i2c",
        "ch585_left_controls_gpio",
        "ch585_right_max17048_i2c",
        "ch585_right_charge_gpio",
        "ch585_right_ec11_gpio",
        "ch585_adc_mux_scan",
        "ch585_spi0_speed_slave",
    )
    for name in required_ch585_tests:
        if name not in ch585_text:
            fail("ch585 sources missing {0}".format(name))

    for token in ("PA8", "PA9", "TX1", "RX1"):
        if token not in ch585_text:
            fail("ch585 sources missing serial token {0}".format(token))

    assert_ch585_firmware_has_no_test_residue()

    print("PASS: hardware test projects stay inside the reserved-interface boundary")


if __name__ == "__main__":
    main()
