import imp
import os
import re
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
LATEX = os.path.abspath(os.path.join(ROOT, os.pardir, "latex", "contest_report_template.tex"))
CHECK_SCRIPT = os.path.join(ROOT, "tools", "check_firmware_hardware_contract.py")


def load_checker():
    return imp.load_source("check_firmware_hardware_contract", CHECK_SCRIPT)


def violation_text(violations):
    return "\n".join(v.format() for v in violations)


class FirmwareHardwareContractTests(unittest.TestCase):
    def test_latex_contract_does_not_declare_h417_uart8(self):
        checker = load_checker()

        contract = checker.load_contract(LATEX)

        self.assertNotIn("UART8", contract.text_upper)
        self.assertNotIn("USART8", contract.text_upper)

    def test_latex_contract_extracts_h417_ch585_spi_pins(self):
        checker = load_checker()

        contract = checker.load_contract(LATEX)

        self.assertEqual(contract.h417_ch585_spi_pins, set(["PB3", "PB4", "PB5", "PF2", "PD9"]))

    def test_fixed_firmware_no_longer_reports_uart8_pb1_or_stale_soft_spi(self):
        checker = load_checker()

        text = violation_text(checker.collect_violations(ROOT, LATEX))

        self.assertNotIn("h417-undeclared-uart8", text)
        self.assertNotIn("h417-led-pin-role", text)
        self.assertNotIn("h417-ch585-spi-stale-pin", text)

    def test_h417_default_firmware_keeps_ch585_spi_link_disconnected(self):
        main_path = os.path.join(
            ROOT, "firmware", "h417", "v5f_rtthread", "applications", "main.c"
        )
        with open(main_path, "r") as handle:
            main_c = handle.read()

        self.assertTrue(
            re.search(
                r"#ifndef\s+APP_ENABLE_CH585_SPI_SCAN\s*\n"
                r"#define\s+APP_ENABLE_CH585_SPI_SCAN\s+0\b",
                main_c,
            )
        )

    def test_disconnected_default_does_not_report_fake_second_ch585_source(self):
        checker = load_checker()

        text = violation_text(checker.collect_violations(ROOT, LATEX))

        self.assertNotIn("ch585-source1-fake-default", text)


if __name__ == "__main__":
    unittest.main()
