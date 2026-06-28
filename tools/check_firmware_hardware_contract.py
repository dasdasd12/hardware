# -*- coding: utf-8 -*-
from __future__ import print_function

import argparse
import io
import os
import re
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
DEFAULT_LATEX = os.path.abspath(
    os.path.join(ROOT, os.pardir, "latex", "contest_report_template.tex")
)


class HardwareContract(object):
    def __init__(self, text, h417_ch585_spi_pins, h417_indicator_pins,
                 left_key_count, right_key_count):
        self.text = text
        self.text_upper = text.upper()
        self.h417_ch585_spi_pins = h417_ch585_spi_pins
        self.h417_indicator_pins = h417_indicator_pins
        self.left_key_count = left_key_count
        self.right_key_count = right_key_count


class Violation(object):
    def __init__(self, rule, path, line, message):
        self.rule = rule
        self.path = path
        self.line = line
        self.message = message

    def format(self):
        if self.line:
            location = "{0}:{1}".format(self.path, self.line)
        else:
            location = self.path
        return "{0}: {1}: {2}".format(self.rule, location, self.message)


def read_text(path):
    with io.open(path, "r", encoding="utf-8", errors="ignore") as handle:
        return handle.read()


def section(text, start_pattern, end_pattern):
    start = re.search(start_pattern, text, flags=re.IGNORECASE)
    if start is None:
        return ""
    end = re.search(end_pattern, text[start.end():], flags=re.IGNORECASE)
    if end is None:
        return text[start.end():]
    return text[start.end():start.end() + end.start()]


def compact_pin(pin_text):
    return pin_text.replace(".", "").upper()


def dotted_pin(pin_text):
    pin = compact_pin(pin_text)
    match = re.match(r"^(P[A-F])(\d+)$", pin)
    if match:
        return "{0}.{1}".format(match.group(1), match.group(2))
    return pin_text


def extract_pins(text):
    return set(compact_pin(pin) for pin in re.findall(r"\bP[A-F]\.?\d+\b", text, flags=re.IGNORECASE))


def extract_key_count(text, label):
    match = re.search(label + u"\\s*(\\d+)\\s*键", text)
    if match is None:
        return None
    return int(match.group(1))


def load_contract(latex_path):
    text = read_text(latex_path)
    spi_section = section(text, r"\\subheading\{6\.4[^}]*\}", r"\\subheading\{6\.5")
    indicator_section = section(text, r"\\subheading\{6\.6[^}]*\}", r"\\parttitle")
    return HardwareContract(
        text=text,
        h417_ch585_spi_pins=extract_pins(spi_section),
        h417_indicator_pins=extract_pins(indicator_section),
        left_key_count=extract_key_count(text, u"左半区"),
        right_key_count=extract_key_count(text, u"右半区"),
    )


def relpath(root, path):
    return os.path.relpath(path, root).replace(os.sep, "/")


def iter_files(root, rel_roots, suffixes):
    for rel_root in rel_roots:
        path = os.path.join(root, rel_root)
        if os.path.isfile(path):
            yield path
            continue
        if not os.path.isdir(path):
            continue
        for base, dirs, files in os.walk(path):
            dirs[:] = [
                name for name in dirs
                if name not in (".git", "build", "rt-thread")
            ]
            for name in files:
                _stem, ext = os.path.splitext(name)
                if ext.lower() in suffixes or name in ("Makefile", "rtconfig.h", "usb_config.h"):
                    yield os.path.join(base, name)


def source_lines(path):
    text = read_text(path)
    for index, line in enumerate(text.splitlines(), 1):
        yield index, line


def add(violations, rule, root, path, line, message):
    violations.append(Violation(rule, relpath(root, path), line, message))


def default_macro_enabled(root, rel_file, macro):
    path = os.path.join(root, *rel_file.split("/"))
    if not os.path.exists(path):
        return None
    pattern = re.compile(
        r"#ifndef\s+{0}\s*\n\s*#define\s+{0}\s+([0-9]+)\b".format(re.escape(macro))
    )
    match = pattern.search(read_text(path))
    if match is None:
        return None
    return int(match.group(1)) != 0


def scan_undeclared_h417_uart8(root, contract, violations):
    if "UART8" in contract.text_upper or "USART8" in contract.text_upper:
        return
    rel_roots = (
        "firmware/h417/v5f_rtthread/Makefile",
        "firmware/h417/v5f_rtthread/rtconfig.h",
        "firmware/h417/v5f_rtthread/bsp",
        "firmware/h417/v5f_rtthread/drivers",
        "firmware/h417/v5f_rtthread/applications",
        "firmware/h417/v3f/Makefile",
        "firmware/h417/v3f/applications",
        "firmware/h417/basic/wch/SRC/Debug",
        "firmware/h417/flash_dualcore.ps1",
    )
    pattern = re.compile(r"BSP_USING_UART8|DEBUG_UART8|RT_CONSOLE_DEVICE_NAME\s+\"uart8\"|"
                         r"\bUSART8\b|\bUART8\b|\buart8\b")
    for path in iter_files(root, rel_roots, (".c", ".h", ".mk", ".ps1")):
        for line_number, line in source_lines(path):
            match = pattern.search(line)
            if match:
                add(
                    violations,
                    "h417-undeclared-uart8",
                    root,
                    path,
                    line_number,
                    "{0} appears in default/board firmware, but /latex does not declare H417 UART8/USART8".format(
                        match.group(0)
                    ),
                )


