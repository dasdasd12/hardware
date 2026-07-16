#!/usr/bin/env python3
"""Read ACQ32_RESULT lines and enforce the 31.25us acquisition budget."""

from __future__ import print_function

import argparse
import re
import sys
import time

import serial


FIELD_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)")


def parse_fields(line):
    return dict(FIELD_RE.findall(line))


def integer(data, name):
    return int(data[name], 0)


def main():
    parser = argparse.ArgumentParser(
        description="Monitor the CH585 fixed-pipeline 32K acquisition test"
    )
    parser.add_argument("--port", required=True, help="CH585 UART COM port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--windows", type=int, default=3)
    parser.add_argument("--min-rate", type=int, default=31900)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()

    deadline = time.monotonic() + args.timeout
    results = []

    with serial.Serial(args.port, args.baud, timeout=0.5) as port:
        while len(results) < args.windows and time.monotonic() < deadline:
            raw = port.readline()
            if not raw:
                continue
            line = raw.decode("ascii", "replace").strip()
            print("device:", line)
            if line.startswith("ACQ32_RESULT "):
                results.append(parse_fields(line))

    if len(results) < args.windows:
        raise RuntimeError(
            "received {0}/{1} result windows".format(len(results), args.windows)
        )

    total_overruns = sum(integer(item, "overruns") for item in results)
    worst_cycles = max(integer(item, "max_cycles") for item in results)
    budget_cycles = min(integer(item, "budget_cycles") for item in results)
    minimum_rate = min(integer(item, "effective_hz") for item in results)
    worst_average = max(integer(item, "avg_cycles") for item in results)
    half = results[-1].get("half", "unknown")

    print(
        "ACQ32_SUMMARY half={0} windows={1} total_overruns={2} "
        "worst_cycles={3} worst_avg_cycles={4} budget_cycles={5} "
        "minimum_effective_hz={6}".format(
            half,
            len(results),
            total_overruns,
            worst_cycles,
            worst_average,
            budget_cycles,
            minimum_rate,
        )
    )

    if total_overruns != 0 or worst_cycles > budget_cycles:
        print("FAIL: acquisition exceeded the 31.25us cycle budget")
        return 1
    if minimum_rate < args.min_rate:
        print("FAIL: effective full-frame acquisition rate is below requirement")
        return 1
    print("PASS: every measured full-half frame met the 32K acquisition budget")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyError, OSError, RuntimeError, ValueError, serial.SerialException) as exc:
        print("ERROR: {0}".format(exc), file=sys.stderr)
        sys.exit(2)
