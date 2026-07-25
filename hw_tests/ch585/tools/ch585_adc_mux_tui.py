#!/usr/bin/env python
"""Fixed-position TUI for the CH585 ADC/MUX serial hw_test output."""

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

FRAME_RE = re.compile(
    r"FRAME\s+side=(?P<side>\S+)\s+seq=(?P<seq>\d+)\s+"
    r"scale=(?P<scale>\d+)\s+keys=(?P<keys>\d+)\s+"
    r"sampled=(?P<sampled>\d+)\s+flags=(?P<flags>0x[0-9A-Fa-f]+)"
    r"\s+down=(?P<down>0x[0-9A-Fa-f]+)"
)
DATA_RE = re.compile(r"DATA\s+(?P<key>[^=]+)=(?P<value>.*)$")
LANE_RE = re.compile(
    r"\s*(?P<label>L\d+)\s+(?P<cs>CS\d'?)\((?P<pin>[^)]+)\)\s+"
    r"CH(?P<channel>\d+)\s+mux=(?P<mux>\d+)"
)
SAMPLE_RE = re.compile(
    r"\s*D(?P<d>\d+)\s+raw=(?P<raw>\d+)\s+"
    r"travel=(?P<travel>[0-9.]+%)\s+(?P<bar>\[[#.]+\])"
)


class Sample(object):
    def __init__(self, index, raw, travel, bar):
        self.index = index
        self.raw = raw
        self.travel = travel
        self.bar = bar


class Lane(object):
    def __init__(self, label, cs, pin, channel, mux_count):
        self.label = label
        self.cs = cs
        self.pin = pin
        self.channel = channel
        self.mux_count = mux_count
        self.samples = []


class TuiState(object):
    def __init__(self):
        self.side = "?"
        self.seq = None
        self.scale = 1024
        self.keys = 0
        self.sampled = 0
        self.flags = "0x00"
        self.down = "0x0"
        self.profile = ""
        self.mux_counts = ""
        self.last_line = ""
        self.last_update = 0.0
        self.lanes = []
        self.current_lane = None


def parse_line(state, line):
    line = line.strip("\r\n")
    state.last_line = line

    match = DATA_RE.match(line)
    if match:
        key = match.group("key")
        value = match.group("value")
        if key == "adc_mux_profile":
            state.profile = value
        elif key == "mux_counts":
            state.mux_counts = value
        return False

    match = FRAME_RE.match(line)
    if match:
        state.side = match.group("side")
        state.seq = int(match.group("seq"))
        state.scale = int(match.group("scale"))
        state.keys = int(match.group("keys"))
        state.sampled = int(match.group("sampled"))
        state.flags = match.group("flags")
        state.down = match.group("down")
        state.lanes = []
        state.current_lane = None
        state.last_update = time.time()
        return True

    match = LANE_RE.match(line)
    if match:
        lane = Lane(
            match.group("label"),
            match.group("cs"),
            match.group("pin"),
            int(match.group("channel")),
            int(match.group("mux")),
        )
        state.lanes.append(lane)
        state.current_lane = lane
        state.last_update = time.time()
        return True

    match = SAMPLE_RE.match(line)
    if match and state.current_lane is not None:
        state.current_lane.samples.append(
            Sample(
                int(match.group("d")),
                int(match.group("raw")),
                match.group("travel"),
                match.group("bar"),
            )
        )
        state.last_update = time.time()
        return True

    return False


def format_sample(sample):
    return "  D%02d raw=%04d travel=%s %s" % (
        sample.index,
        sample.raw,
        sample.travel,
        sample.bar,
    )


def fit_line(line, width):
    if width is None or width <= 1:
        return line

    max_len = width - 1
    if len(line) <= max_len:
        return line
    return line[:max_len]


def value_text(value, width):
    text = str(value)
    if len(text) > width:
        return text[:width]
    return text.ljust(width)


def cursor_to(row, col):
    return "\x1b[%d;%dH" % (row, col)


def terminal_size():
    if hasattr(shutil, "get_terminal_size"):
        size = shutil.get_terminal_size((80, 24))
        return max(20, size.columns), max(1, size.lines)
    return 80, 24


