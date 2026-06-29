#!/usr/bin/env python3
"""Fixed-position TUI for CH585 half_scan debug UART output."""

from __future__ import print_function

import argparse
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

START_RE = re.compile(
    r"half_scan\s+start\s+half=(?P<half>\d+)\s+sys=(?P<sys>\d+)\s+"
    r"keys=(?P<keys>\d+)\s+rf=(?P<rf>\d+)\s+uart_dbg=(?P<uart>\d+)"
)
STATUS_RE = re.compile(
    r"hs\s+half=(?P<half>\d+)\s+seq=(?P<seq>\d+)\s+scan=(?P<scan>\d+)\s+"
    r"raw_min=(?P<raw_key>\d+):(?P<raw>\d+)\s+"
    r"pos_max=(?P<pos_key>\d+):(?P<pos>\d+)\s+"
    r"down=(?P<down>[0-9A-Fa-f]+)\s+first=(?P<first>\d+)\s+"
    r"spi=(?P<spi>\d+)\s+abort=(?P<abort>\d+)\s+last=(?P<last>-?\d+)\s+"
    r"host=(?P<host>\d+)\s+cmd=(?P<cmd>\d+)\s+hseq=(?P<hseq>\d+)"
    r"(?:\s+rxcnt=(?P<rxcnt>\d+)\s+rx=(?P<rx>[0-9A-Fa-f]+)\s+"
    r"hcrc=(?P<hcrc>[0-9A-Fa-f]{4})/(?P<hcalc>[0-9A-Fa-f]{4}))?"
)
RF_RE = re.compile(
    r"half_scan\s+rf\s+tx\s+tick=(?P<tick>\d+)\s+done=(?P<done>\d+)\s+"
    r"reports=(?P<reports>\d+)\s+hseq=(?P<hseq>\d+)\s+flags=(?P<flags>\d+)"
)


class HalfScanState(object):
    def __init__(self):
        self.half = None
        self.sys_hz = None
        self.key_count = None
        self.rf_enabled = None
        self.uart_debug = None
        self.seq = None
        self.scan = None
        self.raw_min_key = None
        self.raw_min = None
        self.pos_max_key = None
        self.pos_max = None
        self.down_hex = "000000000000"
        self.first_down = 255
        self.spi_frames = 0
        self.spi_aborts = 0
        self.last_spi_result = None
        self.host_valid = 0
        self.cmd = None
        self.hseq = None
        self.host_rx_count = None
        self.host_rx_hex = ""
        self.host_crc = None
        self.host_calc_crc = None
        self.rf_tick = None
        self.rf_done = None
        self.rf_reports = None
        self.rf_hseq = None
        self.rf_flags = None
        self.last_line = ""
        self.last_update = 0.0
        self.frames_seen = 0

    def side_name(self):
        if self.half == 0:
            return "left"
        if self.half == 1:
            return "right"
        return "?"

    def down_bytes(self):
        text = self.down_hex
        if len(text) % 2 != 0:
            text = "0" + text
        values = []
        for index in range(0, len(text), 2):
            try:
                values.append(int(text[index:index + 2], 16))
            except ValueError:
                values.append(0)
        return values

    def down_keys(self):
        keys = []
        limit = self.key_count
        for byte_index, value in enumerate(self.down_bytes()):
            for bit in range(8):
                key = byte_index * 8 + bit
                if limit is not None and key >= limit:
                    continue
                if value & (1 << bit):
                    keys.append(key)
        return keys

    def down_count(self):
        return len(self.down_keys())


def parse_int(match, name):
    return int(match.group(name))


