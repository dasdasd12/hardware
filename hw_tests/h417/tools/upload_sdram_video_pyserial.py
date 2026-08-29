#!/usr/bin/env python3
"""Upload a packed raw VIDEO image (ARGB1555/ARGB8888) over CDC via pyserial.

Raw-video twin of upload_sdram_h4v1_pyserial.py: same Win32/pyserial host
stack (avoids the System.IO.Ports ERROR_SEM_TIMEOUT failures), same 4 KiB
stop-and-wait credit protocol the firmware implements for the VIDEO command.
Used as the regression control for the H4V1 USB hang investigation.
"""

from __future__ import annotations

import argparse
import re
import sys
import time
import zlib
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[3]
LOCAL_DEPS = REPO_ROOT / ".tmp" / "h417_codec_bench_deps"
if LOCAL_DEPS.is_dir():
    sys.path.insert(0, str(LOCAL_DEPS))

try:
    import serial
except ModuleNotFoundError as exc:  # pragma: no cover - host setup failure
    raise SystemExit(
        f"pyserial is missing; expected it under {LOCAL_DEPS}"
    ) from exc


class CdcText:
    def __init__(self, port: "serial.Serial") -> None:
        self.port = port
        self.pending = ""

    def pump(self) -> None:
        available = self.port.in_waiting
        data = self.port.read(available if available else 1)
        if not data:
            return
        text = data.decode("ascii", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
        self.pending += text
        if len(self.pending) > 16384:
            self.pending = self.pending[-8192:]

    def wait(self, pattern: str, timeout: float) -> re.Match[str] | None:
        regex = re.compile(pattern)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.pump()
            match = regex.search(self.pending)
            if match:
                self.pending = self.pending[match.end() :]
                return match
            time.sleep(0.001)
        return None

    def drain(self, seconds: float) -> None:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.pump()
            time.sleep(0.005)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--packed", type=Path, required=True)
    parser.add_argument("--format", choices=["ARGB1555", "ARGB8888"],
                        default="ARGB1555")
    parser.add_argument("--frames", type=int, default=32)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--window", type=int, default=4096)
    parser.add_argument("--chunk", type=int, default=1024)
    parser.add_argument("--chunk-delay-ms", type=float, default=0.0,
                        help="optional sleep between chunk writes "
                             "(USB rate-sensitivity probe)")
    parser.add_argument("--write-timeout", type=float, default=5.0)
    parser.add_argument("--result-timeout", type=float, default=300.0)
    parser.add_argument("--crc32", default="",
                        help="override transfer crc32 (hex); default: computed")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    packed = args.packed.resolve()
    if not packed.is_file():
        raise SystemExit(f"packed file missing: {packed}")
    total = packed.stat().st_size
    if args.crc32:
        expected_crc = args.crc32.lower()
    else:
        expected_crc = f"{zlib.crc32(packed.read_bytes()) & 0xFFFFFFFF:08x}"
    if total % args.window:
        raise SystemExit("transfer size is not aligned to the credit window")
    if args.window <= 0 or args.chunk <= 0 or args.window % args.chunk:
        raise SystemExit("window must be a positive multiple of chunk")

    print(f"VIDEO pyserial path={packed}")
    print(
        "VIDEO pyserial format={} frames={} fps={} bytes={} crc={} "
        "window={} chunk={} backend=pyserial-{}".format(
            args.format, args.frames, args.fps, total, expected_crc,
            args.window, args.chunk, serial.VERSION,
        )
    )

    with serial.Serial(
        port=args.port,
        baudrate=115200,
        timeout=0.02,
        write_timeout=args.write_timeout,
        rtscts=False,
        dsrdtr=False,
        xonxoff=False,
    ) as port:
        port.dtr = True
        port.rts = True
        try:
            port.set_buffer_size(rx_size=65536, tx_size=args.window)
        except (AttributeError, OSError):
            pass
        port.reset_input_buffer()
        port.reset_output_buffer()
        time.sleep(0.25)
        text = CdcText(port)

        waiting = None
        for _ in range(5):
            port.write(b"STATUS\r\n")
            waiting = text.wait(r"VIDEO WAIT", 2.0)
            if waiting:
                break
        if not waiting:
            raise SystemExit("MCU did not enter the VIDEO command state")

        command = (
            f"VIDEO {args.format} {args.frames} {args.fps} "
            f"{total} {expected_crc}\r\n"
        ).encode("ascii")
        port.write(command)
        if not text.wait(r"VIDEO READY.*", 10.0):
            raise SystemExit("MCU did not accept VIDEO metadata")

        sent = 0
        started = time.monotonic()
        with packed.open("rb") as stream:
            while sent < total:
                block = stream.read(args.window)
                if len(block) != args.window:
                    raise SystemExit(f"upload image ended at {sent} bytes")
                try:
                    for offset in range(0, len(block), args.chunk):
                        part = block[offset : offset + args.chunk]
                        written = port.write(part)
                        if written != len(part):
                            raise serial.SerialTimeoutException(
                                f"short write {written}/{len(part)}"
                            )
                        if args.chunk_delay_ms > 0:
                            time.sleep(args.chunk_delay_ms / 1000.0)
                except (serial.SerialException, serial.SerialTimeoutException):
                    print(
                        f"\nPY CDC WRITE FAIL at={sent + offset}/{total}; "
                        "draining MCU diagnostics (45s: RX WAIT + upload_timeout + IWDG recovery)..."
                    )
                    text.drain(45.0)
                    raise

                sent += len(block)
                ack = f"VIDEO ACK bytes={sent}/{total}"
                if not text.wait(re.escape(ack), 15.0):
                    print(f"\nPY ACK TIMEOUT at={sent}/{total}")
                    print("draining MCU diagnostics (45s: RX WAIT + upload_timeout + IWDG recovery)...")
                    text.drain(45.0)
                    return 3
                if sent % (1024 * 1024) == 0 or sent == total:
                    elapsed = time.monotonic() - started
                    print(
                        f"CDC pyserial upload {sent / 1048576:.3f}/"
                        f"{total / 1048576:.3f} MiB "
                        f"({sent / 1048576 / elapsed:.2f} MiB/s)"
                    )

        result = text.wait(r"RESULT (PASS|FAIL)", args.result_timeout)
        if not result:
            print("Timed out waiting for readback/LTDC result")
            return 4
        return 0 if result.group(1) == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
