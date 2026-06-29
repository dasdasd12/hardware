import io
import os
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
PROTO_H = os.path.join(ROOT, "hw_tests", "common", "ch585_h417_adc_key_cal_proto.h")
CH585_MAKEFILE = os.path.join(ROOT, "hw_tests", "ch585", "Makefile")
CH585_COMMON_H = os.path.join(ROOT, "hw_tests", "ch585", "src", "ch585_common.h")
CH585_SLAVE_C = os.path.join(ROOT, "hw_tests", "ch585", "src", "ch585_adc_key_cal_slave.c")
CH585_ADC_H = os.path.join(ROOT, "firmware", "ch585", "applications", "ch585_ads7948_mux_acq.h")
CH585_ADC_C = os.path.join(ROOT, "firmware", "ch585", "applications", "ch585_ads7948_mux_acq.c")
H417_MAKEFILE = os.path.join(ROOT, "hw_tests", "h417", "Makefile")
H417_V5F_MAKEFILE = os.path.join(ROOT, "firmware", "h417", "v5f_rtthread", "Makefile")
H417_V5F_H = os.path.join(ROOT, "hw_tests", "h417", "passed", "v5f_rtthread", "include", "v5f_hw_test.h")
H417_V5F_TEST = os.path.join(ROOT, "hw_tests", "h417", "passed", "v5f_rtthread", "src", "v5f_hw_test.c")


def read_text(path):
    with io.open(path, "r", encoding="utf-8") as handle:
        return handle.read()


class Ch585H417AdcKeyCalHwTestPolicy(unittest.TestCase):
    def test_shared_protocol_is_small_and_hw_test_owned(self):
        proto = read_text(PROTO_H)

        self.assertIn("CH585_H417_ADC_KEY_CAL_FRAME_BYTES 24U", proto)
        self.assertIn("CH585_H417_ADC_KEY_CAL_CMD_MAGIC", proto)
        self.assertIn("CH585_H417_ADC_KEY_CAL_SAMPLE_MAGIC", proto)
        self.assertIn("CH585_H417_ADC_KEY_CAL_CMD_SELECT", proto)
        self.assertIn("CH585_H417_ADC_KEY_CAL_FLAG_RESET_STATS", proto)
        self.assertIn("ch585_h417_adc_key_cal_crc16", proto)
        self.assertIn("ch585_h417_adc_key_cal_cmd_valid", proto)
        self.assertIn("ch585_h417_adc_key_cal_sample_valid", proto)
        self.assertNotIn("HID", proto)
        self.assertNotIn("RF", proto)

    def test_ch585_single_key_slave_entry(self):
        makefile = read_text(CH585_MAKEFILE)
        common_h = read_text(CH585_COMMON_H)
        slave = read_text(CH585_SLAVE_C)
        adc_h = read_text(CH585_ADC_H)
        adc_c = read_text(CH585_ADC_C)

        self.assertIn("ch585_adc_key_cal_slave", makefile)
        self.assertIn("ch585_adc_key_cal_slave.c", makefile)
        self.assertIn("ch585_ads7948_mux_acq.c", makefile)
        self.assertIn("ch585_spi0_slave_link.c", makefile)
        self.assertIn("CH58x_spi0.c", makefile)
        self.assertIn("void ch585_adc_key_cal_slave_run(void);", common_h)
        self.assertIn("#include \"ch585_h417_adc_key_cal_proto.h\"", slave)
        self.assertIn("#include \"ch585_spi0_slave_link.h\"", slave)
        self.assertIn("ch585_ads7948_mux_acq_read_compact_key", adc_h)
        self.assertIn("ch585_ads7948_mux_acq_read_compact_key", adc_c)
        self.assertIn("ch585_spi0_slave_link_serve_frame", slave)
        self.assertIn("s_ch585_adc_key_cal_current_key", slave)
        self.assertIn("s_ch585_adc_key_cal_min", slave)
        self.assertIn("s_ch585_adc_key_cal_max", slave)
        self.assertNotIn("HID", slave)
        self.assertNotIn("RF_", slave)

    def test_h417_v5f_cdc_bridge_entry(self):
        h417_makefile = read_text(H417_MAKEFILE)
        v5f_makefile = read_text(H417_V5F_MAKEFILE)
        header = read_text(H417_V5F_H)
        source = read_text(H417_V5F_TEST)

        self.assertIn("h417_v5f_ch585_adc_key_cal", h417_makefile)
        self.assertIn("APP_V5F_HW_TEST_MODE := ch585_adc_key_cal", h417_makefile)
        self.assertIn("APP_V5F_HW_TEST_USB_CDC := 1", h417_makefile)
        self.assertIn("APP_CH585_ADC_KEY_CAL_SOURCE_LEFT=1", h417_makefile)
        self.assertIn("APP_CH585_ADC_KEY_CAL_SOURCE_RIGHT=1", h417_makefile)
        self.assertIn("ch585_adc_key_cal", v5f_makefile)
        self.assertIn("APP_V5F_HW_TEST_CH585_ADC_KEY_CAL", header)
        self.assertIn("#include \"ch585_h417_adc_key_cal_proto.h\"", source)
        self.assertIn("run_ch585_adc_key_cal_test", source)
        self.assertIn("ch32h417_dual_cdc_init", source)
        self.assertIn("ch32h417_usb_cdc_read_line", source)
        self.assertIn("ch32h417_usb_cdc_write", source)
        self.assertIn("CAL_SAMPLE", source)
        self.assertIn("CAL_CMD", source)
        self.assertIn("ch32h417_ch585_spi_link_transfer", source)
        self.assertIn("CH585_H417_ADC_KEY_CAL_FRAME_BYTES", source)
        self.assertNotIn("hid_keyboard", source.lower())


if __name__ == "__main__":
    unittest.main()