def parse_line(state, line):
    line = line.strip("\r\n")
    state.last_line = line

    match = START_RE.match(line)
    if match:
        state.half = parse_int(match, "half")
        state.sys_hz = parse_int(match, "sys")
        state.key_count = parse_int(match, "keys")
        state.rf_enabled = parse_int(match, "rf")
        state.uart_debug = parse_int(match, "uart")
        state.last_update = time.time()
        return True

    match = STATUS_RE.match(line)
    if match:
        state.half = parse_int(match, "half")
        state.seq = parse_int(match, "seq")
        state.scan = parse_int(match, "scan")
        state.raw_min_key = parse_int(match, "raw_key")
        state.raw_min = parse_int(match, "raw")
        state.pos_max_key = parse_int(match, "pos_key")
        state.pos_max = parse_int(match, "pos")
        state.down_hex = match.group("down").lower()
        state.first_down = parse_int(match, "first")
        state.spi_frames = parse_int(match, "spi")
        state.spi_aborts = parse_int(match, "abort")
        state.last_spi_result = parse_int(match, "last")
        state.host_valid = parse_int(match, "host")
        state.cmd = parse_int(match, "cmd")
        state.hseq = parse_int(match, "hseq")
        if match.group("rxcnt") is not None:
            state.host_rx_count = parse_int(match, "rxcnt")
            state.host_rx_hex = match.group("rx").lower()
            state.host_crc = int(match.group("hcrc"), 16)
            state.host_calc_crc = int(match.group("hcalc"), 16)
        state.frames_seen += 1
        state.last_update = time.time()
        return True

    match = RF_RE.match(line)
    if match:
        state.rf_tick = parse_int(match, "tick")
        state.rf_done = parse_int(match, "done")
        state.rf_reports = parse_int(match, "reports")
        state.rf_hseq = parse_int(match, "hseq")
        state.rf_flags = parse_int(match, "flags")
        state.last_update = time.time()
        return True

    return False


def value_or_unknown(value):
    return "?" if value is None else str(value)


def key_value(key, value):
    return "%s=%s" % (key, value_or_unknown(value))


def fit_line(line, width):
    if width is None or width <= 1:
        return line
    max_len = width - 1
    if len(line) <= max_len:
        return line
    return line[:max_len]


def terminal_size():
    if hasattr(shutil, "get_terminal_size"):
        size = shutil.get_terminal_size((100, 24))
        return max(40, size.columns), max(1, size.lines)
    return 100, 24


def down_bar(state, width):
    keys = set(state.down_keys())
    count = state.key_count or 0
    if count <= 0:
        return ""
    width = max(8, min(width, count))
    chars = []
    for index in range(width):
        key = int(index * count / width)
        chars.append("#" if key in keys else ".")
    return "[" + "".join(chars) + "]"


def is_wired_only_dummy_mosi(state):
    if state.rf_enabled == 0:
        return True
    if state.rf_enabled is not None:
        return False
    if state.cmd != 0 or state.hseq != 0:
        return False
    return bool(state.host_rx_hex) and state.host_rx_hex.strip("0") == ""


def diagnostic_lines(state):
    lines = []
    if state.seq is None:
        lines.append("waiting for hs status lines from CH585 UART...")
        return lines

    if state.spi_frames == 0:
        lines.append("diag: no completed SPI transaction yet; check H417 polling, CS, SCK, or side wiring.")
    elif state.host_valid == 0:
        if is_wired_only_dummy_mosi(state):
            lines.append("diag: wired-only mode; SPI MOSI host command is disabled and ignored.")
        elif state.host_rx_count == 0:
            lines.append("diag: SPI is clocking, but CH585 captured zero MOSI bytes.")
        elif state.host_rx_hex and not state.host_rx_hex.startswith("a601"):
            lines.append("diag: host rx header is %s; expected a601.... Check MOSI or SPI mode." %
                         state.host_rx_hex[:8])
        else:
            lines.append("diag: SPI is clocking, but host command CRC/magic is not valid on CH585.")
    elif state.down_count() == 0:
        lines.append("diag: SPI and host command are alive; no CH585 down_bits set yet.")
    else:
        lines.append("diag: CH585 reports down_bits; if PC has no key, inspect H417 merge/USB path.")

    if state.spi_aborts:
        lines.append("diag: SPI aborts=%s; CS may be dropping before the SPI frame completes." % state.spi_aborts)
    if state.last_spi_result not in (None, 0):
        lines.append("diag: last SPI result=%s; expected 0." % state.last_spi_result)
    return lines


