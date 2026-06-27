#!/usr/bin/env python3
"""Read CH585 2.4G 8K RF receiver statistics from USBHS custom HID EP3."""

from __future__ import annotations

import argparse
import struct
import sys
import time

VID = 0x1A86
PID = 0xFE07
USAGE_PAGE_VENDOR = 0xFF00


def load_hid():
    try:
        import hid  # type: ignore
    except ImportError:
        print("Missing Python package: hid", file=sys.stderr)
        print("Install with: py -m pip install hidapi", file=sys.stderr)
        raise SystemExit(2)
    return hid


def choose_device(hid, vid: int, pid: int):
    devices = hid.enumerate(vid, pid)
    if not devices:
        raise SystemExit(f"No HID device found for VID={vid:04X} PID={pid:04X}")

    for dev in devices:
        if dev.get("usage_page") == USAGE_PAGE_VENDOR:
            return dev

    for dev in devices:
        if dev.get("interface_number") == 2:
            return dev

    print("Vendor custom HID interface was not obvious; using first matching HID.")
    for index, dev in enumerate(devices):
        print(
            f"  [{index}] interface={dev.get('interface_number')} "
            f"usage_page={dev.get('usage_page')} usage={dev.get('usage')} "
            f"product={dev.get('product_string')}"
        )
    return devices[0]


def normalize_report(data: bytes) -> bytes | None:
    if not data:
        return None
    if len(data) >= 64 and data[0] == 0xA8:
        return data[:64]
    if len(data) >= 65 and data[0] == 0x00 and data[1] == 0xA8:
        return data[1:65]
    return None


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def parse_report(buf: bytes) -> dict[str, int]:
    xorv = 0
    for b in buf[:63]:
        xorv ^= b
    if xorv != buf[63]:
        return {"bad_xor_report": 1}

    return {
        "seq": struct.unpack_from("<H", buf, 2)[0],
        "ok": u32(buf, 4),
        "lost": u32(buf, 8),
        "bad_xor": u32(buf, 12),
        "bad_len": u32(buf, 16),
        "crc": u32(buf, 20),
        "rx_irq": u32(buf, 24),
        "usb_sent": u32(buf, 28),
        "usb_drop": u32(buf, 32),
        "legacy_ok": u32(buf, 36),
        "timeout": u32(buf, 40),
        "rssi": struct.unpack_from("<b", buf, 44)[0],
        "key_ok": u32(buf, 45),
        "key_seq": struct.unpack_from("<H", buf, 49)[0],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vid", type=lambda s: int(s, 0), default=VID)
    parser.add_argument("--pid", type=lambda s: int(s, 0), default=PID)
    parser.add_argument("--timeout-ms", type=int, default=1000)
    parser.add_argument("--duration", type=float, default=0.0)
    args = parser.parse_args()

    hid = load_hid()
    dev_info = choose_device(hid, args.vid, args.pid)
    dev = hid.device()
    dev.open_path(dev_info["path"])
    dev.set_nonblocking(False)

    print(
        "Opened "
        f"VID={args.vid:04X} PID={args.pid:04X} "
        f"interface={dev_info.get('interface_number')} "
        f"usage_page={dev_info.get('usage_page')}"
    )

    last_t = time.monotonic()
    start_t = last_t
    last_count = 0
    count = 0
    last = None

    while True:
        if args.duration and (time.monotonic() - start_t) >= args.duration:
            return 0

        raw = bytes(dev.read(65, args.timeout_ms))
        report = normalize_report(raw)
        if report is None:
            if raw:
                print(f"unknown report: {raw[:8].hex()} len={len(raw)}")
            continue

        parsed = parse_report(report)
        if "bad_xor_report" in parsed:
            print("bad USB report xor")
            continue

        count += 1
        now = time.monotonic()
        if now - last_t >= 1.0:
            hs_rate = (count - last_count) / (now - last_t)
            if last:
                print(
                    f"hs/s={hs_rate:7.1f} "
                    f"seq={last['seq']:5d} "
                    f"rf_ok={last['ok']} "
                    f"key_ok={last['key_ok']} "
                    f"key_seq={last['key_seq']} "
                    f"rf_lost={last['lost']} "
                    f"legacy_ok={last['legacy_ok']} "
                    f"rx_irq={last['rx_irq']} "
                    f"bad_len={last['bad_len']} "
                    f"bad_xor={last['bad_xor']} "
                    f"crc={last['crc']} "
                    f"timeout={last['timeout']} "
                    f"usb_sent={last['usb_sent']} "
                    f"usb_drop={last['usb_drop']} "
                    f"rssi={last['rssi']}",
                    flush=True,
                )
            last_t = now
            last_count = count
        last = parsed


if __name__ == "__main__":
    raise SystemExit(main())