def scan_eval_board_led(root, contract, violations):
    rel_roots = (
        "firmware/h417/v5f_rtthread/applications",
        "firmware/h417/v5f_rtthread/bsp",
        "firmware/h417/v3f/applications",
    )
    led_pin_pattern = re.compile(r"LED[^\\n]*rt_pin_get\(\"(P[A-F]\.\d+)\"\)|"
                                 r"rt_pin_get\(\"(P[A-F]\.\d+)\"\)[^\\n]*LED", re.IGNORECASE)
    eval_led_pattern = re.compile(r"Eval board.*LED|LED_PIN", re.IGNORECASE)
    for path in iter_files(root, rel_roots, (".c", ".h")):
        last_led_context = False
        for line_number, line in source_lines(path):
            if eval_led_pattern.search(line):
                last_led_context = True
            match = led_pin_pattern.search(line)
            if match:
                pin = match.group(1) or match.group(2)
                compact = compact_pin(pin)
                if compact not in contract.h417_indicator_pins:
                    add(
                        violations,
                        "h417-led-pin-role",
                        root,
                        path,
                        line_number,
                        "LED uses {0}, but /latex indicator pins are {1}".format(
                            pin,
                            ", ".join(sorted(contract.h417_indicator_pins)),
                        ),
                    )
            elif last_led_context and "rt_pin_get" in line:
                pin_match = re.search(r"\"(P[A-F]\.\d+)\"", line, flags=re.IGNORECASE)
                if pin_match:
                    pin = pin_match.group(1)
                    compact = compact_pin(pin)
                    if compact not in contract.h417_indicator_pins:
                        add(
                            violations,
                            "h417-led-pin-role",
                            root,
                            path,
                            line_number,
                            "LED context uses {0}, but that pin is not a /latex indicator network".format(pin),
                        )
                last_led_context = False


def scan_ch585_spi_defaults(root, contract, violations):
    path = os.path.join(root, "firmware", "h417", "v5f_rtthread", "applications", "ch585_spi_scan.c")
    if not os.path.exists(path):
        return
    default_scan_enabled = default_macro_enabled(
        root,
        "firmware/h417/v5f_rtthread/applications/main.c",
        "APP_ENABLE_CH585_SPI_SCAN",
    )
    soft_pin_pattern = re.compile(r"#define\s+(APP_CH585_SPI_PIN_[A-Z0-9_]+)\s+\"(P[A-F]\.\d+)\"")
    fake_source_pattern = re.compile(r"#define\s+APP_CH585_SPI_FAKE_SOURCE1\s+1\b")
    for line_number, line in source_lines(path):
        soft_pin = soft_pin_pattern.search(line)
        if soft_pin:
            macro = soft_pin.group(1)
            pin = soft_pin.group(2)
            compact = compact_pin(pin)
            if compact not in contract.h417_ch585_spi_pins:
                add(
                    violations,
                    "h417-ch585-spi-stale-pin",
                    root,
                    path,
                    line_number,
                    "{0} defaults to {1}, outside /latex H417-CH585 SPI pins {2}".format(
                        macro,
                        pin,
                        ", ".join(sorted(contract.h417_ch585_spi_pins)),
                    ),
                )
        if default_scan_enabled is not False and fake_source_pattern.search(line):
            add(
                violations,
                "ch585-source1-fake-default",
                root,
                path,
                line_number,
                "APP_CH585_SPI_FAKE_SOURCE1 defaults source1 to fake data although /latex defines two CH585 halves",
            )


def scan_half_key_counts(root, contract, violations):
    path = os.path.join(root, "firmware", "h417", "v5f_rtthread", "applications", "ch585_spi_scan.h")
    if not os.path.exists(path):
        return
    if contract.left_key_count is None or contract.right_key_count is None:
        return
    for line_number, line in source_lines(path):
        match = re.search(r"#define\s+CH585_SCAN_KEYS_PER_SOURCE\s+64U\b", line)
        if match:
            add(
                violations,
                "ch585-half-key-count",
                root,
                path,
                line_number,
                "code assumes 64 keys/source; /latex says left={0}, right={1} and forbids assuming every MUX channel is populated".format(
                    contract.left_key_count,
                    contract.right_key_count,
                ),
            )


def scan_ch585_default_app(root, violations):
    path = os.path.join(root, "firmware", "ch585", "Makefile")
    if not os.path.exists(path):
        return
    for line_number, line in source_lines(path):
        if re.search(r"^APP\s*\?=\s*rf_basic\b", line):
            add(
                violations,
                "ch585-default-app",
                root,
                path,
                line_number,
                "CH585 default APP is rf_basic, not the /latex half-scan ADS7948/MUX controller firmware",
            )


def collect_violations(root, latex_path):
    root = os.path.abspath(root)
    contract = load_contract(latex_path)
    violations = []
    scan_undeclared_h417_uart8(root, contract, violations)
    scan_eval_board_led(root, contract, violations)
    scan_ch585_spi_defaults(root, contract, violations)
    scan_half_key_counts(root, contract, violations)
    scan_ch585_default_app(root, violations)
    return violations


def main(argv):
    parser = argparse.ArgumentParser(
        description="Check firmware hardware assumptions against ../latex as the source of truth."
    )
    parser.add_argument("--repo-root", default=ROOT)
    parser.add_argument("--latex", default=DEFAULT_LATEX)
    args = parser.parse_args(argv)

    violations = collect_violations(args.repo_root, args.latex)
    if violations:
        for violation in violations:
            print("FAIL: {0}".format(violation.format()))
        return 1
    print("PASS: firmware hardware assumptions match /latex")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