def header_lines(state):
    seq = "?" if state.seq is None else str(state.seq)
    lines = []
    lines.append(
        "CH585 ADC MUX TUI  side=%s seq=%s scale=%s"
        % (
            value_text(state.side, 5),
            value_text(seq, 6),
            value_text(state.scale, 4),
        )
    )
    lines.append(
        "keys=%s sampled=%s flags=%s down=%s" %
        (
            value_text(state.keys, 3),
            value_text(state.sampled, 3),
            value_text(state.flags, 6),
            value_text(state.down, 18),
        )
    )
    lines.append(
        "profile=%s mux_counts=%s Ctrl+C exits; values are raw/1024." %
        (
            value_text(state.profile or state.side, 8),
            value_text(state.mux_counts, 15),
        )
    )
    lines.append("")
    return lines


def append_last_line_if_room(lines, state, width, max_rows):
    if not state.last_line:
        return
    if max_rows is not None and len(lines) >= max_rows:
        return
    lines.append("last: %s" % state.last_line[:100])


def vertical_screen_lines(state, width, max_rows):
    lines = header_lines(state)
    if not state.lanes:
        lines.append("Waiting for FRAME/Lane/Dxx serial lines...")
    else:
        for lane in state.lanes:
            lines.append(
                "%s %s(%s) CH%d mux=%d" %
                (lane.label, lane.cs, lane.pin, lane.channel, lane.mux_count)
            )
            if lane.samples:
                for sample in lane.samples:
                    lines.append(format_sample(sample))
            else:
                lines.append("  waiting for samples")
            lines.append("")

    append_last_line_if_room(lines, state, width, max_rows)
    return lines


def compact_sample_cell(sample, bar_width):
    return "D%02d %s" % (
        sample.index,
        compact_sample_dynamic_text(sample, bar_width),
    )


def compact_sample_dynamic_text(sample, bar_width):
    return "%04d %6s %s" % (
        sample.raw,
        sample.travel,
        raw_to_bar(sample.raw, bar_width),
    )


def compact_join(cells, col_width, width):
    fitted = [fit_line(cell, col_width).ljust(col_width) for cell in cells]
    return fit_line(" ".join(fitted).rstrip(), width)


