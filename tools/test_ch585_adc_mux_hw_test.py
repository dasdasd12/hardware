import io
import os
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
MAKEFILE = os.path.join(ROOT, "hw_tests", "ch585", "Makefile")
COMMON_H = os.path.join(ROOT, "hw_tests", "ch585", "src", "ch585_common.h")
COMMON_C = os.path.join(ROOT, "hw_tests", "ch585", "src", "ch585_common.c")
ADC_TEST = os.path.join(ROOT, "hw_tests", "ch585", "src", "ch585_adc_mux_scan.c")
ADC_DRIVER_H = os.path.join(ROOT, "firmware", "ch585", "drivers", "ch585_ads7948_mux_acq.h")
ADC_DRIVER_C = os.path.join(ROOT, "firmware", "ch585", "drivers", "ch585_ads7948_mux_acq.c")
OLD_ADC_SCAN_H = os.path.join(ROOT, "firmware", "ch585", "drivers", "ch585_ads7948_mux_scan.h")
OLD_ADC_SCAN_C = os.path.join(ROOT, "firmware", "ch585", "drivers", "ch585_ads7948_mux_scan.c")


def read_text(path):
    with io.open(path, "r", encoding="utf-8") as handle:
        return handle.read()


class Ch585AdcMuxHwTestPolicy(unittest.TestCase):
    def test_ch585_adc_mux_hw_test_imports_fork_driver_code(self):
        makefile = read_text(MAKEFILE)

        self.assertIn("HALF ?= left", makefile)
        self.assertIn("CH585_HALF_ROLE := left", makefile)
        self.assertIn("CH585_HALF_ROLE := right", makefile)
        self.assertIn("ch585_adc_mux_scan", makefile)
        self.assertIn("$(CH585_FIRMWARE_ROOT)/drivers", makefile)
        self.assertIn("ch585_ads7948_mux_acq.c", makefile)
        self.assertNotIn("ads7948.c", makefile)
        self.assertNotIn("ch585_ads7948_mux_scan.c", makefile)
        self.assertIn("CH58x_spi1.c", makefile)
        self.assertNotIn("ADC_MUX_MODE", makefile)
        self.assertNotIn("CH585_ADC_MUX_FAST_MODE", makefile)
        self.assertIn("CH585_SYSCLK_SOURCE=CLK_SOURCE_HSE_PLL_78MHz", makefile)
        self.assertIn("FREQ_SYS=78000000", makefile)
        self.assertIn("CH585_ADC_MUX_HALF_LEFT", makefile)
        self.assertIn("CH585_ADC_MUX_HALF_RIGHT", makefile)
        self.assertNotIn("CH585_ADC_MUX_HALF_U2", makefile)
        self.assertNotIn("CH585_ADC_MUX_HALF_U3", makefile)

    def test_ch585_adc_mux_old_scan_driver_is_removed(self):
        self.assertFalse(os.path.exists(OLD_ADC_SCAN_H))
        self.assertFalse(os.path.exists(OLD_ADC_SCAN_C))

    def test_ch585_adc_mux_hw_test_is_declared_in_common_header(self):
        common_h = read_text(COMMON_H)

        self.assertIn("void ch585_adc_mux_scan_run(void);", common_h)

    def test_ch585_common_allows_per_test_sysclk_source(self):
        common_c = read_text(COMMON_C)

        self.assertIn("CH585_SYSCLK_SOURCE", common_c)
        self.assertIn("SetSysClock(CH585_SYSCLK_SOURCE);", common_c)

    def test_ch585_adc_mux_firmware_driver_owns_latex_half_mux_ranges(self):
        header = read_text(ADC_DRIVER_H)
        source = read_text(ADC_DRIVER_C)

        self.assertIn("ch585_ads7948_mux_lane_t lanes[CH585_ADS7948_MUX_LANE_COUNT]", header)
        self.assertIn("CH585_ADS7948_MUX_LANE(0U, 0U, CH585_ADS7948_MUX_CS_PB14, \"CS1\", 0U, 9U)", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(0U, 1U, CH585_ADS7948_MUX_CS_PB14, \"CS1\", 0U, 9U)", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(1U, 0U, CH585_ADS7948_MUX_CS_PB15, \"CS2\", 0U, 9U)", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(1U, 1U, CH585_ADS7948_MUX_CS_PB15, \"CS2\", 0U, 9U)", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(0U, 0U, CH585_ADS7948_MUX_CS_PB15, \"CS1'\", 0U, 10U)", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(0U, 1U, CH585_ADS7948_MUX_CS_PB15, \"CS1'\", 0U, 10U)", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(1U, 0U, CH585_ADS7948_MUX_CS_PB14, \"CS2'\", 0U, 10U)", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(1U, 1U, CH585_ADS7948_MUX_CS_PB14, \"CS2'\", 0U, 11U)", source)
        self.assertIn('"left"', source)
        self.assertIn('"right"', source)
        self.assertIn("ch585_ads7948_mux_profile_active_keys", source)
        self.assertIn('"left all"', source)
        self.assertNotIn('"left L3 D7 A"', source)
        self.assertIn("CH585_ADS7948_MUX_KEY_COUNT", header)

    def test_ch585_adc_mux_hw_test_is_thin_shell_over_firmware_driver(self):
        source = read_text(ADC_TEST)

        self.assertIn('#include "ch585_ads7948_mux_acq.h"', source)
        self.assertIn("ch585_ads7948_mux_gpio_init();", source)
        self.assertIn("ch585_ads7948_mux_acq_init(&g_ch585_adc_mux_acq, profile)", source)
        self.assertIn("ch585_ads7948_mux_acq_poll(&g_ch585_adc_mux_acq);", source)
        self.assertIn("ch585_adc_mux_log_frame(&g_ch585_adc_mux_acq);", source)
        self.assertNotIn("GPIO_Pin_0", source)
        self.assertNotIn("SPI1_MasterDefInit", source)
        self.assertNotIn("SPI1_MasterRecvByte", source)
        self.assertNotIn("R8_SPI1", source)
        self.assertNotIn("ch585_adc_mux_fast_read_adc_code", source)
        self.assertNotIn("ch585_adc_mux_fast_poll_profile", source)

    def test_ch585_adc_mux_hw_test_uses_serial_not_usb_visualizer(self):
        source = read_text(ADC_TEST)

        self.assertIn("ch585_log_", source)
        self.assertIn("scale=1024", source)
        self.assertIn("travel=", source)
        self.assertIn("CH585_ADC_MUX_BAR_WIDTH", source)
        self.assertIn("ch585_adc_mux_log_bar", source)
        self.assertNotIn("CH585_ADC_MUX_FAST_TELEMETRY_INTERVAL", source)
        self.assertNotIn("USB_HID", source)
        self.assertNotIn("USBHS", source)
        self.assertNotIn("hidapi", source)
        self.assertNotIn("read_rf8k_usbhs", source)

    def test_ch585_adc_mux_firmware_driver_binds_latex_adc_mux_pins(self):
        source = read_text(ADC_DRIVER_C)

        for token in (
            "GPIO_Pin_0",
            "GPIO_Pin_1",
            "GPIO_Pin_2",
            "GPIO_Pin_3",
            "GPIO_Pin_14",
            "GPIO_Pin_15",
            "GPIO_Pin_18",
            "GPIO_Pin_19",
            "SPI1_MasterDefInit",
            "R8_SPI1_BUFFER",
        ):
            self.assertIn(token, source)
        self.assertIn("CH585_ADC_MUX_CH_SEL_PB18", source)
        self.assertNotIn("CH585_ADC_MUX_CH_SEL_PA1", source)

    def test_ch585_adc_mux_firmware_driver_has_per_lane_mux_channel_limit(self):
        header = read_text(ADC_DRIVER_H)
        source = read_text(ADC_DRIVER_C)

        self.assertIn("uint8_t mux_first;", header)
        self.assertIn("uint8_t mux_count;", header)
        self.assertIn("lane->mux_first", source)
        self.assertIn("lane->mux_count", source)
        self.assertIn("ch585_ads7948_mux_lane_has_mux", source)

    def test_ch585_adc_mux_lanes_follow_latex_ads7948_channel_order(self):
        source = read_text(ADC_DRIVER_C)

        self.assertIn("CH585_ADS7948_MUX_LANE(0U, 0U", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(0U, 1U", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(1U, 0U", source)
        self.assertIn("CH585_ADS7948_MUX_LANE(1U, 1U", source)

    def test_ch585_adc_mux_fast_path_targets_two_msps_without_conservative_reads(self):
        source = read_text(ADC_DRIVER_C)

        self.assertIn("CH585_ADS7948_MUX_TARGET_SPS 2000000U", source)
        self.assertIn("CH585_ADS7948_MUX_SYSCLK_HZ 78000000U", source)
        self.assertIn("CH585_ADS7948_MUX_SPI_CLOCK_DIV 2U", source)
        self.assertIn("CH585_ADS7948_MUX_FRAME_CLOCKS 16U", source)
        self.assertIn("CH585_ADS7948_MUX_FRAME_SPS", source)
        self.assertIn("#if CH585_ADS7948_MUX_FRAME_SPS < CH585_ADS7948_MUX_TARGET_SPS", source)
        self.assertIn("ch585_ads7948_mux_acq_poll", source)
        self.assertIn("ch585_ads7948_mux_read_adc_code", source)
        self.assertNotIn("ads7948_read_channel", source)

    def test_ch585_adc_mux_fast_path_reuses_frame_logs_for_validation(self):
        source = read_text(ADC_TEST)

        self.assertIn("static void ch585_adc_mux_log_frame", source)
        self.assertIn(
            "ch585_adc_mux_log_lane_raw(acq->profile, raw, lane)",
            source,
        )
        self.assertIn("ch585_adc_mux_log_frame(&g_ch585_adc_mux_acq);", source)
        self.assertIn('ch585_log_str("\\r\\nFRAME side=");', source)
        self.assertNotIn('ch585_log_str("\\r\\nFAST side=");', source)

    def test_ch585_adc_mux_fast_path_logs_every_completed_frame(self):
        source = read_text(ADC_TEST)

        self.assertIn(
            "while(1)\n"
            "    {\n"
            "        ch585_ads7948_mux_acq_poll(&g_ch585_adc_mux_acq);\n"
            "        ch585_adc_mux_log_frame(&g_ch585_adc_mux_acq);\n"
            "    }",
            source,
        )
        self.assertNotIn("g_ch585_adc_mux_fast_frames %", source)


if __name__ == "__main__":
    unittest.main()
