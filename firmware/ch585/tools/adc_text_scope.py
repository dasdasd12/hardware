from __future__ import print_function

import argparse
import re
import sys
import time

import serial


AP_RE = re.compile(
    r"^AP\s+"
    r"(?P<seq>\d+)\s+"
    r"(?P<hall>\d+)\s+"
    r"(?P<key>\d+)\s+"
    r"(?P<lane>\d+)\s+"
    r"(?P<mux>\d+)\s+"
    r"(?P<d>\d+)\s+"
    r"(?P<label>\S+)\s+"
    r"(?P<raw>\d+)\s+"
    r"(?P<filt>\d+)\s+"
    r"(?P<pos>\d+)\s+"
    r"(?P<word>0x[0-9a-fA-F]+)\s+"
    r"(?P<down>\d+)\s+"
    r"(?P<rt>\d+)\s+"
    r"(?P<status>-?\d+)"
)


RIGHT_LABELS = {
    0: "F12", 1: "F11", 2: "F10", 3: "F9", 4: "F8", 5: "F7", 6: "F6",
    7: "Backspace", 8: "Equal", 9: "Minus",
    16: "0", 17: "9", 18: "8", 19: "7", 20: "Backslash",
    21: "RBracket", 22: "LBracket", 23: "P", 24: "O", 25: "I",
    32: "U", 33: "Y", 34: "Enter", 35: "Quote", 36: "Semicolon",
    37: "L", 38: "K", 39: "J", 40: "H", 41: "Shift",
    48: "Slash", 49: "Dot", 50: "Comma", 51: "M", 52: "N", 53: "B",
    54: "Ctrl", 55: "Win", 56: "Fn", 57: "Alt", 58: "Space",
}


LEFT_LABELS = {
    0: "F5", 1: "F4", 2: "F3", 3: "F2", 4: "F1", 5: "Esc",
    6: "6", 7: "5", 8: "4",
    16: "3", 17: "2", 18: "1", 19: "Grave", 20: "Y",
    21: "T", 22: "R", 23: "E", 24: "W",
    32: "Q", 33: "Tab", 34: "G", 35: "F", 36: "D",
    37: "S", 38: "A", 39: "Caps", 40: "B",
    48: "V", 49: "C", 50: "X", 51: "Z", 52: "Shift",
    53: "Space", 54: "Alt", 55: "Win", 56: "Ctrl",
}


def serial_waiting(ser):
    if hasattr(ser, "in_waiting"):
        return ser.in_waiting
    return ser.inWaiting()


def to_text(data):
    try:
        return data.decode("utf-8", "replace")
    except AttributeError:
        return data


def clamp(value, low, high):
    return max(low, min(high, value))


def bar(value, maximum=1023, width=20):
    value = clamp(int(value), 0, maximum)
    fill = int(round((value * width) / float(maximum)))
    return "#" * fill + "." * (width - fill)


def drop_bar(drop, width=16, scale=256):
    drop = clamp(int(drop), 0, scale)
    fill = int(round((drop * width) / float(scale)))
    return "!" * fill + "." * (width - fill)


def label_for(side, key, line_label):
    if line_label != "-":
        return line_label
    if side == "right":
        return RIGHT_LABELS.get(key, "-")
    if side == "left":
        return LEFT_LABELS.get(key, "-")
    return "-"


def parse_ap_line(line):
    match = AP_RE.match(line.strip())
    if not match:
        return None
    data = match.groupdict()
    return {
        "seq": int(data["seq"]),
        "hall": int(data["hall"]),
        "key": int(data["key"]),
        "lane": int(data["lane"]),
        "mux": int(data["mux"]),
        "d": int(data["d"]),
        "label": data["label"],
        "raw": int(data["raw"]),
        "filt": int(data["filt"]),
        "pos": int(data["pos"]),
        "word": data["word"],
        "down": int(data["down"]),
        "rt": int(data["rt"]),
        "status": int(data["status"]),
    }


def update_state(state, sample, side):
    key = sample["key"]
    item = state.setdefault(key, {
        "min": sample["raw"],
        "max": sample["raw"],
        "baseline_sum": 0,
        "baseline_count": 0,
        "baseline": None,
    })
    item.update(sample)
    item["label"] = label_for(side, key, sample["label"])
    item["min"] = min(item["min"], sample["raw"])
    item["max"] = max(item["max"], sample["raw"])


def update_baseline(state, baseline_until, now):
    if now > baseline_until:
        for item in state.values():
            if item["baseline"] is None and item["baseline_count"] > 0:
                item["baseline"] = int(round(item["baseline_sum"] / float(item["baseline_count"])))
        return

    for item in state.values():
        item["baseline_sum"] += item["raw"]
        item["baseline_count"] += 1


