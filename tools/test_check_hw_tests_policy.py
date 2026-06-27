import io
import os
import re


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
CHECK_SCRIPT = os.path.join(ROOT, "tools", "check_hw_tests.py")


def read_check_script():
    with io.open(CHECK_SCRIPT, "r", encoding="utf-8") as handle:
        return handle.read()


def test_ch585_adc_and_spi_are_not_forbidden_keywords():
    text = read_check_script()

    assert "CH585 ADC use" not in text
    assert "CH585 SPI use" not in text
    assert not re.search(r"forbidden_ch585\s*=\s*{", text)


def test_ch585_firmware_test_residue_is_checked_explicitly():
    text = read_check_script()

    assert "assert_ch585_firmware_has_no_test_residue" in text
    assert "CH585 firmware test residue" in text
