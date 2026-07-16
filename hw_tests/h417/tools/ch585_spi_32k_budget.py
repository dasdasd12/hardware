#!/usr/bin/env python3
"""Run the existing SPI sweep and project its stable rates onto a 32K budget."""

from __future__ import print_function

import argparse
import re
import sys
import time

import serial


FIELD_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)")


def fields(line):
    return dict(FIELD_RE.findall(line))


def integer(data, name, default=0):
    try:
        return int(data.get(name, default))
    except (TypeError, ValueError):
        return default


def stable(result):
    frames = integer(result, "frames")
    return (
        frames > 0
        and integer(result, "ok") == frames
        and integer(result, "bad_ready") == 0
        and integer(result, "timeout") == 0
        and integer(result, "bad_fixed") == 0
    )


def wire_time_us(byte_count, khz):
    return (float(byte_count) * 8.0 * 1000.0) / float(khz)


def main():
    parser = argparse.ArgumentParser(
        description="Convert the H417/CH585 SPI sweep into a 32K timing budget"
    )
    parser.add_argument("--port", required=True, help="H417 debug CDC COM port")
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument("--budget-us", type=float, default=31.25)
    parser.add_argument("--state-bytes", type=int, default=12)
    parser.add_argument("--command-bytes", type=int, default=32)
    parser.add_argument(
        "--cycle-hz",
        "--hclk-hz",
        dest="cycle_hz",
        type=int,
        default=0,
        help="override the H417 mcycle frequency when the START line was missed",
    )
    args = parser.parse_args()

    rates = []
    cycle_hz = args.cycle_hz
    deadline = time.monotonic() + args.timeout

    with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=2.0) as port:
        port.reset_input_buffer()
        port.write(b"auto\n")

        while time.monotonic() < deadline:
            raw = port.readline()
            if not raw:
                continue
            line = raw.decode("ascii", "replace").strip()
            print("device:", line)

            if line.startswith("CH585_SPI_SPEED START"):
                start = fields(line)
                cycle_hz = integer(
                    start,
                    "core",
                    integer(start, "sys", integer(start, "hclk", cycle_hz)),
                )
            elif line.startswith("SPI_RATE "):
                rates.append(fields(line))
            elif line.startswith("SPI_MAX ") and "mode=auto" in line and rates:
                break
        else:
            raise RuntimeError("timed out waiting for SPI_MAX mode=auto")

    stable_rates = [item for item in rates if stable(item)]
    if not stable_rates:
        print("FAIL: no error-free SPI rate was found")
        return 1

    best = max(stable_rates, key=lambda item: integer(item, "khz"))
    khz = integer(best, "khz")
    transfer_bytes = integer(best, "bytes")
    frames = integer(best, "frames")
    cycles = integer(best, "cycles")

    measured_frame_us = 0.0
    overhead_us = 0.0
    if cycle_hz > 0 and frames > 0:
        measured_frame_us = (
            float(cycles) * 1000000.0 / float(cycle_hz) / float(frames)
        )
        overhead_us = max(
            0.0,
            measured_frame_us - wire_time_us(transfer_bytes, khz),
        )

    state_transaction_us = wire_time_us(args.state_bytes, khz) + overhead_us
    state_pair_us = state_transaction_us * 2.0
    current_half_us = (
        wire_time_us(args.command_bytes + args.state_bytes, khz)
        + overhead_us * 2.0
    )
    current_pair_us = current_half_us * 2.0
    state_margin_us = args.budget_us - state_pair_us

    print(
        "SPI32_RESULT best={0} khz={1} measured_frame_us={2:.3f} "
        "estimated_overhead_us={3:.3f} state_bytes={4} "
        "two_half_state_us={5:.3f} current_command_bytes={6} "
        "two_half_current_us={7:.3f} budget_us={8:.3f} margin_us={9:.3f}".format(
            best.get("name", "unknown"),
            khz,
            measured_frame_us,
            overhead_us,
            args.state_bytes,
            state_pair_us,
            args.command_bytes,
            current_pair_us,
            args.budget_us,
            state_margin_us,
        )
    )

    if cycle_hz == 0:
        print(
            "WARN: START line was not observed; estimates exclude software/CS overhead. "
            "Reset H417 and rerun or pass --cycle-hz for measured overhead."
        )

    if state_pair_us > args.budget_us:
        print("FAIL: even the short state-only pair exceeds the 32K wire budget")
        return 1

    print(
        "PASS: stable SCK can carry two short state frames inside 31.25us; "
        "ADC readiness and scheduler time remain separate tests"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, serial.SerialException) as exc:
        print("ERROR: {0}".format(exc), file=sys.stderr)
        sys.exit(2)