def compact_layout(width, lane_count):
    cols = max(1, min(lane_count, max(1, width // 28)))
    col_width = max(20, (width - (cols - 1)) // cols)
    bar_width = max(4, min(12, col_width - 18))
    return cols, col_width, bar_width


def compact_screen_lines(state, width, max_rows):
    lines = header_lines(state)
    if not state.lanes:
        lines.append("Waiting for FRAME/Lane/Dxx serial lines...")
        append_last_line_if_room(lines, state, width, max_rows)
        return lines

    cols, col_width, bar_width = compact_layout(width, len(state.lanes))

    for start in range(0, len(state.lanes), cols):
        group = state.lanes[start:start + cols]
        lines.append(
            compact_join(
                [
                    "%s %s(%s) CH%d mux=%d" %
                    (lane.label, lane.cs, lane.pin, lane.channel, lane.mux_count)
                    for lane in group
                ],
                col_width,
                width,
            )
        )
        max_samples = max([len(lane.samples) for lane in group] + [0])
        if max_samples == 0:
            lines.append(compact_join(["waiting for samples" for lane in group], col_width, width))
            continue
        for index in range(max_samples):
            cells = []
            for lane in group:
                if index < len(lane.samples):
                    cells.append(compact_sample_cell(lane.samples[index], bar_width))
                else:
                    cells.append("")
            lines.append(compact_join(cells, col_width, width))

    append_last_line_if_room(lines, state, width, max_rows)
    return lines


def bound_screen_lines(lines, width, max_rows):
    bounded = [fit_line(line, width) for line in lines]
    if max_rows is not None and len(bounded) > max_rows:
        bounded = bounded[:max_rows]
        bounded[-1] = fit_line("... terminal too short; increase window height", width)
    return bounded


def screen_lines_and_mode(state, width=80, max_rows=24):
    lines = vertical_screen_lines(state, width, max_rows)
    mode = "vertical"
    if max_rows is not None and len(lines) > max_rows:
        lines = compact_screen_lines(state, width, max_rows)
        mode = "compact"
    lines = bound_screen_lines(lines, width, max_rows)
    return lines, mode


def render_screen(state, width=80, max_rows=24):
    lines, _mode = screen_lines_and_mode(state, width, max_rows)

    return ANSI_HOME + "\n".join(lines) + ANSI_CLEAR_TAIL


def add_marker_cell(cells, key, row, line, marker, text, width):
    try:
        col = line.index(marker) + len(marker) + 1
    except ValueError:
        return
    add_cell(cells, key, row, col, text, width)


def add_cell(cells, key, row, col, text, width):
    if row < 1 or col < 1:
        return
    if width is not None:
        available = max(0, width - col)
        if available <= 0:
            return
        text = text[:available]
    cells.append((key, row, col, text))


def header_dynamic_cells(state, width):
    seq = "?" if state.seq is None else str(state.seq)
    lines = header_lines(state)
    cells = []
    add_marker_cell(cells, ("header", "side"), 1, lines[0], "side=", value_text(state.side, 5), width)
    add_marker_cell(cells, ("header", "seq"), 1, lines[0], "seq=", value_text(seq, 6), width)
    add_marker_cell(cells, ("header", "scale"), 1, lines[0], "scale=", value_text(state.scale, 4), width)
    add_marker_cell(cells, ("header", "keys"), 2, lines[1], "keys=", value_text(state.keys, 3), width)
    add_marker_cell(cells, ("header", "sampled"), 2, lines[1], "sampled=", value_text(state.sampled, 3), width)
    add_marker_cell(cells, ("header", "flags"), 2, lines[1], "flags=", value_text(state.flags, 6), width)
    add_marker_cell(cells, ("header", "down"), 2, lines[1], "down=", value_text(state.down, 18), width)
    add_marker_cell(cells, ("header", "profile"), 3, lines[2], "profile=", value_text(state.profile or state.side, 8), width)
    add_marker_cell(cells, ("header", "mux_counts"), 3, lines[2], "mux_counts=", value_text(state.mux_counts, 15), width)
    return cells


def vertical_dynamic_cells(state, width):
    cells = header_dynamic_cells(state, width)
    row = len(header_lines(state)) + 1
    for lane in state.lanes:
        row += 1
        for sample in lane.samples:
            line = format_sample(sample)
            add_marker_cell(
                cells,
                ("sample", lane.label, sample.index, "raw"),
                row,
                line,
                "raw=",
                "%04d" % sample.raw,
                width,
            )
            add_marker_cell(
                cells,
                ("sample", lane.label, sample.index, "travel"),
                row,
                line,
                "travel=",
                "%6s" % sample.travel,
                width,
            )
            bar_col = line.rfind(sample.bar) + 1
            add_cell(cells, ("sample", lane.label, sample.index, "bar"), row, bar_col, sample.bar, width)
            row += 1
        if not lane.samples:
            row += 1
        row += 1
    return cells


def compact_dynamic_cells(state, width):
    cells = header_dynamic_cells(state, width)
    if not state.lanes:
        return cells

    cols, col_width, bar_width = compact_layout(width, len(state.lanes))
    row = len(header_lines(state)) + 1
    for start in range(0, len(state.lanes), cols):
        group = state.lanes[start:start + cols]
        row += 1
        max_samples = max([len(lane.samples) for lane in group] + [0])
        for index in range(max_samples):
            for lane_index, lane in enumerate(group):
                if index >= len(lane.samples):
                    continue
                sample = lane.samples[index]
                col = 1 + lane_index * (col_width + 1) + len("D%02d " % sample.index)
                add_cell(
                    cells,
                    ("sample", lane.label, sample.index, "value"),
                    row,
                    col,
                    compact_sample_dynamic_text(sample, bar_width),
                    width,
                )
            row += 1
    return cells


def dynamic_cells(state, width=80, max_rows=24):
    lines, mode = screen_lines_and_mode(state, width, max_rows)
    if mode == "compact":
        cells = compact_dynamic_cells(state, width)
    else:
        cells = vertical_dynamic_cells(state, width)
    max_row = len(lines)
    return [
        (key, row, col, text)
        for key, row, col, text in cells
        if row <= max_row
    ]


def layout_signature(state, width=80, max_rows=24):
    lines, mode = screen_lines_and_mode(state, width, max_rows)
    masked = [list(line) for line in lines]
    for _key, row, col, text in dynamic_cells(state, width, max_rows):
        line_index = row - 1
        start = col - 1
        if line_index < 0 or line_index >= len(masked):
            continue
        for offset in range(len(text)):
            index = start + offset
            if 0 <= index < len(masked[line_index]):
                masked[line_index][index] = " "
    return (width, max_rows, mode, tuple("".join(line) for line in masked))


class DashboardRenderer(object):
    def __init__(self):
        self.signature = None
        self.cells = {}

    def render(self, state, width=80, max_rows=24):
        signature = layout_signature(state, width, max_rows)
        current_cells = {}
        ordered_cells = dynamic_cells(state, width, max_rows)
        for key, row, col, text in ordered_cells:
            current_cells[key] = (row, col, text)

        if self.signature != signature:
            self.signature = signature
            self.cells = current_cells
            return render_screen(state, width, max_rows)

        chunks = []
        for key, row, col, text in ordered_cells:
            if self.cells.get(key) != (row, col, text):
                chunks.append(cursor_to(row, col) + text)
        self.cells = current_cells
        return "".join(chunks)


def raw_to_bar(raw, width):
    value = max(0, min(1023, raw))
    filled = int(((value * width) + 1023) / 1024)
    return "[" + ("#" * filled) + ("." * (width - filled)) + "]"


def demo_lines():
    seq = 1
    lines = [
        "DATA adc_mux_profile=left",
        "DATA mux_counts=9,9,9,9",
    ]
    for frame in range(2):
        lines.append(
            "FRAME side=left seq=%d scale=1024 keys=36 sampled=36 flags=0x02 down=0x0000000000000000"
            % seq
        )
        for lane in range(4):
            cs = "CS1(PB14)" if lane in (0, 2) else "CS2(PB15)"
            channel = 0 if lane < 2 else 1
            lines.append(" L%d %s CH%d mux=9" % (lane + 1, cs, channel))
            for d in range(1, 10):
                raw = (frame * 97 + lane * 173 + d * 41) % 1024
                travel_x10 = int((raw * 1000) / 1024)
                lines.append(
                    "  D%02d raw=%04d travel=%d.%d%% %s" %
                    (d, raw, travel_x10 // 10, travel_x10 % 10, raw_to_bar(raw, 16))
                )
        seq += 1
    return lines


def iter_serial_lines(port, baud):
    try:
        import serial
    except ImportError:
        print("pyserial is not installed. Install it with: python -m pip install pyserial", file=sys.stderr)
        raise SystemExit(2)

    try:
        ser = serial.Serial(port, baud, timeout=0.2)
    except serial.SerialException as exc:
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


def frame_complete(state):
    if state.seq is None or state.sampled <= 0:
        return False
    return sum(len(lane.samples) for lane in state.lanes) >= state.sampled


def enable_windows_virtual_terminal(stream):
    if sys.platform != "win32" or not hasattr(stream, "isatty") or not stream.isatty():
        return False

    try:
        import ctypes
    except ImportError:
        return False

    kernel32 = ctypes.windll.kernel32
    kernel32.GetStdHandle.argtypes = [ctypes.c_ulong]
    kernel32.GetStdHandle.restype = ctypes.c_void_p

    handle = kernel32.GetStdHandle(ctypes.c_ulong(-11).value)
    if not handle or handle == ctypes.c_void_p(-1).value:
        return False

    mode = ctypes.c_uint()
    if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
        return False

    enable_virtual_terminal_processing = 0x0004
    new_mode = mode.value | enable_virtual_terminal_processing
    if new_mode == mode.value:
        return True

    return bool(kernel32.SetConsoleMode(handle, new_mode))


def run_tui(line_iter, refresh_hz):
    state = TuiState()
    renderer = DashboardRenderer()
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
            should_draw = False
            if changed and frame_complete(state):
                should_draw = True
            elif line is None and (state.seq is None or frame_complete(state)):
                should_draw = (now - last_draw) >= min_interval

            if should_draw:
                width, height = terminal_size()
                update = renderer.render(state, width, height)
                if update:
                    sys.stdout.write(update)
                    sys.stdout.flush()
                    last_draw = now
    finally:
        sys.stdout.write(ANSI_SHOW_CURSOR + ANSI_MAIN_SCREEN)
        sys.stdout.flush()


def demo_iter(delay):
    while True:
        for line in demo_lines():
            yield line
            time.sleep(delay)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Fixed-position TUI for CH585 ADC/MUX UART output."
    )
    parser.add_argument("--port", default=None, help="Serial port, for example COM7")
    parser.add_argument("--baud", default=115200, type=int, help="UART baud rate")
    parser.add_argument("--refresh-hz", default=10.0, type=float, help="Maximum redraw rate")
    parser.add_argument("--demo", action="store_true", help="Run without serial input")
    parser.add_argument("--demo-delay", default=0.02, type=float, help="Delay per demo line")
    args = parser.parse_args(argv)

    if args.demo:
        run_tui(demo_iter(args.demo_delay), args.refresh_hz)
        return 0

    if not args.port:
        print("missing --port, for example: python hw_tests/ch585/tools/ch585_adc_mux_tui.py --port COM7")
        return 2

    run_tui(iter_serial_lines(args.port, args.baud), args.refresh_hz)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stdout.write(ANSI_SHOW_CURSOR + ANSI_MAIN_SCREEN)
        sys.stdout.flush()
        raise SystemExit(0)
