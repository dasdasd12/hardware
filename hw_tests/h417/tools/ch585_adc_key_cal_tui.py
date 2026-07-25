#!/usr/bin/env python
"""H417 USB CDC TUI for CH585 single-key ADC calibration."""

from __future__ import print_function

import argparse
import os
import re
import shutil
import sys
import time


ANSI_CLEAR = "\x1b[2J"
ANSI_HOME = "\x1b[H"
ANSI_CLEAR_TAIL = "\x1b[J"
ANSI_HIDE_CURSOR = "\x1b[?25l"
ANSI_SHOW_CURSOR = "\x1b[?25h"
ANSI_ALT_SCREEN = "\x1b[?1049h"
ANSI_MAIN_SCREEN = "\x1b[?1049l"

SAMPLE_RE = re.compile(
    r"CAL_SAMPLE\s+side=(?P<side>\S+)\s+key=(?P<key>\d+)\s+"
    r"seq=(?P<seq>\d+)\s+raw=(?P<raw>\d+)\s+"
    r"min=(?P<min>\d+)\s+max=(?P<max>\d+)\s+"
    r"count=(?P<count>\d+)\s+status=(?P<status>\d+)\s+"
    r"spi=(?P<spi>-?\d+)\s+diag=(?P<diag>0x[0-9A-Fa-f]+)"
)


class KeyStats(object):
    def __init__(self):
        self.raw = None
        self.min_raw = None
        self.max_raw = None
        self.count = 0
        self.seq = 0
        self.status = 0
        self.spi = 0
        self.diag = "0x00000000"
        self.updated_at = 0.0

    @property
    def span(self):
        if self.min_raw is None or self.max_raw is None:
            return None
        return self.max_raw - self.min_raw

    def reset(self):
        self.raw = None
        self.min_raw = None
        self.max_raw = None
        self.count = 0
        self.seq = 0
        self.status = 0
        self.spi = 0
        self.diag = "0x00000000"
        self.updated_at = 0.0


class CalState(object):
    def __init__(self, side="left", key_count=36):
        self.side = side
        self.key_count = key_count
        self.current_key = 0
        self.last_sample_key = None
        self.keys = [KeyStats() for _ in range(max(1, key_count))]
        self.last_line = ""
        self.last_sample_at = 0.0
        self.frames = 0
        self.running = True

    def ensure_key(self, key):
        while key >= len(self.keys):
            self.keys.append(KeyStats())
        if key + 1 > self.key_count:
            self.key_count = key + 1


def parse_line(state, line):
    line = line.strip("\r\n")
    state.last_line = line
    match = SAMPLE_RE.match(line)
    if not match:
        if line.startswith("CAL_CMD "):
            return True
        return False

    key = int(match.group("key"))
    state.ensure_key(key)
    stats = state.keys[key]
    raw = int(match.group("raw"))
    new_min = raw if stats.min_raw is None else min(stats.min_raw, raw)
    new_max = raw if stats.max_raw is None else max(stats.max_raw, raw)
    changed = (
        stats.min_raw is None or
        stats.max_raw is None or
        stats.min_raw != new_min or
        stats.max_raw != new_max
    )

    state.side = match.group("side")
    state.last_sample_key = key
    stats.raw = raw
    stats.min_raw = new_min
    stats.max_raw = new_max
    stats.count = int(match.group("count"))
    stats.seq = int(match.group("seq"))
    stats.status = int(match.group("status"))
    stats.spi = int(match.group("spi"))
    stats.diag = match.group("diag")
    stats.updated_at = time.time()
    state.last_sample_at = stats.updated_at
    state.frames += 1
    return changed


def build_key_command(key):
    return "key %d\n" % key


def build_reset_command():
    return "reset\n"


def build_start_command():
    return "start\n"


def build_stop_command():
    return "stop\n"


def value_or_dash(value, width):
    if value is None:
        return "-".rjust(width)
    return ("%0*d" % (width, value))[-width:]


def raw_bar(stats, width):
    if stats.raw is None:
        return "[" + ("." * width) + "]"
    value = max(0, min(1023, stats.raw))
    filled = int(((1023 - value) * width) / 1023)
    return "[" + ("#" * filled) + ("." * (width - filled)) + "]"


def key_line(index, stats, current, width):
    marker = ">" if current else " "
    span = "-" if stats.span is None else str(stats.span)
    line = (
        "%s K%02d raw=%s min=%s max=%s span=%4s n=%-7u %s"
        % (
            marker,
            index,
            value_or_dash(stats.raw, 4),
            value_or_dash(stats.min_raw, 4),
            value_or_dash(stats.max_raw, 4),
            span,
            stats.count,
            raw_bar(stats, 18),
        )
    )
    return line[:max(1, width - 1)]


