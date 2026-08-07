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
H417_V3F_DRIVER_ROOT = os.path.join(H417_FIRMWARE_ROOT, "v3f", "drivers")
H417_V5F_DRIVER_ROOT = os.path.join(H417_FIRMWARE_ROOT, "v5f_rtthread", "drivers")
H417_RGB1W_ROOT = os.path.join(H417_V3F_DRIVER_ROOT, "rgb1w_pioc")
H417_FLASH_NAND_ROOT = os.path.join(H417_V3F_DRIVER_ROOT, "gd5f1g_spi_nand")
H417_LTDC_RGB_ROOT = os.path.join(H417_V5F_DRIVER_ROOT, "ltdc_rgb")
H417_GPHA_2D_ROOT = os.path.join(H417_V5F_DRIVER_ROOT, "gpha_2d")
H417_SDRAM_DRIVER_ROOT = os.path.join(H417_V5F_DRIVER_ROOT, "sdram")
H417_V3F_TEST_ROOT = os.path.join(H417_ROOT, "cases", "v3f_standalone")
H417_V3F_TEST_SRC_ROOT = os.path.join(H417_V3F_TEST_ROOT, "src")
H417_V5F_TEST_ROOT = os.path.join(H417_ROOT, "cases", "v5f_rtthread")
H417_V5F_TEST_SRC = os.path.join(H417_V5F_TEST_ROOT, "src", "v5f_hw_test.c")
H417_USB_CDC_SOURCE = os.path.join(H417_FIRMWARE_ROOT, "v5f_rtthread", "applications", "usb_cdc_dual.c")
H417_SDRAM_RESULT_READER = os.path.join(H417_ROOT, "tools", "read_sdram_result.ps1")


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