def screen_lines(state, width=100, max_rows=24):
    age = "?"
    if state.last_update:
        age = "%.1fs" % (time.time() - state.last_update)

    host = "valid" if state.host_valid else "invalid"
    first = "-" if state.first_down == 255 else "K%02d" % state.first_down
    down_keys = state.down_keys()

    lines = [
        "CH585 Half Scan Debug  side=%s half=%s age=%s frames=%s" %
        (state.side_name(), value_or_unknown(state.half), age, state.frames_seen),
        "%s %s %s %s" %
        (
            key_value("sys", state.sys_hz),
            key_value("keys", state.key_count),
            key_value("rf", state.rf_enabled),
            key_value("uart_dbg", state.uart_debug),
        ),
        "seq=%s scan=%s host=%s cmd=%s hseq=%s" %
        (
            value_or_unknown(state.seq),
            value_or_unknown(state.scan),
            host,
            value_or_unknown(state.cmd),
            value_or_unknown(state.hseq),
        ),
        "raw_min=K%02d:%04d pos_max=K%02d:%04d first=%s" %
        (
            state.raw_min_key if state.raw_min_key is not None else 0,
            state.raw_min if state.raw_min is not None else 0,
            state.pos_max_key if state.pos_max_key is not None else 0,
            state.pos_max if state.pos_max is not None else 0,
            first,
        ),
        "down=%s count=%d keys=%s" % (state.down_hex, len(down_keys), down_keys[:24]),
        down_bar(state, max(8, width - 2)),
        "spi frames=%s aborts=%s last=%s" %
        (
            value_or_unknown(state.spi_frames),
            value_or_unknown(state.spi_aborts),
            value_or_unknown(state.last_spi_result),
        ),
        "host_rx count=%s head=%s hcrc=%s/%s" %
        (
            value_or_unknown(state.host_rx_count),
            state.host_rx_hex or "?",
            "%04x" % state.host_crc if state.host_crc is not None else "?",
            "%04x" % state.host_calc_crc if state.host_calc_crc is not None else "?",
        ),
        "rf tick=%s done=%s reports=%s hseq=%s flags=%s" %
        (
            value_or_unknown(state.rf_tick),
            value_or_unknown(state.rf_done),
            value_or_unknown(state.rf_reports),
            value_or_unknown(state.rf_hseq),
            value_or_unknown(state.rf_flags),
        ),
        "",
    ]
    lines.extend(diagnostic_lines(state))
    lines.append("")
    lines.append("last: %s" % state.last_line[:120])

    bounded = [fit_line(line, width) for line in lines]
    if max_rows is not None and len(bounded) > max_rows:
        bounded = bounded[:max_rows]
        bounded[-1] = fit_line("... terminal too short; increase window height", width)
    return bounded


def render_screen(state, width=100, max_rows=24):
    return ANSI_HOME + "\n".join(screen_lines(state, width, max_rows)) + ANSI_CLEAR_TAIL


def enable_windows_virtual_terminal(stream):
    if sys.platform != "win32" or not hasattr(stream, "isatty") or not stream.isatty():
        return False
    try:
        import ctypes
    except ImportError:
        return False

    kernel32 = ctypes.windll.kernel32
    handle = kernel32.GetStdHandle(-11)
    if not handle:
        return False
    mode = ctypes.c_uint()
    if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
        return False
    return bool(kernel32.SetConsoleMode(handle, mode.value | 0x0004))


def load_pyserial_api():
    try:
        import serial
    except ImportError:
        print("pyserial is not installed. Install it with: python -m pip install pyserial", file=sys.stderr)
        raise SystemExit(2)

    serial_cls = getattr(serial, "Serial", None)
    serial_exception = getattr(serial, "SerialException", None)
    if serial_cls is not None and serial_exception is not None:
        return serial_cls, serial_exception

    serial_path = getattr(serial, "__file__", "<unknown>")
    try:
        from serial import serialutil as pyserial_util
        for name in dir(pyserial_util):
            if not name.startswith("_"):
                setattr(serial, name, getattr(pyserial_util, name))

        if sys.platform == "win32":
            from serial.serialwin32 import Serial as serial_cls
        else:
            from serial.serialposix import Serial as serial_cls
        serial_exception = pyserial_util.SerialException
        serial.Serial = serial_cls
        serial.SerialException = serial_exception
        return serial_cls, serial_exception
    except (ImportError, AttributeError):
        pass

    print(
        "imported a wrong 'serial' package, not pyserial: %s\n"
        "fix this Python environment with:\n"
        "  python -m pip uninstall serial\n"
        "  python -m pip install --force-reinstall pyserial" % serial_path,
        file=sys.stderr,
    )
    raise SystemExit(2)