def render_screen(state, width=96, max_rows=28):
    current_stats = state.keys[state.current_key]
    age = time.time() - state.last_sample_at if state.last_sample_at else 0.0
    span = "-" if current_stats.span is None else str(current_stats.span)
    sample_key = "?" if state.last_sample_key is None else str(state.last_sample_key)
    lines = [
        "CH585 ADC Key Cal  side=%s key=%u/%u sample=%s frames=%u age=%.1fs running=%u"
        % (
            state.side,
            state.current_key,
            max(0, state.key_count - 1),
            sample_key,
            state.frames,
            age,
            1 if state.running else 0,
        ),
        "current raw=%s min=%s max=%s span=%s seq=%u status=%u spi=%d diag=%s"
        % (
            value_or_dash(current_stats.raw, 4),
            value_or_dash(current_stats.min_raw, 4),
            value_or_dash(current_stats.max_raw, 4),
            span,
            current_stats.seq,
            current_stats.status,
            current_stats.spi,
            current_stats.diag,
        ),
        "keys: n next, p prev, r reset current, s start/stop, q quit",
        "",
    ]

    rows_left = max(0, max_rows - len(lines) - 2)
    if rows_left <= 0:
        rows_left = 1
    half = rows_left // 2
    start = max(0, state.current_key - half)
    end = min(state.key_count, start + rows_left)
    start = max(0, end - rows_left)

    for index in range(start, end):
        lines.append(key_line(index, state.keys[index], index == state.current_key, width))

    if end < state.key_count:
        lines.append("... %u more keys" % (state.key_count - end))
    if state.last_line:
        lines.append(("last: " + state.last_line)[:max(1, width - 1)])

    return "\n".join(line[:max(1, width - 1)] for line in lines[:max_rows])


def demo_lines():
    return [
        "CAL_SAMPLE side=left key=0 seq=1 raw=0490 min=0490 max=0490 count=1 status=0 spi=0 diag=0x000002c0",
        "CAL_SAMPLE side=left key=0 seq=2 raw=0480 min=0480 max=0490 count=2 status=0 spi=0 diag=0x000002c0",
        "CAL_SAMPLE side=left key=1 seq=3 raw=0856 min=0856 max=0856 count=1 status=0 spi=0 diag=0x000002c4",
        "CAL_SAMPLE side=left key=1 seq=4 raw=0353 min=0353 max=0856 count=2 status=0 spi=0 diag=0x000002c4",
    ]


def terminal_size():
    if hasattr(shutil, "get_terminal_size"):
        size = shutil.get_terminal_size((96, 28))
        return max(40, size.columns), max(12, size.lines)
    return 96, 28


def enable_windows_virtual_terminal(stream):
    if sys.platform != "win32" or not hasattr(stream, "isatty") or not stream.isatty():
        return False

    try:
        import ctypes
    except ImportError:
        return False

    kernel32 = ctypes.windll.kernel32
    handle = kernel32.GetStdHandle(ctypes.c_ulong(-11).value)
    if not handle or handle == ctypes.c_void_p(-1).value:
        return False

    mode = ctypes.c_uint()
    if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
        return False

    return bool(kernel32.SetConsoleMode(handle, mode.value | 0x0004))


def import_pyserial():
    try:
        import serial
    except ImportError:
        print("pyserial is not installed. Install it with: python -m pip install pyserial", file=sys.stderr)
        raise SystemExit(2)

    if not hasattr(serial, "Serial"):
        path = getattr(serial, "__file__", "unknown")
        print(
            "imported module 'serial' from %s, but it is not pyserial. "
            "Install pyserial and remove the conflicting serial package." % path,
            file=sys.stderr,
        )
        raise SystemExit(2)

    return serial


def read_console_key():
    if sys.platform == "win32":
        try:
            import msvcrt
        except ImportError:
            return None
        if msvcrt.kbhit():
            ch = msvcrt.getwch()
            if ch in ("\x00", "\xe0") and msvcrt.kbhit():
                _ = msvcrt.getwch()
                return None
            return ch
        return None

    try:
        import select
    except ImportError:
        return None
    readable, _, _ = select.select([sys.stdin], [], [], 0)
    if readable:
        return sys.stdin.read(1)
    return None


