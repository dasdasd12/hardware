#!/usr/bin/env python
"""Static regression checks for CH32H417 USB bring-up glue."""

from __future__ import print_function

import os
import re
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
USBHS = os.path.join(ROOT, "rtthread_port/rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417_usbhs.c")
V5F_MAKEFILE = os.path.join(ROOT, "rtthread_port/Makefile")
DUAL_CDC = os.path.join(ROOT, "rtthread_port/applications/usb_cdc_dual.c")


def read_text(path):
    with open(path, "r") as handle:
        return handle.read()


def require(condition, message):
    if not condition:
        print("FAIL: {0}".format(message))
        return False
    return True


def extract_function(source, name):
    pattern = r"static\s+\w+\s+{0}\s*\([^)]*\)\s*\{{".format(re.escape(name))
    match = re.search(pattern, source)
    if match is None:
        raise AssertionError("{0} not found".format(name))

    start = match.end()
    depth = 1
    pos = start
    while pos < len(source):
        char = source[pos]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos]
        pos += 1

    raise AssertionError("{0} body is not balanced".format(name))


def check_usbhs_out_toggle():
    body = extract_function(read_text(USBHS), "usbhs_handle_ep_out_xfer_complete")
    toggle_pos = body.find("ep->ep_toggle ^= 1U;")
    complete_pos = body.find("usbd_event_ep_out_complete_handler")
    return require(toggle_pos != -1, "USBHS non-EP0 OUT handler does not update ep_toggle") and require(
        complete_pos != -1, "USBHS non-EP0 OUT handler does not notify OUT completion"
    ) and require(
        toggle_pos < complete_pos,
        "USBHS non-EP0 OUT handler must update ep_toggle before completion callback can re-prime the endpoint",
    )


def check_usbss_default_config():
    makefile = read_text(V5F_MAKEFILE)
    dual_cdc = read_text(DUAL_CDC)
    return require(
        "APP_ENABLE_USBSS_CDC ?= 0" in makefile,
        "V5F Makefile should explicitly default USBSS CDC off instead of using the V3F-official skip macro",
    ) and require(
        "DEFS += -DAPP_ENABLE_USBSS_CDC=$(APP_ENABLE_USBSS_CDC)" in makefile,
        "V5F Makefile should pass APP_ENABLE_USBSS_CDC to the application",
    ) and require(
        "APP_USBSS_SKIP_FOR_V3F_OFFICIAL=1" not in makefile,
        "V5F Makefile must not hard-code V3F official USBSS ownership",
    ) and require(
        "USBSS CDC disabled on V5F" in dual_cdc,
        "Dual CDC init should report the explicit USBSS-disabled default path",
    )


def main():
    checks = [
        check_usbhs_out_toggle(),
        check_usbss_default_config(),
    ]
    if not all(checks):
        return 1
    print("USBHS regression checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