def iter_serial_lines(port, baud):
    serial_cls, serial_exception = load_pyserial_api()

    try:
        ser = serial_cls(port, baud, timeout=0.2)
    except serial_exception as exc:
        print("serial error: %s" % exc, file=sys.stderr)
        raise SystemExit(1)

    try:
        while True:
            data = ser.readline()
            if not data:
                yield None
                continue
            if not isinstance(data, str):
                data = data.decode("utf-8", "replace")
            yield data.rstrip("\r\n")
    finally:
        ser.close()


def demo_lines_once():
    return [
        "half_scan start half=0 sys=78000000 keys=36 rf=0 uart_dbg=1",
        "hs half=0 seq=100 scan=331 raw_min=7:490 pos_max=0:0 down=000000000000 first=255 spi=0 abort=0 last=0 host=0 cmd=0 hseq=0 rxcnt=12 rx=00000000 hcrc=0000/84c0",
        "hs half=0 seq=200 scan=680 raw_min=5:330 pos_max=5:1000 down=200000000000 first=5 spi=87 abort=0 last=0 host=0 cmd=0 hseq=0 rxcnt=12 rx=00000000 hcrc=0000/84c0",
    ]


def demo_iter(delay):
    while True:
        for line in demo_lines_once():
            yield line
            time.sleep(delay)


def run_tui(line_iter, refresh_hz):
    state = HalfScanState()
    min_interval = 1.0 / max(1.0, refresh_hz)
    last_draw = 0.0

    enable_windows_virtual_terminal(sys.stdout)
    sys.stdout.write(ANSI_ALT_SCREEN + ANSI_CLEAR + ANSI_HIDE_CURSOR)
    sys.stdout.flush()
    try:
        for line in line_iter:
            changed = False
            if line is not None:
                changed = parse_line(state, line)
            now = time.time()
            if changed or (now - last_draw) >= min_interval:
                width, height = terminal_size()
                sys.stdout.write(render_screen(state, width, height))
                sys.stdout.flush()
                last_draw = now
    finally:
        sys.stdout.write(ANSI_SHOW_CURSOR + ANSI_MAIN_SCREEN)
        sys.stdout.flush()


def run_raw(line_iter):
    state = HalfScanState()
    for line in line_iter:
        if line is None:
            continue
        if parse_line(state, line):
            sys.stdout.write(" | ".join(screen_lines(state, 160, None)[0:9]) + "\n")
            sys.stdout.flush()


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Fixed-position TUI for CH585 half_scan debug UART output."
    )
    parser.add_argument("--port", default=None, help="Serial port, for example COM7")
    parser.add_argument("--baud", default=921600, type=int, help="UART baud rate")
    parser.add_argument("--refresh-hz", default=10.0, type=float, help="Maximum redraw rate")
    parser.add_argument("--demo", action="store_true", help="Run without serial input")
    parser.add_argument("--demo-delay", default=0.5, type=float, help="Delay per demo line")
    parser.add_argument("--raw", action="store_true", help="Print one-line summaries instead of TUI")
    args = parser.parse_args(argv)

    if args.demo:
        lines = demo_iter(args.demo_delay)
    else:
        if not args.port:
            print("missing --port, for example: python firmware/ch585/tools/ch585_half_scan_debug_tui.py --port COM7")
            return 2
        lines = iter_serial_lines(args.port, args.baud)

    if args.raw:
        run_raw(lines)
    else:
        run_tui(lines, args.refresh_hz)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stdout.write(ANSI_SHOW_CURSOR + ANSI_MAIN_SCREEN)
        sys.stdout.flush()
        raise SystemExit(0)
