#!/usr/bin/env python3
"""Monitor the V3F USBSS diagnostic stream carried by USBFS CDC."""

from __future__ import print_function

import argparse
import sys
import time

import serial
from serial.tools import list_ports


VID = 0x1A86
PID = 0xFE17
AUTO_NEXT_STEPS = {
    "BUFFERS_CLEARED",
    "USBHS_QUIESCED",
    "PLATFORM_READY",
    "PLL_ARMED",
    "PLL_READY",
    "CALL_DEVICE_INIT",
}
SESSION_SILENCE_SECONDS = 8.0


def print_now(message):
    print(message)
    sys.stdout.flush()


def advance_dtr(port, after):
    print_now("USBSS_DIAG_MONITOR control=DTR_RISE after={0}".format(after))
    port.dtr = False
    time.sleep(0.1)
    port.dtr = True


def find_port(timeout_seconds=12.0, requested_port=None):
    deadline = time.monotonic() + timeout_seconds
    while True:
        if requested_port:
            matches = [
                port.device
                for port in list_ports.comports()
                if port.device.upper() == requested_port.upper()
            ]
        else:
            matches = [
                port.device
                for port in list_ports.comports()
                if port.vid == VID and port.pid == PID
            ]
        if len(matches) == 1:
            return matches[0]
        if len(matches) > 1:
            raise RuntimeError(
                "multiple USBFS diagnostic CDC ports: " + ", ".join(matches)
            )
        if time.monotonic() >= deadline:
            if requested_port:
                raise RuntimeError(
                    "USBFS diagnostic CDC port {0} was not found within {1:.0f}s".format(
                        requested_port, timeout_seconds
                    )
                )
            raise RuntimeError(
                "USBFS diagnostic CDC device 1A86:FE17 was not found within "
                "{0:.0f}s".format(timeout_seconds)
            )
        time.sleep(0.25)


def monitor_connection(port_name, baud, no_start):
    port = serial.Serial(port=None, baudrate=baud, timeout=1.0)
    port.port = port_name
    port.dtr = False
    try:
        port.open()
        last_activity = time.monotonic()
        if not no_start:
            time.sleep(0.1)
            advance_dtr(port, after="START")
        else:
            print_now(
                "USBSS_DIAG_MONITOR mode=NO_REPORT "
                "firmware_output=INTENTIONALLY_SILENT"
            )
        while True:
            line = port.readline()

            if not line:
                if (not no_start) and (
                    time.monotonic() - last_activity >= SESSION_SILENCE_SECONDS
                ):
                    print_now("USBSS_DIAG_MONITOR silence=8s action=RECONNECT")
                    return None
                continue

            last_activity = time.monotonic()
            decoded = line.decode("ascii", "replace").rstrip()
            print_now(decoded)

            if "SS19 boot=" in decoded:
                result = decoded.split("boot=", 1)[1].split()[0]
                print_now("USBSS_DIAG_MONITOR boot={0}".format(result))
                if result == "PLL_READY":
                    advance_dtr(port, after="BOOT_PLL_READY")
                    continue
                if result == "PROBE_INVALID":
                    return 4
                print_now("USBSS_DIAG_MONITOR result={0}".format(result))
                return 3

            if "SS19 step=" in decoded:
                step = decoded.split("step=", 1)[1].split()[0]
                if step == "USBSS_ENUMERATED":
                    print_now("USBSS_DIAG_MONITOR result=PASS_USBSS_ENUMERATED")
                    return 0
                if step == "PLL_TIMEOUT":
                    print_now("USBSS_DIAG_MONITOR result=PLL_TIMEOUT")
                    return 3
                if step in AUTO_NEXT_STEPS:
                    advance_dtr(port, after=step)
    except (OSError, serial.SerialException) as exc:
        print_now("USBSS_DIAG_MONITOR connection_lost={0}".format(exc))
        return None
    finally:
        try:
            port.close()
        except (OSError, serial.SerialException):
            pass


def main():
    parser = argparse.ArgumentParser(
        description="Read USBSS link-state diagnostics from the USBFS CDC port"
    )
    parser.add_argument("--port", help="CDC COM port; auto-detect 1A86:FE17 by default")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--no-start",
        action="store_true",
        help="open USBFS without requesting the V18 result; the boot probe still runs",
    )
    parser.add_argument(
        "--reconnect-timeout",
        type=float,
        default=20.0,
        help="seconds to wait for the recovery CDC port after USBSS disrupts USBFS",
    )
    args = parser.parse_args()

    first_connection = True
    while True:
        timeout = 12.0 if first_connection else args.reconnect_timeout
        if not args.port:
            print_now(
                "USBSS_DIAG_MONITOR waiting_for=1A86:FE17 timeout={0:.0f}s".format(
                    timeout
                )
            )
        elif not first_connection:
            print_now(
                "USBSS_DIAG_MONITOR waiting_for={0} timeout={1:.0f}s".format(
                    args.port, timeout
                )
            )
        port_name = find_port(timeout, requested_port=args.port)
        print_now("USBSS_DIAG_MONITOR port={0}".format(port_name))
        result = monitor_connection(port_name, args.baud, args.no_start)
        if result is not None:
            return result
        if args.no_start:
            return 2
        first_connection = False


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
    except (OSError, RuntimeError, serial.SerialException) as exc:
        print("ERROR: {0}".format(exc), file=sys.stderr)
        sys.exit(2)