def send_text(ser, text):
    data = text.encode("ascii")
    ser.write(data)
    try:
        ser.flush()
    except AttributeError:
        pass


def handle_keypress(ch, state, ser):
    if not ch:
        return True

    ch = ch.lower()
    if ch == "q":
        return False
    if ch == "n":
        state.current_key = min(state.key_count - 1, state.current_key + 1)
        send_text(ser, build_key_command(state.current_key))
        return True
    if ch == "p":
        state.current_key = max(0, state.current_key - 1)
        send_text(ser, build_key_command(state.current_key))
        return True
    if ch == "r":
        state.keys[state.current_key].reset()
        send_text(ser, build_reset_command())
        return True
    if ch == "s":
        state.running = not state.running
        send_text(ser, build_start_command() if state.running else build_stop_command())
        return True
    return True


def run_tui(ser, state, refresh_hz):
    min_interval = 1.0 / max(1.0, refresh_hz)
    last_draw = 0.0
    dirty = True

    enable_windows_virtual_terminal(sys.stdout)
    sys.stdout.write(ANSI_ALT_SCREEN + ANSI_CLEAR + ANSI_HIDE_CURSOR)
    sys.stdout.flush()
    send_text(ser, build_key_command(state.current_key))
    try:
        while True:
            ch = read_console_key()
            if ch is not None:
                if not handle_keypress(ch, state, ser):
                    return
                dirty = True

            data = ser.readline()
            if data:
                if not isinstance(data, str):
                    data = data.decode("utf-8", "replace")
                if parse_line(state, data):
                    dirty = True

            now = time.time()
            if dirty and (now - last_draw) >= min_interval:
                width, height = terminal_size()
                sys.stdout.write(ANSI_HOME + render_screen(state, width, height) + ANSI_CLEAR_TAIL)
                sys.stdout.flush()
                dirty = False
                last_draw = now
    finally:
        sys.stdout.write(ANSI_SHOW_CURSOR + ANSI_MAIN_SCREEN)
        sys.stdout.flush()


def run_demo(side, key_count, refresh_hz):
    state = CalState(side=side, key_count=key_count)
    enable_windows_virtual_terminal(sys.stdout)
    sys.stdout.write(ANSI_ALT_SCREEN + ANSI_CLEAR + ANSI_HIDE_CURSOR)
    sys.stdout.flush()
    try:
        while True:
            dirty = False
            for line in demo_lines():
                dirty = parse_line(state, line) or dirty
                if dirty:
                    width, height = terminal_size()
                    sys.stdout.write(ANSI_HOME + render_screen(state, width, height) + ANSI_CLEAR_TAIL)
                    sys.stdout.flush()
                    dirty = False
                time.sleep(1.0 / max(1.0, refresh_hz))
    finally:
        sys.stdout.write(ANSI_SHOW_CURSOR + ANSI_MAIN_SCREEN)
        sys.stdout.flush()


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="H417 USB CDC TUI for CH585 per-key ADC calibration."
    )
    parser.add_argument("--port", default=None, help="H417 CDC serial port, for example COM8")
    parser.add_argument("--baud", default=115200, type=int, help="CDC baud placeholder")
    parser.add_argument("--side", default="left", choices=("left", "right"), help="Half being calibrated")
    parser.add_argument("--keys", default=None, type=int, help="Override key count")
    parser.add_argument("--key", default=0, type=int, help="Initial key index")
    parser.add_argument("--refresh-hz", default=20.0, type=float, help="Maximum redraw rate")
    parser.add_argument("--demo", action="store_true", help="Run without a serial port")
    args = parser.parse_args(argv)

    key_count = args.keys
    if key_count is None:
        key_count = 41 if args.side == "right" else 36
    state = CalState(side=args.side, key_count=key_count)
    state.current_key = max(0, min(key_count - 1, args.key))

    if args.demo:
        run_demo(args.side, key_count, args.refresh_hz)
        return 0

    if not args.port:
        print(
            "missing --port, for example: python hw_tests/h417/tools/ch585_adc_key_cal_tui.py --port COM8 --side left",
            file=sys.stderr,
        )
        return 2

    serial = import_pyserial()
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.02, write_timeout=0.2)
    except serial.SerialException as exc:
        print("serial error: %s" % exc, file=sys.stderr)
        return 1

    try:
        run_tui(ser, state, args.refresh_hz)
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stdout.write(ANSI_SHOW_CURSOR + ANSI_MAIN_SCREEN)
        sys.stdout.flush()
        raise SystemExit(0)
