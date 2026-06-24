import io
import os
import re
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
H417_ROOT = os.path.join(ROOT, "hw_tests", "h417")
CH585_ROOT = os.path.join(ROOT, "hw_tests", "ch585")


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


def main():
    h417_makefile = os.path.join(H417_ROOT, "Makefile")
    ch585_makefile = os.path.join(CH585_ROOT, "Makefile")

    assert_contains(h417_makefile, r"\bHW_TEST\s*\?=", "HW_TEST selection")
    assert_contains(ch585_makefile, r"\bTEST\s*\?=", "TEST selection")
    assert_contains(ch585_makefile, r"\bHALF\s*\?=", "HALF selection")
    assert_contains(h417_makefile, r"Core_V3F", "H417 V3F-only build define")
    assert_contains(h417_makefile, r"startup_ch32h417_v3f\.S", "official H417 V3F startup")
    assert_not_contains(h417_makefile, r"_dual\.hex|Core_V5F|startup_h417_v5f|Link_h417_v5f", "H417 V5F or dual-core test flow")
    assert_contains(
        os.path.join(H417_ROOT, "src", "h417_ws2812.c"),
        r"#define\s+WS2812_LED_COUNT\s+77u",
        "WS2812 per-key LED count",
    )
    assert_contains(
        os.path.join(H417_ROOT, "src", "h417_ws2812.c"),
        r"#define\s+WS2812_TEST_LEVEL\s+0x08u",
        "low-brightness WS2812 test level",
    )
    assert_contains(
        os.path.join(H417_ROOT, "src", "h417_ws2812.c"),
        r"RGB1W_SendRAM\(",
        "PIOC RAM-mode WS2812 full-frame sender",
    )
    assert_contains(
        os.path.join(H417_ROOT, "src", "h417_rgb1w_pioc.c"),
        r"#define\s+PIOC_IO\s+PIOC_IO1_5",
        "PIOC IO1 PF13 routing",
    )
    assert_contains(
        os.path.join(H417_ROOT, "src", "h417_rgb1w_pioc.c"),
        r"GPIO_PinAFConfig\(GPIOF,\s*GPIO_PinSource13,\s*GPIO_AF5\)",
        "PF13 PIOC AF5 configuration",
    )
    assert_contains(
        os.path.join(H417_ROOT, "src", "system_ch32h417.c"),
        r"SystemCoreClock\s*=\s*100000000u",
        "100 MHz V3F clock for WCH PIOC RGB1W timing",
    )

    h417_text = scan_tree(H417_ROOT, (".c", ".h", ".S", ".ld", ".mk", ""))
    ch585_text = scan_tree(CH585_ROOT, (".c", ".h", ".S", ".ld", ".mk", ""))

    forbidden_h417 = {
        r"\bUSART\b|\bUART\b|USART_|UART_": "H417 UART/USART use",
        r"\bSPI\b|SPI_": "H417 SPI use",
        r"\bADC\b|ADC_": "H417 ADC use",
        r"\bUSB\b|USBHS|USBFS|OTG": "H417 USB use",
        r"rtthread|RT-Thread|\brt_[a-z0-9_]*": "RT-Thread dependency",
        r"PB4": "H417 PB4/MISO0 use",
    }
    for pattern, description in forbidden_h417.items():
        flags = 0 if description == "RT-Thread dependency" else re.IGNORECASE
        if re.search(pattern, h417_text, flags=flags):
            fail("h417 sources contain forbidden {0}".format(description))

    required_h417_tests = (
        "h417_gpio_status",
        "h417_ws2812",
        "h417_lcd_signal",
    )
    for name in required_h417_tests:
        if name not in h417_text:
            fail("h417 sources missing {0}".format(name))

    required_ch585_tests = (
        "ch585_u2_eeprom_i2c",
        "ch585_u2_controls_gpio",
        "ch585_u3_max17048_i2c",
        "ch585_u3_charge_gpio",
        "ch585_u3_ec11_gpio",
    )
    for name in required_ch585_tests:
        if name not in ch585_text:
            fail("ch585 sources missing {0}".format(name))

    for token in ("PA8", "PA9", "TX1", "RX1"):
        if token not in ch585_text:
            fail("ch585 sources missing serial token {0}".format(token))

    forbidden_ch585 = {
        r"\bSPI\b|SPI_": "CH585 SPI use",
        r"\bADC\b|ADC_": "CH585 ADC use",
        r"\bUSB\b|USBHS|USBFS": "CH585 USB use",
        r"\bBLE\b|Bluetooth|RF_": "CH585 wireless use",
    }
    for pattern, description in forbidden_ch585.items():
        if re.search(pattern, ch585_text, flags=re.IGNORECASE):
            fail("ch585 sources contain forbidden {0}".format(description))

    print("PASS: hardware test projects stay inside the non-SPI/ADC/USB/wireless boundary")


if __name__ == "__main__":
    main()
