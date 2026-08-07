#!/usr/bin/env python3
"""Upload an already packed H4V1 image over CDC using pyserial/Win32 I/O.

This transport intentionally bypasses System.IO.Ports.SerialPort.  The H417
can remain ACK-ready while SerialPort.Write fails with ERROR_SEM_TIMEOUT; this
script gives us an independent host stack without changing the firmware or
the on-wire credit protocol.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
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
    parser.add_argument(
        "--packed",
        type=Path,
        default=REPO_ROOT
        / ".tmp"
        / "sdram_video"
        / "miku_h4v1_90f_30fps.h4v",
    )
    parser.add_argument("--window", type=int, default=4096)
    parser.add_argument("--chunk", type=int, default=1024)
    parser.add_argument("--write-timeout", type=float, default=5.0)
    parser.add_argument("--result-timeout", type=float, default=600.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    packed = args.packed.resolve()
    upload = Path(f"{packed}.upload.bin")
    metadata_path = Path(f"{packed}.json")
    if not upload.is_file() or not metadata_path.is_file():
        raise SystemExit(f"packed upload or metadata missing for {packed}")

    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    total = int(metadata["transfer_bytes"])
    expected_crc = str(metadata["transfer_crc32"])
    if upload.stat().st_size != total:
        raise SystemExit("upload length does not match metadata")
    if total % args.window:
        raise SystemExit("transfer size is not aligned to the credit window")
    if args.window <= 0 or args.chunk <= 0 or args.window % args.chunk:
        raise SystemExit("window must be a positive multiple of chunk")

    print(f"H4V1 pyserial path={packed}")
    print(
        "H4V1 pyserial bytes={} crc={} window={} chunk={} backend=pyserial-{}".format(
            total, expected_crc, args.window, args.chunk, serial.VERSION
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
            waiting = text.wait(r"H4V1 WAIT", 2.0)
            if waiting:
                break
        if not waiting:
            raise SystemExit("MCU did not enter the H4V1 command state")

        command = f"H4V1 {total} {expected_crc}\r\n".encode("ascii")
        port.write(command)
        if not text.wait(r"VIDEO READY format=H4V1.*", 10.0):
            raise SystemExit("MCU did not accept H4V1 metadata")

        sent = 0
        with upload.open("rb") as stream:
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
                except (serial.SerialException, serial.SerialTimeoutException):
                    print(
                        f"\nPY CDC WRITE FAIL at={sent + offset}/{total}; "
                        "draining MCU diagnostics..."
                    )
                    text.drain(6.0)
                    raise

                sent += len(block)
                ack = f"VIDEO ACK bytes={sent}/{total}"
                if not text.wait(re.escape(ack), 15.0):
                    print(f"\nPY ACK TIMEOUT at={sent}/{total}")
                    text.drain(6.0)
                    return 3
                if sent % (1024 * 1024) == 0 or sent == total:
                    print(f"CDC pyserial upload {sent / 1048576:.3f}/{total / 1048576:.3f} MiB")

        result = text.wait(r"RESULT (PASS|FAIL)", args.result_timeout)
        if not result:
            print("Timed out waiting for decode/playback result")
            return 4
        return 0 if result.group(1) == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
