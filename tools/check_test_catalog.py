#!/usr/bin/env python
"""Validate the organized hardware and PC-side test catalogs."""

from __future__ import print_function

import argparse
import io
import json
import os
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
HW_CATALOG = os.path.join(ROOT, "hw_tests", "catalog.json")
PC_CATALOG = os.path.join(ROOT, "tests", "catalog.json")
ALLOWED_STATUS = {"active", "bringup", "feasibility", "quarantined", "deprecated"}
REQUIRED_32K_IDS = {
    "h417_v3f_usbss_ch372_baseline",
    "h417_v3f_usbss_fs_diag",
    "h417_v5f_ch585_spi_speed",
    "ch585_ads7948_32k_pipeline",
    "ch585_spi0_speed_slave",
}
REMOVED_PATHS = (
    "hw_tests/h417/passed",
    "test/Makefile",
    "test/README.md",
    "hw_tests/ch585/src/ch585_ads7948_mux_probe.c",
    "firmware/ch585/applications/rf_keyboard_tx",
    "firmware/ch585/applications/rf_receiver_usbfs",
    "firmware/ch585/tools/ch585_half_scan_uart_monitor.py",
)


def repo_path(relative):
    return os.path.join(ROOT, *relative.split("/"))


def load_json(path):
    with io.open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def validate_catalog_header(catalog, name, errors):
    if catalog.get("schema_version") != 1:
        errors.append("%s schema_version must be 1" % name)
    if not isinstance(catalog.get("tests"), list):
        errors.append("%s tests must be a list" % name)


def validate_hardware(catalog, errors):
    seen = set()
    entries = catalog.get("tests", [])
    for entry in entries:
        test_id = entry.get("id")
        if not test_id:
            errors.append("hardware entry missing id")
            continue
        if test_id in seen:
            errors.append("duplicate hardware id: %s" % test_id)
        seen.add(test_id)

        status = entry.get("status")
        if status not in ALLOWED_STATUS:
            errors.append("%s has invalid status %r" % (test_id, status))
        if status == "feasibility" and entry.get("preserve") is not True:
            errors.append("%s feasibility test must set preserve=true" % test_id)
        if entry.get("destructive") is True and not entry.get("destructive_scope"):
            errors.append("%s destructive test needs destructive_scope" % test_id)

        build = entry.get("build", {})
        directory = build.get("directory")
        if not directory or not os.path.isdir(repo_path(directory)):
            errors.append("%s build directory missing: %r" % (test_id, directory))
            continue
        makefile = os.path.join(repo_path(directory), "Makefile")
        if not os.path.isfile(makefile):
            errors.append("%s Makefile missing under %s" % (test_id, directory))
            continue

        if build.get("kind") == "make-selector":
            value = build.get("value")
            selector = build.get("selector")
            with io.open(makefile, "r", encoding="utf-8", errors="ignore") as handle:
                make_text = handle.read()
            if not selector or not value:
                errors.append("%s selector build needs selector and value" % test_id)
            elif value not in make_text:
                errors.append("%s target %s missing from %s" % (test_id, value, directory))
            for alias in build.get("aliases", []):
                if alias not in make_text:
                    errors.append("%s alias %s missing from %s" % (test_id, alias, directory))
            for variant in build.get("variants", [{}]):
                if not isinstance(variant, dict):
                    errors.append("%s build variant must be an object" % test_id)
        elif build.get("kind") != "make":
            errors.append("%s has unsupported build kind %r" % (test_id, build.get("kind")))

    missing_32k = REQUIRED_32K_IDS - seen
    if missing_32k:
        errors.append("required 32K feasibility tests missing: %s" % ", ".join(sorted(missing_32k)))


def validate_pc(catalog, errors):
    seen = set()
    cataloged_pytests = set()
    cataloged_host_tests = set()
    for entry in catalog.get("tests", []):
        test_id = entry.get("id")
        if not test_id:
            errors.append("PC-side entry missing id")
            continue
        if test_id in seen:
            errors.append("duplicate PC-side id: %s" % test_id)
        seen.add(test_id)
        if entry.get("status") not in ALLOWED_STATUS:
            errors.append("%s has invalid status %r" % (test_id, entry.get("status")))
        if entry.get("status") == "feasibility" and entry.get("preserve") is not True:
            errors.append("%s feasibility test must set preserve=true" % test_id)
        path = entry.get("path")
        if not path or not os.path.isfile(repo_path(path)):
            errors.append("%s path missing: %r" % (test_id, path))
        elif entry.get("kind") == "pytest":
            cataloged_pytests.add(path)
        elif entry.get("kind") == "host-c":
            cataloged_host_tests.add(path)

    python_root = repo_path("tests/python")
    actual_pytests = {
        "tests/python/" + name
        for name in os.listdir(python_root)
        if name.startswith("test_") and name.endswith(".py")
    }
    if cataloged_pytests != actual_pytests:
        errors.append(
            "pytest catalog mismatch: missing=%s extra=%s"
            % (
                sorted(actual_pytests - cataloged_pytests),
                sorted(cataloged_pytests - actual_pytests),
            )
        )

    host_root = repo_path("tests/host")
    actual_host_tests = {
        "tests/host/" + name
        for name in os.listdir(host_root)
        if name.startswith("host_") and name.endswith("_test.c")
    }
    if cataloged_host_tests != actual_host_tests:
        errors.append(
            "host C catalog mismatch: missing=%s extra=%s"
            % (
                sorted(actual_host_tests - cataloged_host_tests),
                sorted(cataloged_host_tests - actual_host_tests),
            )
        )


def validate_removed_paths(errors):
    for relative in REMOVED_PATHS:
        if os.path.exists(repo_path(relative)):
            errors.append("obsolete test path still exists: %s" % relative)


def list_hardware(catalog):
    for entry in catalog["tests"]:
        preserved = " preserve" if entry.get("preserve") else ""
        print(
            "{:<42} {:<11} {:<17}{}"
            .format(entry["id"], entry["status"], entry["category"], preserved)
        )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-hardware", action="store_true")
    args = parser.parse_args(argv)

    errors = []
    hardware = load_json(HW_CATALOG)
    pc_side = load_json(PC_CATALOG)
    validate_catalog_header(hardware, "hardware", errors)
    validate_catalog_header(pc_side, "PC-side", errors)
    validate_hardware(hardware, errors)
    validate_pc(pc_side, errors)
    validate_removed_paths(errors)

    if errors:
        for error in errors:
            print("FAIL: %s" % error)
        return 1

    if args.list_hardware:
        list_hardware(hardware)
    print(
        "PASS: test catalogs valid (%u hardware, %u PC-side entries, %u preserved 32K firmware tests)"
        % (len(hardware["tests"]), len(pc_side["tests"]), len(REQUIRED_32K_IDS))
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