def complete_count(state):
    return sum(1 for key in range(64) if key in state)


def top_changes(state, limit):
    rows = []
    for key, item in state.items():
        baseline = item.get("baseline")
        if baseline is None:
            baseline = item["raw"]
        drop = baseline - item["raw"]
        rows.append((drop, key, item))
    rows.sort(reverse=True)
    return rows[:limit]


def render(state, side, started_at, baseline_until, top_n):
    lines = []
    now = time.time()
    baseline_state = "collecting" if now <= baseline_until else "locked"
    lines.append(
        "CH585 ADC text scope side={0} keys={1}/64 uptime={2:.1f}s baseline={3}".format(
            side, complete_count(state), now - started_at, baseline_state
        )
    )
    lines.append(
        "raw bar is ADC/1023. drop is baseline-raw; press usually makes raw smaller."
    )
    lines.append("")

    top = top_changes(state, top_n)
    lines.append("Top drops:")
    if top:
        for drop, key, item in top:
            lines.append(
                "  K{key:02d} L{lane}D{d:02d} {label:<10} raw={raw:04d} base={base:04d} "
                "drop={drop:4d} down={down}".format(
                    key=key,
                    lane=item["lane"] + 1,
                    d=item["d"],
                    label=item["label"][:10],
                    raw=item["raw"],
                    base=item.get("baseline") if item.get("baseline") is not None else item["raw"],
                    drop=drop,
                    down=item["down"],
                )
            )
    else:
        lines.append("  waiting for AP lines...")
    lines.append("")

    for lane in range(4):
        lines.append("Lane {0} / MUX{0}".format(lane + 1))
        for d in range(1, 17):
            key = lane * 16 + (d - 1)
            item = state.get(key)
            if item is None:
                lines.append("  K{0:02d} D{1:02d} {'-':<10} raw=---- [....................]".format(key, d))
                continue

            baseline = item.get("baseline")
            if baseline is None:
                baseline = item["raw"]
            drop = baseline - item["raw"]
            lines.append(
                "  K{key:02d} D{d:02d} {label:<10} raw={raw:04d} "
                "[{rawbar}] drop={drop:4d} [{dropbar}] min={minv:04d} max={maxv:04d} down={down}".format(
                    key=key,
                    d=d,
                    label=item["label"][:10],
                    raw=item["raw"],
                    rawbar=bar(item["raw"]),
                    drop=drop,
                    dropbar=drop_bar(drop),
                    minv=item["min"],
                    maxv=item["max"],
                    down=item["down"],
                )
            )
        lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Text ADC scope for CH585 ADS7948 UART probe AP lines."
    )
    parser.add_argument("port", help="Serial port, e.g. COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--side", choices=("right", "left", "raw"), default="right")
    parser.add_argument("--refresh", type=float, default=0.2, help="Screen refresh seconds")
    parser.add_argument("--baseline-seconds", type=float, default=1.0)
    parser.add_argument("--duration", type=float, default=0.0, help="Exit after seconds; 0 means run forever")
    parser.add_argument("--top", type=int, default=8, help="Number of top drop rows")
    parser.add_argument("--no-clear", action="store_true", help="Do not clear the screen between renders")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.05)
    state = {}
    buffer = ""
    started_at = time.time()
    next_render = started_at
    baseline_until = started_at + args.baseline_seconds

    try:
        while True:
            now = time.time()
            if args.duration > 0 and now - started_at >= args.duration:
                break

            data = ser.read(serial_waiting(ser) or 1)
            if data:
                buffer += to_text(data)
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    sample = parse_ap_line(line)
                    if sample is not None:
                        update_state(state, sample, args.side)

            update_baseline(state, baseline_until, now)

            if now >= next_render:
                screen = render(state, args.side, started_at, baseline_until, args.top)
                if not args.no_clear:
                    sys.stdout.write("\x1b[2J\x1b[H")
                sys.stdout.write(screen + "\n")
                sys.stdout.flush()
                next_render = now + args.refresh

            time.sleep(0.01)
    except KeyboardInterrupt:
        pass
    finally:
        final_screen = render(state, args.side, started_at, baseline_until, args.top)
        if args.no_clear:
            sys.stdout.write("\n--- final ---\n")
        else:
            sys.stdout.write("\x1b[2J\x1b[H")
        sys.stdout.write(final_screen + "\n")
        sys.stdout.flush()
        ser.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
