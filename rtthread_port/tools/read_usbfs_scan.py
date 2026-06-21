#!/usr/bin/env python3
"""Read CH32H417 USBFS CDC scan reports."""

import argparse
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Read H417 USBFS CDC scan reports.")
    parser.add_argument("--port", default="COM5", help="CDC COM port, for example COM5")
    parser.add_argument("--baud", default=115200, type=int, help="Baud rate placeholder for CDC")
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("pyserial is not installed. Install it with: python -m pip install pyserial")
        return 2

    try:
        with serial.Serial(args.port, args.baud, timeout=1) as ser:
            print(f"reading {args.port} at {args.baud}; press Ctrl+C to stop")
            while True:
                data = ser.readline()
                if data:
                    print(data.decode("utf-8", errors="replace").rstrip())
    except KeyboardInterrupt:
        print("\nstopped")
        return 0
    except serial.SerialException as exc:
        print(f"serial error: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
