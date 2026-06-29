import io
import os
import re
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def path(*parts):
    return os.path.join(ROOT, *parts)


def read_text(*parts):
    with io.open(path(*parts), "r", encoding="utf-8", errors="ignore") as handle:
        return handle.read()


def assert_re(test_case, text, pattern):
    test_case.assertTrue(
        re.search(pattern, text, re.MULTILINE) is not None,
        "pattern not found: {0}".format(pattern),
    )


class FirmwareMainMvpContract(unittest.TestCase):
    def test_shared_spi_protocol_is_fixed_12_byte_wired_half_state_contract(self):
        header_path = path("firmware", "common", "aik_spi_protocol.h")
        self.assertTrue(os.path.exists(header_path))

        header = read_text("firmware", "common", "aik_spi_protocol.h")
        for token in (
            "AIK_SPI_HOST_CMD_SIZE 32U",
            "AIK_SPI_HALF_STATE_SIZE 12U",
            "AIK_SPI_HOST_MAGIC 0xA6U",
            "AIK_SPI_HALF_MAGIC 0x5AU",
            "AIK_SPI_HALF_TYPE_STATE 0x11U",
            "AIK_SPI_CMD_POLL 0U",
            "AIK_SPI_CMD_POLL_WITH_RF 1U",
            "AIK_KEY_COUNT_LEFT 36U",
            "AIK_KEY_COUNT_RIGHT 41U",
            "AIK_KEY_COUNT_TOTAL 77U",
            "aik_spi_crc16_ccitt",
            "aik_spi_host_cmd_v1_t",
            "aik_spi_half_state_v1_t",
        ):
            self.assertIn(token, header)

        half_struct = re.search(
            r"typedef struct AIK_SPI_PACKED\s*\{(?P<body>[^}]*)\}\s*aik_spi_half_state_v1_t;",
            header,
            re.DOTALL,
        ).group("body")
        self.assertIn("uint8_t down_bits[AIK_HALF_DOWN_BITS_BYTES];", half_struct)
        self.assertNotIn("uint8_t version;", half_struct)
        self.assertNotIn("uint8_t half_id;", half_struct)
        self.assertNotIn("uint8_t key_count;", half_struct)
        self.assertNotIn("uint8_t status;", half_struct)
        self.assertNotIn("uint8_t local[AIK_LOCAL_STATE_BYTES];", half_struct)
        self.assertNotIn("uint32_t scan_frame32;", half_struct)
        assert_re(self, header, r"sizeof\(aik_spi_host_cmd_v1_t\)\s*==\s*AIK_SPI_HOST_CMD_SIZE")
        assert_re(self, header, r"sizeof\(aik_spi_half_state_v1_t\)\s*==\s*AIK_SPI_HALF_STATE_SIZE")

    def test_ch585_default_build_is_half_scan_with_left_and_right_targets(self):
        makefile = read_text("firmware", "ch585", "Makefile")
        assert_re(self, makefile, r"^APP\s*\?=\s*half_scan\b")
        self.assertIn("half_scan_left", makefile)
        self.assertIn("half_scan_right", makefile)
        self.assertIn("CH585_HALF_ID=0", makefile)
        self.assertIn("CH585_HALF_ID=1", makefile)
        assert_re(self, makefile, r"half_scan_left:\n\t@.*CH585_RF_TX_ENABLE=0")
        assert_re(self, makefile, r"half_scan_right:\n\t@.*CH585_RF_TX_ENABLE=0")
        self.assertIn("rf_keyboard_tx is disabled", makefile)
        self.assertNotIn("APP=rf_keyboard_tx", makefile)
        self.assertIn("ch585_rf_nkro_tx.c", makefile)

        main_c = read_text("firmware", "ch585", "applications", "half_scan", "main.c")
        engine_c = read_text("firmware", "ch585", "applications", "magnetic_key_engine.c")
        self.assertIn("ch585_ads7948_mux_acq_poll", main_c)
        self.assertIn("mag_key_engine_update", main_c)
        self.assertIn("aik_spi_half_state_v1_t", main_c)
        self.assertIn("ch585_spi0_slave_link_serve_frame", main_c)
        self.assertIn("CH585_RF_TX_ENABLE", main_c)
        self.assertIn("CH585_SPI_ACCEPT_HOST_CMD", main_c)
        self.assertIn("#if CH585_RF_TX_ENABLE", main_c)
        self.assertIn("CH58x_BLEInit", main_c)
        self.assertIn("ch585_rf_nkro_tx_init", main_c)
        self.assertIn("ch585_rf_nkro_tx_set_report", main_c)
        self.assertIn("cfg.mode = MAG_KEY_MODE_RAPID_TRIGGER", main_c)
        self.assertIn("MAG_KEY_DEFAULT_RT_PRESS_DELTA_PM  300U", engine_c)
        self.assertIn("MAG_KEY_DEFAULT_RT_RELEASE_DELTA_PM 300U", engine_c)
        self.assertIn("MAG_KEY_DEFAULT_FILTER_SHIFT       0U", engine_c)

    def test_h417_v3f_default_build_owns_product_main_without_v5f_default(self):
        h417_makefile = read_text("firmware", "h417", "Makefile")
        assert_re(self, h417_makefile, r"^all:\s*v3f\s*$")
        self.assertNotIn("v3f_usbfs_enum_probe", h417_makefile)
        self.assertNotIn("V3F_USBFS_ENUM_ONLY", h417_makefile)

        v3f_makefile = read_text("firmware", "h417", "v3f", "Makefile")
        for source in (
            "board_init.c",
            "ch585_link.c",
            "half_state.c",
            "default_profile.c",
            "rf_report_bridge.c",
            "rgb_status.c",
            "ch32h417_usbhs_hid_nkro.c",
        ):
            self.assertIn(source, v3f_makefile)
        self.assertIn("V3F_ENABLE_USBHS_8K ?= 1", v3f_makefile)
        self.assertIn("V3F_USB_REPORT_INTERVAL_US ?= 125", v3f_makefile)
        self.assertIn("V3F_ENABLE_RGB_STATUS ?= 0", v3f_makefile)
        self.assertIn("V3F_ENABLE_RF_BRIDGE ?= 0", v3f_makefile)
        self.assertIn("V3F_ENABLE_SPI_HOST_CMD ?= 0", v3f_makefile)
        self.assertNotIn("V3F_USBFS_ENUM_ONLY", v3f_makefile)

        main_c = read_text("firmware", "h417", "v3f", "applications", "main.c")
        self.assertIn("V3F_ENABLE_USBHS_8K", main_c)
        self.assertIn("ch32h417_usbhs_hid_nkro.h", main_c)
        self.assertIn("v3f_ch585_link_poll", main_c)
        self.assertIn("v3f_half_state_merge", main_c)
        self.assertIn("v3f_default_profile_build_nkro16", main_c)
        self.assertIn("v3f_usb_hid_nkro_submit", main_c)
        self.assertIn("v3f_usb_hid_nkro_pending_empty", main_c)
        self.assertIn("v3f_usb_hid_nkro_reports", main_c)
        self.assertIn("v3f_rf_report_bridge_prepare_cmd", main_c)
        self.assertIn("V3F_ENABLE_SPI_HOST_CMD", main_c)
        self.assertIn("v3f_prepare_spi_poll_tx", main_c)
        self.assertIn("V3F_LINK_STALE_US 5000U", main_c)
        self.assertIn("V3F_LINK_STALE_TICKS", main_c)
        self.assertIn("age_half_cache_on_usb_report", main_c)
        self.assertIn("if(v3f_usb_hid_nkro_pending_empty() == 0U)", main_c)
        self.assertNotIn("V3F_USBFS_ENUM_ONLY", main_c)
        self.assertIn("v3f_usb_diag_trace", main_c)
        self.assertNotIn("v3f_board_delay_us(V3F_USB_REPORT_INTERVAL_US)", main_c)
        self.assertNotIn("v3f_board_delay_1ms();", main_c)
        self.assertNotIn("v3f_usb_hid_nkro_send(nkro16)", main_c)
        self.assertLess(
            main_c.index("\n    v3f_usb_hid_nkro_init();"),
            main_c.index("\n    v3f_ch585_link_init();"),
        )

    def test_h417_default_profile_uses_latex_default_keymap(self):
        source = read_text("firmware", "h417", "v3f", "applications", "default_profile.c")

        self.assertIn("s_default_key_outputs[AIK_KEY_COUNT_TOTAL]", source)
        for marker in (
            "/* key_000 */ { HID_USAGE_F12, 0U }",
            "/* key_007 */ { HID_USAGE_BACKSPACE, 0U }",
            "/* key_008 */ { HID_USAGE_EQUAL, 0U }",
            "/* key_036 */ { 0U, HID_MOD_RIGHT_CTRL }",
            "/* key_037 */ { 0U, HID_MOD_RIGHT_GUI }",
            "/* key_038 */ { 0U, 0U }",
            "/* key_039 */ { 0U, HID_MOD_RIGHT_ALT }",
            "/* key_040 */ { HID_USAGE_SPACE, 0U }",
            "/* key_041 */ { HID_USAGE_F5, 0U }",
            "/* key_052 */ { HID_USAGE_1, 0U }",
            "/* key_065 */ { HID_USAGE_A, 0U }",
            "/* key_072 */ { 0U, HID_MOD_LEFT_SHIFT }",
            "/* key_076 */ { 0U, HID_MOD_LEFT_CTRL }",
        ):
            self.assertIn(marker, source)

        self.assertIn("nkro16[0] |= output->modifier_mask", source)
        self.assertIn("usage - HID_USAGE_A", source)
        self.assertNotIn("bit_index = key_id", source)

    def test_h417_v3f_usbfs_hid_uses_official_core_with_nkro_wrapper(self):
        wrapper = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbfs_hid_nkro",
            "src",
            "ch32h417_usbfs_hid_nkro.c",
        )
        official_core = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbfs_compat_hid",
            "src",
            "ch32h417_usbfs_device.c",
        )
        descriptor = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbfs_compat_hid",
            "src",
            "usb_desc.c",
        )
        header = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbfs_hid_nkro",
            "include",
            "ch32h417_usbfs_hid_nkro.h",
        )
        v3f_makefile = read_text("firmware", "h417", "v3f", "Makefile")
        board_init = read_text("firmware", "h417", "v3f", "applications", "board_init.c")

        self.assertIn("drivers/wch_usbfs_compat_hid/include", v3f_makefile)
        self.assertIn("drivers/wch_usbfs_compat_hid/src/ch32h417_usbfs_device.c", v3f_makefile)
        self.assertIn("drivers/wch_usbfs_compat_hid/src/usb_desc.c", v3f_makefile)

        for token in (
            "USBFS_IRQHandler",
            "RCC_USBHSPLLCLKConfig",
            "RCC_USBHSPLLReferConfig",
            "RCC_USBHSPLLClockSourceDivConfig",
            "RCC_USBHS_PLLCmd",
            "RCC_USBHS_PLLRDY",
            "RCC_USBFSCLKConfig(RCC_USBFSCLKSource_USBHSPLL)",
            "RCC_USBFS48ClockSourceDivConfig(RCC_USBFS_Div10)",
            "RCC_HBPeriphClockCmd(RCC_HBPeriph_OTG_FS, ENABLE)",
            "USBFSD->UEP0_DMA",
            "USBFSD->UDEV_CTRL",
            "USBFSH->BASE_CTRL",
            "USB_GET_DESCRIPTOR",
            "USB_SET_ADDRESS",
            "USB_SET_CONFIGURATION",
            "USB_DESCR_TYP_REPORT",
            "HID_SET_IDLE",
        ):
            self.assertIn(token, official_core)
        assert_re(self, official_core, r"NVIC_EnableIRQ\(\s*USBFS_IRQn\s*\)")

        for token in (
            "HID_Report_Buffer[64]",
            "USBFS_Device_Init(ENABLE)",
            "USBFS_DevEnumStatus",
            "USBFS_EP2_Buf",
            "USBFSD_UEP_TLEN(DEF_UEP2) = AIK_NKRO_REPORT_BYTES",
            "AIK_NKRO_REPORT_BYTES",
        ):
            self.assertIn(token, wrapper)
        self.assertNotIn("USBFS_IRQHandler", wrapper)

        for token in (
            "MyDevDescr",
            "MyCfgDescr",
            "MyHIDReportDesc",
            "0x05, 0x01",
            "0x09, 0x06",
            "0x19, 0x04",
            "0x29, 0x73",
            "0x95, 0x70",
            "0x82",
            "0x10, 0x00",
            "0x01,                           // bInterval: 1mS",
        ):
            self.assertIn(token, descriptor)
        self.assertIn("ch32h417_usbfs_hid_nkro_diag_t", header)
        self.assertIn("ch32h417_usbfs_hid_nkro_diag_snapshot", header)
        self.assertNotIn("v3f_usbfs_clock_init", board_init)
        assert_re(self, official_core, r"USBFSD->UEP0_TX_CTRL\s*=\s*USBFS_UEP_T_TOG\s*\|\s*USBFS_UEP_T_RES_NAK")
        assert_re(self, official_core, r"USBFSD->UEP0_RX_CTRL\s*=\s*USBFS_UEP_R_TOG\s*\|\s*USBFS_UEP_R_RES_NAK")
        assert_re(self, official_core, r"USBFSD->UEP0_TX_CTRL\s*=\s*USBFS_UEP_T_TOG\s*\|\s*USBFS_UEP_T_RES_ACK")

    def test_h417_v3f_default_usbhs_8k_uses_official_core_with_nkro_wrapper(self):
        wrapper_path = path(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbhs_hid_nkro",
            "src",
            "ch32h417_usbhs_hid_nkro.c",
        )
        header_path = path(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbhs_hid_nkro",
            "include",
            "ch32h417_usbhs_hid_nkro.h",
        )
        self.assertTrue(os.path.exists(wrapper_path))
        self.assertTrue(os.path.exists(header_path))

        wrapper = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbhs_hid_nkro",
            "src",
            "ch32h417_usbhs_hid_nkro.c",
        )
        header = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbhs_hid_nkro",
            "include",
            "ch32h417_usbhs_hid_nkro.h",
        )
        descriptor = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbhs_compat_hid",
            "src",
            "usb_desc.c",
        )
        descriptor_h = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbhs_compat_hid",
            "include",
            "usb_desc.h",
        )
        official_core = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbhs_compat_hid",
            "src",
            "ch32h417_usbhs_device.c",
        )

        self.assertIn("ch32h417_usbhs_hid_nkro_diag_t", header)
        self.assertIn("ch32h417_usbhs_hid_nkro_diag_snapshot", header)
        for token in (
            "HID_Report_Buffer[DEF_USBD_HS_PACK_SIZE + 1]",
            "s_pending_report[AIK_NKRO_REPORT_BYTES]",
            "s_pending_full",
            "s_in_flight",
            "USBHS_Device_Init(ENABLE)",
            "USBHS_DevEnumStatus",
            "USBHS_EP2_Tx_Buf",
            "USBHSD->UEP2_TX_LEN = AIK_NKRO_REPORT_BYTES",
            "USBHS_UEP_T_RES_ACK",
            "USBHS_UEP_T_RES_NAK",
            "ch32h417_usbhs_hid_nkro_pending_empty",
            "ch32h417_usbhs_hid_nkro_submit",
            "ch32h417_usbhs_hid_nkro_on_in_complete",
            "s_pending_full = 0U",
            "s_in_flight = 0U",
            "AIK_NKRO_REPORT_BYTES",
        ):
            self.assertIn(token, wrapper)
        self.assertNotIn("USBHS_IRQHandler", wrapper)
        self.assertIn("ch32h417_usbhs_hid_nkro_on_in_complete", official_core)
        assert_re(
            self,
            official_core,
            r"case DEF_UEP2:[\s\S]*ch32h417_usbhs_hid_nkro_on_in_complete\(\);",
        )

        for token in (
            "DEF_USB_PID                  0xFE18",
            "DEF_USBD_REPORT_DESC_LEN     45",
        ):
            self.assertIn(token, descriptor_h)

        for token in (
            "MyCfgDescr_HS",
            "MyCfgDescr_FS",
            "MyHIDReportDesc_HS",
            "MyHIDReportDesc_FS",
            "0x05, 0x01",
            "0x09, 0x06",
            "0x19, 0x04",
            "0x29, 0x73",
            "0x95, 0x70",
            "0x82",
            "0x10, 0x00",
            "0x01,                           // bInterval: 125us at high speed",
        ):
            self.assertIn(token, descriptor)

        for token in (
            "USBHS_IRQHandler",
            "USBHS_Device_Init",
            "RCC_USBHSPLLCLKConfig",
            "USB_SET_ADDRESS",
            "USB_SET_CONFIGURATION",
            "USB_DESCR_TYP_REPORT",
            "HID_SET_IDLE",
        ):
            self.assertIn(token, official_core)

    def test_h417_v3f_usbfs_cdc_debug_is_available_for_link_diagnostics(self):
        v3f_makefile = read_text("firmware", "h417", "v3f", "Makefile")
        main_c = read_text("firmware", "h417", "v3f", "applications", "main.c")
        wrapper = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbfs_hid_nkro",
            "src",
            "ch32h417_usbfs_hid_nkro.c",
        )
        wrapper_h = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "usbfs_hid_nkro",
            "include",
            "ch32h417_usbfs_hid_nkro.h",
        )
        official_core = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbfs_compat_hid",
            "src",
            "ch32h417_usbfs_device.c",
        )
        descriptor = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbfs_compat_hid",
            "src",
            "usb_desc.c",
        )
        descriptor_h = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "wch_usbfs_compat_hid",
            "include",
            "usb_desc.h",
        )
        link_h = read_text("firmware", "h417", "v3f", "applications", "ch585_link.h")
        link_c = read_text("firmware", "h417", "v3f", "applications", "ch585_link.c")
        spi_link_c = read_text(
            "firmware",
            "h417",
            "v3f",
            "drivers",
            "ch585_spi_link",
            "src",
            "ch32h417_ch585_spi_link.c",
        )

        self.assertIn("V3F_ENABLE_USBFS_CDC_DEBUG ?= 0", v3f_makefile)
        self.assertIn("-DV3F_ENABLE_USBFS_CDC_DEBUG=$(V3F_ENABLE_USBFS_CDC_DEBUG)", v3f_makefile)
        self.assertIn("DEF_USB_PID                  0xFE17", descriptor_h)
        self.assertIn("DEF_USB_PID                  0xFE07", descriptor_h)
        self.assertIn("ch32h417_usbfs_hid_nkro_debug_write", wrapper_h)
        self.assertIn("USBFS_CDC_Debug_Send", wrapper)
        self.assertIn("v3f_cdc_debug_poll", main_c)
        self.assertIn("ch32h417_usbfs_hid_nkro_debug_write", main_c)
        self.assertIn("last_rx_head", link_h)
        self.assertIn("last_rx_down", link_h)
        self.assertIn("last_diag", link_h)
        self.assertIn("ch32h417_ch585_spi_link_last_diag", link_c)
        self.assertIn("ch32h417_ch585_spi_link_diag_sample", spi_link_c)
        self.assertIn("raw=%02x%02x%02x%02x", main_c)
        self.assertIn("bits=%02x%02x%02x%02x%02x%02x", main_c)
        self.assertIn("diag=%08lx", main_c)
        self.assertIn("CDC_GET_LINE_CODING", official_core)
        self.assertIn("CDC_SET_LINE_CODING", official_core)
        self.assertIn("USBFS_CDC_LineCoding", official_core)
        self.assertIn("USBFS_CDC_Debug_Send", official_core)
        self.assertIn("return (uint8_t)((USBFS_DevEnumStatus != 0U) ? 1U : 0U);", official_core)
        self.assertIn("USBFSD->UEP3_DMA", official_core)
        self.assertIn("USBFS_UEP3_TX_EN", official_core)
        self.assertIn("USBFS_UEP4_TX_EN", official_core)
        assert_re(
            self,
            official_core,
            r"case\s+USBFS_UIS_TOKEN_OUT\s*\|\s*DEF_UEP0:"
            r"(?:(?!case\s+USBFS_UIS_TOKEN_OUT\s*\|\s*DEF_UEP1:)[\s\S])*"
            r"CDC_SET_LINE_CODING"
            r"(?:(?!case\s+USBFS_UIS_TOKEN_OUT\s*\|\s*DEF_UEP1:)[\s\S])*"
            r"USBFS_SetupReqLen\s*=\s*0U"
            r"(?:(?!case\s+USBFS_UIS_TOKEN_OUT\s*\|\s*DEF_UEP1:)[\s\S])*"
            r"if\(\s*USBFS_SetupReqLen\s*==\s*0\s*\)"
            r"(?:(?!case\s+USBFS_UIS_TOKEN_OUT\s*\|\s*DEF_UEP1:)[\s\S])*"
            r"USBFSD->UEP0_TX_LEN\s*=\s*0"
            r"(?:(?!case\s+USBFS_UIS_TOKEN_OUT\s*\|\s*DEF_UEP1:)[\s\S])*"
            r"USBFSD->UEP0_TX_CTRL\s*=\s*USBFS_UEP_T_TOG\s*\|\s*USBFS_UEP_T_RES_ACK",
        )
        self.assertIn("0x08, 0x0B", descriptor)
        self.assertIn("0x02,                           // bInterfaceClass CDC", descriptor)
        self.assertIn("0x0A,                           // bInterfaceClass CDC data", descriptor)
        self.assertIn("0x03,                           // bInterfaceClass HID", descriptor)
        self.assertIn("0x83,                           // bEndpointAddress: CDC bulk IN EP3", descriptor)
        self.assertIn("0x82,                           // bEndpointAddress: HID IN EP2", descriptor)

    def test_legacy_64_key_source_assumption_is_not_in_product_path(self):
        h417_scan_h = read_text(
            "firmware", "h417", "v5f_rtthread", "applications", "ch585_spi_scan.h"
        )
        self.assertNotIn("CH585_SCAN_KEYS_PER_SOURCE 64U", h417_scan_h)

    def test_ch585_half_scan_left_owns_1k_rf_tx_runtime(self):
        source = read_text("firmware", "ch585", "applications", "half_scan", "ch585_rf_nkro_tx.c")
        self.assertIn("RF_SYNC_WORD        0xA55A1234UL", source)
        self.assertIn("RF_CHANNEL          16", source)
        self.assertIn("RF_FRAME_MAGIC      0x55", source)
        self.assertIn("RF_TX_TIMER_HZ      1000", source)
        self.assertIn("ch585_rf_nkro_tx_set_report", source)
        self.assertIn("TMR0_IRQHandler", source)
        self.assertNotIn("SPI0_SlaveInit", source)


if __name__ == "__main__":
    unittest.main()