def scan_tree(path, suffixes, include_paths=True):
    if not os.path.exists(path):
        fail("missing {0}".format(os.path.relpath(path, ROOT)))
    data = []
    for base, _dirs, files in os.walk(path):
        for name in sorted(files):
            child = os.path.join(base, name)
            _root, ext = os.path.splitext(child)
            if ext.lower() in suffixes:
                if include_paths:
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
    assert_contains(h417_makefile, r"V3F_DRIVER_ROOT\s*:=\s*\$\(H417_FIRMWARE_ROOT\)/v3f/drivers", "V3F driver root")
    assert_contains(h417_makefile, r"V5F_DRIVER_ROOT\s*:=\s*\$\(H417_FIRMWARE_ROOT\)/v5f_rtthread/drivers", "V5F driver root")
    assert_contains(h417_makefile, r"RGB1W_PIOC_ROOT\s*:=\s*\$\(V3F_DRIVER_ROOT\)/rgb1w_pioc", "V3F RGB1W PIOC driver tree")
    assert_contains(h417_makefile, r"FLASH_NAND_ROOT\s*:=\s*\$\(V3F_DRIVER_ROOT\)/gd5f1g_spi_nand", "V3F GD5F1G driver tree")
    assert_contains(h417_makefile, r"LTDC_RGB_ROOT\s*:=\s*\$\(V5F_DRIVER_ROOT\)/ltdc_rgb", "V5F LTDC RGB driver tree")
    assert_contains(h417_makefile, r"V3F_STANDALONE_ROOT\s*:=\s*cases/v3f_standalone", "H417 V3F standalone test root")
    assert_contains(h417_makefile, r"H417_DUAL_CORE_TESTS\s*:=", "H417 dual-core test wrapper list")
    assert_contains(h417_makefile, r"H417_HW_TEST_BUILD_NAME\s*:=\s*\$\(HW_TEST\)", "H417 default build name")
    assert_contains(h417_makefile, r"DIRECT_V5F_BUILD_ROOT\s*:=\s*\.\./\.\./\.\./hw_tests/h417/\$\(BUILD_ROOT\)/\$\(H417_HW_TEST_BUILD_NAME\)", "direct V5F test build root")
    assert_contains(h417_makefile, r"HW_TEST=\$\(H417_V3F_WAKE_STUB_TEST\)", "V3F test wake-stub build")
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
        "firmware/h417/v5f_rtthread/drivers/sdram: SDRAM bring-up must stay in hw_tests until driver cleanup",
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
        r"h417_v5f_sdram_memtest[\s\S]*?APP_V5F_HW_TEST_USB_CDC\s*:=\s*1",
        "main USBFS CDC enabled for the SDRAM full-memory test",
    )
    assert_contains(
        h417_makefile,
        r"h417_v5f_sdram_dq_probe[\s\S]*?APP_V5F_HW_TEST_USB_CDC\s*:=\s*1",
        "USBFS CDC enabled for the SDRAM DQ probe debug build",
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
        r"V5F_SDRAM_MEMTEST_CDC_ONLY",
        "CDC-only SDRAM full-memory test gate",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"H417 SDRAM CDC TEST v61 DMA2 ISOLATION",
        "SDRAM DMA-controller isolation stress banner",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"WATCHDOG RECOVERY cause=IWDG",
        "SDRAM hard-lock retained recovery report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"WATCHDOG ARMED timeout_approx_s=25 retain=2017ff00 checkpoint=pre-enable",
        "SDRAM hard-lock independent watchdog arm marker",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"memcpy\(framed, line, length\)",
        "SDRAM CDC line and CRLF coalescing",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_DMA_RESTART_GAP_US\s+100u",
        "SDRAM DMA restart settling interval",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"RANGE C0000000-C1FFFFFF bytes=33554432 access=short-dma1,stress-dma2-word32",
        "SDRAM full native-x16 address range",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BANK_SPAN bytes=00800000 bases=C0000000/C0800000/C1000000/C1800000",
        "SDRAM native-x16 four-bank base mapping",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"X16_DQ START patterns=0000,ffff,aaaa,5555,walking1,walking0 banks=4 samples=64",
        "full x16 fixed and walking-pattern per-DQ diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"X16_BIT D%u FAIL bad=%u/%u first=%08x exp=%04x got=%04x",
        "per-DQ error count and first-failure report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BW %s bytes=%u cycles=%u raw=%u.%02uMB/s useful=%u.%02uMB/s",
        "measured raw and good-bit-adjusted SDRAM bandwidth report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_dma_timed",
        "timed 32-bit and 256-bit DMA SDRAM read/write paths",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"DMA_Init\(DMA2_Channel3, &dma\)",
        "sustained SDRAM stress isolated onto DMA2 channel 3",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"DMA_ROUTE short=DMA1_CH3 stress=DMA2_CH3 gap_us=100",
        "short and sustained DMA controller routing report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"GEOMETRY fmc_width=16 device_width=16 row=13 col=9 banks=4",
        "native-x16 SDRAM with native-halfword CPU accesses",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"init\.FMC_MemoryDataWidth\s*=\s*FMC_MemoryDataWidth_16",
        "FMC SDRAM controller native x16 data width",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_x16_low8_read",
        "native-halfword low-byte masked read helper",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_x8_full",
        "finite x8 full-memory test entry",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_x8_bank_probe",
        "x8 BA0/BA1 bank-alias diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"FULL_TEST START mode=prefetch access=cpu16 compare=low8 logical=%u physical=%u banks=%s",
        "prefetched masked-halfword all-bank full test",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BANK_READ_SWITCH grouped_bad=%u/256 alt_bad=%u/256",
        "grouped-versus-alternating bank read diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BANK_PAIR_PALL bank=%u cmd=%u",
        "explicit precharge-all bank switch diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BANK_MODE START modes=normal,enhance,rburst phases=16 pipes=3",
        "low-byte cross-bank FMC read mode, phase, and pipe scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BANK_MODE FORCE mode=normal enh=0 rb=0",
        "fixed vendor-reference FMC read mode after diagnostic scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BANK_SETTLE START access=cpu16 compare=low8 discard=0,1,2,4,8,16,32",
        "native-halfword low-byte bank-switch settling scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_bank_settle_scan\(\)",
        "bank-switch settling diagnostic invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"BANK_SYNC START access=cpu16 compare=low8 strategies=direct,d1,fence,us1,us10,pall,d32,us10_r8191",
        "bank-switch synchronization strategy scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_bank_sync_scan\(\)",
        "bank-switch synchronization diagnostic invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"MAP16 START access=cpu16 compare=low8 native=c0000000 remap=60000000",
        "native and remapped SDRAM window comparison scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_map16_report\(\"remap_read_native\"",
        "native-write remapped-read comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_map16_report\(\"native_read_remap\"",
        "remapped-write native-read comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_map16_scan\(\)",
        "native/remapped x16 comparison invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"ACCESS_PROFILE START write=cpu32 repeated-byte compare=low8",
        "CPU 8/16/32-bit access matrix using known 32-bit writes",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_access_profile_report\(\"wch_official\"",
        "WCH prefetch plus remapped-window access profile",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_access_profile_scan\(\)",
        "normal-versus-WCH access profile invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"DMA_PATH START source=sdram destination=sram channel=DMA1_CH3 modes=word,256",
        "official-style SDRAM-to-SRAM DMA path comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"DMA_SAMPLE word_ok=%u b0=%08x/%08x wide_ok=%u b1=%08x/%08x mask=00ff00ff",
        "raw word and 256-bit DMA sample report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"DMA_BOUNDARY word=%u/3 bad=%u wide256=%u/3 bad=%u span=64 compare=00ff00ff",
        "single DMA transfer spanning each physical Bank boundary",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"DMA_REC word direct=%u d1=%u d2=%u pall=%u pall_ar1=%u us10=%u bad=%u",
        "32-bit DMA Bank-switch recovery strategies",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"DMA_REC wide256 direct=%u d1=%u d2=%u pall=%u pall_ar1=%u us10=%u bad=%u",
        "256-bit DMA Bank-switch recovery strategies",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_dma_path_scan\(\)",
        "DMA bank and boundary diagnostic invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"CAS_SCAN START modes=cl3,cl2 phases=16 pipes=3 score=cpu16/256\+dma",
        "matched SDRAM/FMC CL3 and CL2 phase/pipe scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"CAS_SCAN END restored=cl3 phase=10 pipe=0 mode=0230",
        "CAS scan restores the production CL3 configuration",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_cas_scan\(\)",
        "CAS comparison invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"COLD_SCAN START order=normal_n0,prefetch_n0,normal_n15 phases=16 pipes=3",
        "cold-init normal, prefetch, and NRFS profile comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"COLD_PROFILE name=%s prefetch=%u nrfs=%u init=%d scan=%u verify=%u d32=%u dma256=%u",
        "cold-init CPU and DMA profile report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"NRFS_SCAN START mode=normal values=0\.\.15 refreshes_per_event=value\+1",
        "NRFS refresh-burst count scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_nrfs_scan\(\)",
        "cold-profile and NRFS scan invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"PREFETCH_CPU direct=%u/4096 bad=%u",
        "long alternating cross-bank CPU validation in prefetch mode",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"PREFETCH_BOUNDARY bank=%u logical=%08x score=%u/256",
        "sequential CPU reads across all physical Bank boundaries",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"PREFETCH_DMA_RAW bank=%u ok=%u exp=%02x raw=%08x/%08x",
        "raw prefetched 256-bit DMA samples",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_prefetch_validate\(\)",
        "cold-prefetch validation invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_init_profile\(0u, 0u, 1u\)",
        "normal mode used in the first reported FMC initialization",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"COHERENCE_SINGLE first=%02x after_write=%02x normal=%02x reenable=%02x",
        "single-address write and prefetch invalidation probe",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"COHERENCE_SEQ prefetch_write=%u/2048 normal_write_then_prefetch=%u/2048",
        "official-style sequential prefetch comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_prefetch_coherence\(\)",
        "prefetch coherency diagnostic invocation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_CLOCK_PERIOD_1HCLK\s+1u",
        "WCH official SDCLK-equals-HCLK discriminator",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_OFFICIAL_REFRESH\s+677u",
        "WCH official refresh-count discriminator",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"NORMAL100_BANK score=%u/4096 bad=%u",
        "100 MHz normal-mode cross-bank CPU validation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"NORMAL100_DMA word=%u/256 wide=%u/256 bad=%u/%u",
        "100 MHz normal-mode DMA validation",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"FULL_TEST START mode=normal100 access=cpu16 compare=low8",
        "100 MHz normal-mode complete low-byte test",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"RECOVERY START refresh=8191 strategies=direct,us20,d32,pall1,pall2,pall_ar1,toggle",
        "slow-refresh recovery strategy comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"REFRESH_PROFILE START counts=41,80,110,160,240,480,8191",
        "standard and A2 refresh-count cross-bank comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"CPU STORAGE RESULT PASS mode=prefetch logical_bytes=16777216 compare=low8",
        "complete prefetched CPU storage-array pass report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"FUNCTION RESULT FAIL reason=prefetch_dma_direct cpu_storage=pass",
        "separate CPU storage health from the direct DMA path",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"TRANS START access=cpu16 compare=low8 pair=35/ca direct,fence,pall max=128",
        "column, row, and bank transition scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_transition_report\(\"ba0_nb2\"",
        "two-bank versus four-bank BA0 comparison",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_ENHANCE_READ_BIT\s+\(1u << 15\)",
        "documented R32_SDRAM_MISC enhanced-read bit",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"PASS passes=6 rw_turnaround=separated",
        "six-pass low-byte March result with separated directions",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"RETENTION PASS delay_ms=1000 checksum=%08x",
        "x8 delayed full-memory retention verification",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"MAP base=%08x remap=%u nor_en=%u bcr0=%08x misc=%08x",
        "SDRAM continuous-test FMC map report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_disable_0x60000000_remap\(\)",
        "SDRAM continuous-test native-window selection",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"IO pwrctl=%08x pcfr1=%08x pd0rm=%u hslv=%u",
        "SDRAM continuous-test IO configuration report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_cdc_diagnose",
        "SDRAM automatic CDC failure diagnostics",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_memtest_cdc_wait_for_start",
        "SDRAM continuous CDC host handshake",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"LOW8 CONFIG fmc_width=16 cpu_width=16 stride=2 compare_mask=00ff",
        "native-x16 controller and low-byte comparison report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"SCOPE WRITE START writes_only=1",
        "SDRAM isolated FMC write-path scope phase",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"SCOPE READ START reads_only=1.*no_writes=1",
        "SDRAM isolated device read-path scope phase",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"READ_SCAN START phases=16 pipes=3",
        "SDRAM read-only phase and pipe scan",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"MODE_SCAN START afr=1 reads_only=1 modes=afpp,afod,float,ipu,ipd",
        "SDRAM PD0/PD1 input-mode matrix",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"MODE_SCAN %s cfg=%02x raw=%u/192 fuse=%u/256 map=",
        "SDRAM raw FMC versus GPIO-fused mode report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"ROUTE_SCAN START reads_only=1 rm=0/1 hslv=0/1 af=1 mode=afpp",
        "SDRAM PD0/PD1 remap and HSLV matrix",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"ROUTE_SCAN rm=%u hslv=%u pcfr1=%08x raw=%u/192 fuse=%u/256 map=",
        "SDRAM remap matrix raw and GPIO-fused report",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"SCOPE PIN report=%u map=",
        "SDRAM per-address PD0/PD1 read mapping",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_MAX_SDCLK_HZ\s+100000000u",
        "SDRAM validated 1-HCLK 100 MHz clock limit",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"refresh_count << 1",
        "SDRAM refresh count field encoding",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"RESULT PASS",
        "SDRAM full-memory CDC PASS result",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"RESULT FAIL",
        "SDRAM full-memory CDC FAIL result",
    )
    assert_exists(
        H417_SDRAM_RESULT_READER,
        "PowerShell SDRAM CDC result reader",
    )
    assert_contains(
        H417_SDRAM_RESULT_READER,
        r"Write\(\"start`r`n\"\)",
        "SDRAM reader start handshake",
    )
    assert_contains(
        H417_SDRAM_RESULT_READER,
        r"CONTINUOUS START",
        "SDRAM reader continuous streaming mode",
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
        r"sdram_usb_debug_scope",
        "SDRAM DQ probe USB CDC repeated read scope diagnostic",
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
        r"AFIO_PCFR1_VIO18_IO_HSLV",
        "SDRAM VIO18 low-voltage high-speed control",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"AFIO_PCFR1_VIO33_IO_HSLV",
        "SDRAM VIO3V3 low-voltage high-speed control",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"AFIO_PCFR1_VDD33_IO_HSLV",
        "SDRAM VDD3V3 low-voltage high-speed control",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"AFIO->PCFR1\s*&=\s*~AFIO_PCFR1_PD0_1_REMAP",
        "SDRAM QEU6 dedicated PD0/PD1 routing for DQ10/DQ11",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"GPIO_PinAFConfig\(GPIOA,\s*GPIO_PinSource9,\s*GPIO_AF15\)",
        "PA9 GPIO-control isolation from its SDRAM DQ10 AF0 route",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"GPIO_PinAFConfig\(GPIOA,\s*GPIO_PinSource10,\s*GPIO_AF15\)",
        "PA10 GPIO-control isolation from its SDRAM DQ11 AF0 route",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_gpio_af\(GPIOD,\s*GPIO_Pin_0,\s*GPIO_PinSource0,\s*GPIO_AF1\)",
        "SDRAM DQ10 on dedicated PD0 AF1",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"sdram_gpio_af\(GPIOD,\s*GPIO_Pin_1,\s*GPIO_PinSource1,\s*GPIO_AF1\)",
        "SDRAM DQ11 on dedicated PD1 AF1",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"H417 SDRAM CDC TEST v71 FULL16",
        "full-x16 SDRAM acceptance-test banner",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"x16_compare_mask\s*=\s*0xFFFFu",
        "all sixteen SDRAM data bits in the acceptance comparison mask",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"FULL16 DMA RESULT PASS mask=ffff dma_rw=pass",
        "full-x16 SDRAM DMA acceptance result",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"H417 SDRAM LTDC TEST v29 ARGB8888 IPC STATIC INTERNAL",
        "ARGB8888 LTDC pixel-clock polarity diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"PCLK CONTRACT source=internal_shared_sram no_sdram=1 no_upload=1 no_dma=1 no_frame_switch=1",
        "internal-SRAM static-frame isolation contract",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"panel\.pixel_clock_polarity\s*=\s*LTDC_PCPolarity_IPC",
        "test-local IPC pixel-clock polarity override",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"PCLK RESULT PASS format=argb8888 internal_crc=pass scan=pass ipc=1 fifo_underrun=0",
        "static ARGB8888 IPC LTDC diagnostic result",
    )
    assert_contains(
        H417_SDRAM_RESULT_READER,
        r'H417 SDRAM LTDC TEST',
        "PowerShell result reader recognizes the static LTDC diagnostic",
    )
    assert_contains(
        H417_V5F_TEST_SRC,
        r"V5F_SDRAM_NORMAL_READ_MODE",
        "manual-defined FMC SDRAM normal-read field configuration",
    )
    assert_not_contains(
        H417_V5F_TEST_SRC,
        r"BTCR\[0\]\s*\|=\s*FMC_BCR1_FMCEN",
        "NAND/NOR/PSRAM FMC enable in the SDRAM initialization path",
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
        r"static\s+uint32_t\s+sdram_select_clock_period[\s\S]*?return\s+1u\s*;[\s\S]*?static\s+uint16_t\s+sdram_refresh_count",
        "non-official SDRAM 1HCLK clock period",
    )
    assert_contains(
        os.path.join(H417_V5F_TEST_ROOT, "src", "gd5f1g_l8_asset_store.h"),
        r"gd5f1g_l8_asset_write_manifest",
        "V5F flash asset test manifest writer",
    )
    assert_not_contains(
        os.path.join(H417_GPHA_2D_ROOT, "include", "ch32h417_gpha_2d.h"),
        r"#\s*include.*rtthread|\brt_[a-z0-9_]*",
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
    pioc_driver_text = scan_tree(H417_RGB1W_ROOT, (".c", ".h"), include_paths=False)
    ltdc_rgb_driver_text = scan_tree(H417_LTDC_RGB_ROOT, (".c", ".h"), include_paths=False)
    combined_h417_text = h417_standalone_text + pioc_driver_text + ltdc_rgb_driver_text
    usbfs_cdc_diag_path = os.path.join(
        H417_V3F_TEST_SRC_ROOT, "h417_usbfs_cdc_diag.c"
    )
    usbfs_cdc_diag_text = read_text(usbfs_cdc_diag_path)
    h417_physical_uart_text = combined_h417_text.replace(usbfs_cdc_diag_text, "")
    assert_not_contains(
        usbfs_cdc_diag_path,
        r"\bDEF_UART\b|UART_Init\s*\(|UART_CfgInit\s*\(|UART_DMAInit\s*\(|USART_",
        "physical UART bridge in USBFS-only diagnostic adapter",
    )
    ch585_text = scan_tree(CH585_ROOT, (".c", ".h", ".S", ".ld", ".mk", ""))

    forbidden_h417 = {
        r"\bUSART\b|\bUART\b|USART_|UART_": "H417 UART/USART use",
        r"\bADC\b|ADC_": "H417 ADC use",
        r"rtthread|RT-Thread|\brt_[a-z0-9_]*": "RT-Thread dependency",
        r"\bPB3\b|\bPB4\b|\bPB5\b|SCK0|MOSI0|MISO0": "H417-CH585 reserved SPI nets",
    }
    for pattern, description in forbidden_h417.items():
        flags = 0 if description == "RT-Thread dependency" else re.IGNORECASE
        source_text = (
            h417_physical_uart_text
            if description == "H417 UART/USART use"
            else combined_h417_text
        )
        if re.search(pattern, source_text, flags=flags):
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
        "h417_v5f_sdram_official_16bit",
        "h417_v5f_ch585_spi_speed",
        "h417_v3f_usbss_ch372",
        "h417_v3f_usbss_fs_diag",
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
        "ch585_ads7948_32k_pipeline",
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
