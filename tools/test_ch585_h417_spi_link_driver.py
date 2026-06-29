import io
import os
import re
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
H417_DRIVER_H = os.path.join(
    ROOT,
    "firmware",
    "h417",
    "v3f",
    "drivers",
    "ch585_spi_link",
    "include",
    "ch32h417_ch585_spi_link.h",
)
H417_DRIVER_C = os.path.join(
    ROOT,
    "firmware",
    "h417",
    "v3f",
    "drivers",
    "ch585_spi_link",
    "src",
    "ch32h417_ch585_spi_link.c",
)
CH585_DRIVER_H = os.path.join(ROOT, "firmware", "ch585", "drivers", "ch585_spi0_slave_link.h")
CH585_DRIVER_C = os.path.join(ROOT, "firmware", "ch585", "drivers", "ch585_spi0_slave_link.c")
H417_V5F_MAKEFILE = os.path.join(ROOT, "firmware", "h417", "v5f_rtthread", "Makefile")
CH585_HW_TEST_MAKEFILE = os.path.join(ROOT, "hw_tests", "ch585", "Makefile")
CH585_HW_TEST = os.path.join(ROOT, "hw_tests", "ch585", "src", "ch585_spi0_speed_slave.c")
H417_HW_TEST = os.path.join(
    ROOT, "hw_tests", "h417", "passed", "v5f_rtthread", "src", "v5f_hw_test.c"
)


def read_text(path):
    with io.open(path, "r", encoding="utf-8") as handle:
        return handle.read()


class Ch585H417SpiLinkDriverPolicy(unittest.TestCase):
    def test_h417_stable_12m5_driver_exists_and_is_buildable(self):
        self.assertTrue(os.path.exists(H417_DRIVER_H))
        self.assertTrue(os.path.exists(H417_DRIVER_C))

        header = read_text(H417_DRIVER_H)
        source = read_text(H417_DRIVER_C)
        makefile = read_text(H417_V5F_MAKEFILE)

        self.assertIn("CH32H417_CH585_SPI_LINK_SPI_KHZ 12500U", header)
        self.assertIn("CH32H417_CH585_SPI_LINK_SIDE_LEFT", header)
        self.assertIn("ch32h417_ch585_spi_link_config_for_side", header)
        self.assertIn("ch32h417_ch585_spi_link_init", header)
        self.assertIn("ch32h417_ch585_spi_link_transfer", header)
        self.assertIn("SPI_BaudRatePrescaler_Mode2", source)
        self.assertIn("SPI_CPHA_1Edge", source)
        self.assertIn("SPI_HIGH_SPEED_MODE1, DISABLE", source)
        self.assertIn("SPI_HIGH_SPEED_MODE2, DISABLE", source)
        self.assertIn("GPIO_PinSource3", source)
        self.assertIn("GPIO_PinSource4", source)
        self.assertIn("GPIO_PinSource5", source)
        self.assertIn("CH585_SPI_LINK_ROOT", makefile)
        self.assertIn("ch32h417_ch585_spi_link.c", makefile)

    def test_ch585_stable_slave_driver_exists_and_is_used_by_hw_test(self):
        self.assertTrue(os.path.exists(CH585_DRIVER_H))
        self.assertTrue(os.path.exists(CH585_DRIVER_C))

        header = read_text(CH585_DRIVER_H)
        source = read_text(CH585_DRIVER_C)
        makefile = read_text(CH585_HW_TEST_MAKEFILE)
        hw_test = read_text(CH585_HW_TEST)

        self.assertIn("ch585_spi0_slave_link_init", header)
        self.assertIn("ch585_spi0_slave_link_serve_tx_frame", header)
        self.assertIn("ch585_spi0_slave_link_get_stats", header)
        self.assertIn("last_rx_count", header)
        self.assertIn("last_rx_head", header)
        self.assertIn("GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14, GPIO_ModeIN_PU)", source)
        self.assertIsNotNone(
            re.search(r"GPIOA_ModeCfg\(\s*GPIO_Pin_15\s*,\s*GPIO_ModeIN_\w+\s*\)", source)
        )
        self.assertIsNone(
            re.search(r"GPIOA_ModeCfg\(\s*GPIO_Pin_15\s*,\s*GPIO_ModeOut_PP", source)
        )
        self.assertIn("SetFirstData", source)
        self.assertIn("RB_SPI_IF_CNT_END", source)
        self.assertIn("RB_SPI_SLV_SELECT", source)
        self.assertIn("s_ch585_spi0_slave_link_last_rx_count", source)
        self.assertIn("s_ch585_spi0_slave_link_last_rx_head", source)
        self.assertIn("ch585_spi0_slave_link.c", makefile)
        self.assertIn("#include \"ch585_spi0_slave_link.h\"", hw_test)
        self.assertIn("ch585_spi0_slave_link_serve_tx_frame", hw_test)
        self.assertNotIn("CH585_SPI0_SPEED_LOG_INTERVAL", hw_test)
        self.assertNotIn("ch585_spi0_speed_log_stat", hw_test)

    def test_h417_hw_test_uses_stable_driver_for_div8_path(self):
        source = read_text(H417_HW_TEST)

        self.assertIn("#include \"ch32h417_ch585_spi_link.h\"", source)
        self.assertIn("ch585_spi_speed_is_stable_12m5", source)
        self.assertIn("ch32h417_ch585_spi_link_config_for_side", source)
        self.assertIn("ch32h417_ch585_spi_link_init", source)
        self.assertIn("ch32h417_ch585_spi_link_transfer", source)
        self.assertIn("CH32H417_CH585_SPI_LINK_SPI_KHZ", source)


if __name__ == "__main__":
    unittest.main()
