from __future__ import print_function

import argparse
import sys
import threading
import time

import serial


LOCAL_COMMAND_GAP_S = 0.2
LOCAL_FAST_COMMAND_GAP_S = 0.02


def _serial_waiting(ser):
    if hasattr(ser, "in_waiting"):
        return ser.in_waiting
    return ser.inWaiting()


def _to_text(data):
    try:
        return data.decode("utf-8", "replace")
    except AttributeError:
        return data


def _read_input(prompt):
    try:
        return raw_input(prompt)
    except NameError:
        return input(prompt)


def _write_command(ser, line):
    ser.write((line + "\r\n").encode("utf-8"))
    ser.flush()


def _write_command_sequence(ser, command_text, gap_s=LOCAL_COMMAND_GAP_S):
    commands = [item.strip() for item in command_text.split(";") if item.strip()]
    if not commands:
        return

    for index, command in enumerate(commands):
        print("Sending: {0}".format(command))
        _write_command(ser, command)
        if index != len(commands) - 1:
            time.sleep(gap_s)


def _handle_local_command(ser, line):
    stripped = line.strip()
    lower = stripped.lower()

    if lower in set(["localhelp", "?"]):
        print("Local commands:")
        print("  after <seconds> <board-command>")
        print("  afterfast <seconds> <board-command>")
        print("  example: after 3 tap b")
        print("  example: after 3 tap b; tap 1; tap enter")
        print("  example: afterfast 3 tap b; tap 1; tap enter")
        print("  example: after 3 report 02 00 04 00 00 00 00 00; release")
        return True

    if lower.startswith("after ") or lower.startswith("afterfast "):
        is_fast = lower.startswith("afterfast ")
        parts = stripped.split(None, 2)
        if len(parts) != 3:
            print("Usage: after <seconds> <board-command>, example: after 3 tap b")
            print("Usage: afterfast <seconds> <board-command>, example: afterfast 3 tap b; tap 1")
            return True

        try:
            delay_s = float(parts[1])
        except ValueError:
            print("Invalid delay: {0}".format(parts[1]))
            return True

        if delay_s < 0:
            print("Delay must be >= 0")
            return True

        command = parts[2]
        print("Will send after {0:.1f}s: {1}".format(delay_s, command))
        print("Focus Notepad or another text box now; serial logs will still print here.")

        end_time = time.time() + delay_s
        while True:
            remaining = end_time - time.time()
            if remaining <= 0:
                break
            sleep_s = 1.0 if remaining > 1.0 else remaining
            time.sleep(sleep_s)

        _write_command_sequence(
            ser,
            command,
            LOCAL_FAST_COMMAND_GAP_S if is_fast else LOCAL_COMMAND_GAP_S,
        )
        return True

    return False


def reader_loop(ser, stop_event):
    while not stop_event.is_set():
        try:
            data = ser.read(_serial_waiting(ser) or 1)
        except serial.SerialException as exc:
            print("\n[reader] serial error: {0}".format(exc))
            stop_event.set()
            return

        if data:
            sys.stdout.write(_to_text(data))
            sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(description="CH585M USB CDC debug console")
    parser.add_argument("port", help="Serial port, e.g. COM5")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate placeholder for CDC ACM")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    stop_event = threading.Event()
    reader = threading.Thread(target=reader_loop, args=(ser, stop_event))
    reader.daemon = True
    reader.start()

    print("Opened {0}. Try: help, status, tap a, combo ctrl c, type hello, kvm switch 2".format(args.port))
    print("Local helper: after 3 tap b  (then focus Notepad before it sends)")
    print("Type quit or exit to close.")

    try:
        while not stop_event.is_set():
            try:
                line = _read_input("> ")
            except EOFError:
                break

            if line.strip().lower() in set(["quit", "exit"]):
                break

            if _handle_local_command(ser, line):
                continue

            _write_command(ser, line)
            time.sleep(0.02)
    finally:
        stop_event.set()
        reader.join(0.5)
        ser.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
