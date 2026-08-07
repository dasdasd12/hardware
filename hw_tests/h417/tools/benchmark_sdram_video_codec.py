#!/usr/bin/env python3
"""Benchmark MCU-friendly compression for an 800x480 ARGB1555 video.

The important path is parity delta: frame N is XORed with frame N-2.  This
matches a ping-pong LTDC framebuffer.  The inactive framebuffer already holds
frame N-2, so the MCU can update it in place without first copying frame N-1.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
import time
import zlib
from dataclasses import dataclass, asdict
from pathlib import Path

import numpy as np

try:
    import lz4.block
except ModuleNotFoundError as exc:
    raise SystemExit(
        "python-lz4 is required; install it into an isolated directory and "
        "add that directory to PYTHONPATH"
    ) from exc


WIDTH = 800
HEIGHT = 480
DECODE_WIDTH = 854
CROP_X = (DECODE_WIDTH - WIDTH) // 2
PIXELS = WIDTH * HEIGHT
FRAME_BYTES = PIXELS * 2


@dataclass
class CodecStats:
    name: str
    total_bytes: int = 0
    maximum_frame_bytes: int = 0
    keyframes: int = 0
    keyframe_bytes: int = 0
    delta_frames: int = 0
    delta_bytes: int = 0

    def add(self, size: int, keyframe: bool) -> None:
        self.total_bytes += size
        self.maximum_frame_bytes = max(self.maximum_frame_bytes, size)
        if keyframe:
            self.keyframes += 1
            self.keyframe_bytes += size
        else:
            self.delta_frames += 1
            self.delta_bytes += size


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--ffmpeg", required=True, type=Path)
    parser.add_argument("--frames", type=int, default=360)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--json-output", type=Path)
    return parser.parse_args()


def decode_rgb24(args: argparse.Namespace, raw_path: Path) -> None:
    command = [
        str(args.ffmpeg),
        "-ss", "0",
        "-i", str(args.input),
        "-r", str(args.fps),
        "-vframes", str(args.frames),
        "-s", f"{DECODE_WIDTH}x{HEIGHT}",
        "-pix_fmt", "rgb24",
        "-f", "rawvideo",
        "-y", str(raw_path),
    ]
    completed = subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
    )
    expected = args.frames * DECODE_WIDTH * HEIGHT * 3
    actual = raw_path.stat().st_size if raw_path.exists() else 0
    if completed.returncode != 0 or actual != expected:
        details = completed.stderr.decode(errors="replace")[-4000:]
        raise RuntimeError(
            f"ffmpeg decode failed: exit={completed.returncode}, "
            f"bytes={actual}/{expected}\n{details}"
        )


def pack_argb1555(rgb: np.ndarray) -> np.ndarray:
    red = rgb[..., 0].astype(np.uint16) >> 3
    green = rgb[..., 1].astype(np.uint16) >> 3
    blue = rgb[..., 2].astype(np.uint16) >> 3
    return (
        np.uint16(0x8000) | (red << 10) | (green << 5) | blue
    ).astype("<u2", copy=False).reshape(-1)


def lz4_hc(data: bytes) -> bytes:
    return lz4.block.compress(
        data, mode="high_compression", compression=12, store_size=False
    )


def put_varint(output: bytearray, value: int) -> None:
    while value >= 0x80:
        output.append((value & 0x7F) | 0x80)
        value >>= 7
    output.append(value)


def zero_literal_rle(words: np.ndarray) -> bytes:
    """Encode alternating zero-skip and literal XOR-word runs.

    Header is unsigned LEB128: bit 0 selects literal, remaining bits contain
    the run length in 16-bit pixels.  Literal data follows in little endian.
    A decoder can apply literal XOR words directly to the inactive LTDC buffer.
    """

    nonzero = words != 0
    if words.size == 0:
        return b""
    transitions = np.flatnonzero(
        np.concatenate((np.array([True]), nonzero[1:] != nonzero[:-1],
                        np.array([True])))
    )
    output = bytearray()
    for run_index in range(len(transitions) - 1):
        start = int(transitions[run_index])
        end = int(transitions[run_index + 1])
        literal = bool(nonzero[start])
        put_varint(output, ((end - start) << 1) | int(literal))
        if literal:
            output.extend(words[start:end].astype("<u2", copy=False).tobytes())
    return bytes(output)


def is_keyframe(index: int, gop: int) -> bool:
    # Two consecutive keys initialize both ping-pong framebuffer parities.
    return (index % gop) < 2


def main() -> int:
    args = parse_args()
    if args.frames <= 1 or args.fps <= 0:
        raise ValueError("frames must be >1 and fps must be positive")
    if not args.input.is_file() or not args.ffmpeg.is_file():
        raise FileNotFoundError("input video or ffmpeg not found")

    gops = (30, 60, 120)
    stats: dict[str, CodecStats] = {
        "independent_lz4hc": CodecStats("independent_lz4hc"),
    }
    for gop in gops:
        stats[f"seq1_lz4hc_gop{gop}"] = CodecStats(
            f"seq1_lz4hc_gop{gop}"
        )
        stats[f"parity2_lz4hc_gop{gop}"] = CodecStats(
            f"parity2_lz4hc_gop{gop}"
        )
        stats[f"parity2_rle_gop{gop}"] = CodecStats(
            f"parity2_rle_gop{gop}"
        )
        stats[f"parity2_rle_lz4hc_gop{gop}"] = CodecStats(
            f"parity2_rle_lz4hc_gop{gop}"
        )

    changed_pixels_seq = 0
    changed_pixels_parity = 0
    delta_frames_counted = 0
    raw_crc = 0
    start_time = time.perf_counter()

    with tempfile.TemporaryDirectory(prefix="h417_codec_bench_") as temp_dir:
        raw_path = Path(temp_dir) / "decoded.rgb24"
        print("DECODE START", flush=True)
        decode_rgb24(args, raw_path)
        print(f"DECODE DONE bytes={raw_path.stat().st_size}", flush=True)
        raw = np.memmap(
            raw_path,
            dtype=np.uint8,
            mode="r",
            shape=(args.frames, HEIGHT, DECODE_WIDTH, 3),
        )
        previous: np.ndarray | None = None
        parity_previous: list[np.ndarray | None] = [None, None]

        for index in range(args.frames):
            rgb = raw[index, :, CROP_X:CROP_X + WIDTH, :]
            # Match the current board/video upload path.
            frame = pack_argb1555(rgb[::-1, ::-1]).copy()
            frame_bytes = frame.tobytes()
            raw_crc = zlib.crc32(frame_bytes, raw_crc)
            full_lz4 = lz4_hc(frame_bytes)
            stats["independent_lz4hc"].add(len(full_lz4), True)

            if previous is not None:
                seq_delta = np.bitwise_xor(frame, previous)
                changed_pixels_seq += int(np.count_nonzero(seq_delta))
                seq_delta_lz4_size = len(lz4_hc(
                    seq_delta.astype("<u2", copy=False).tobytes()
                ))
            else:
                seq_delta = None
                seq_delta_lz4_size = 0
            parity_base = parity_previous[index & 1]
            if parity_base is not None:
                parity_delta = np.bitwise_xor(frame, parity_base)
                changed_pixels_parity += int(np.count_nonzero(parity_delta))
                delta_frames_counted += 1
                parity_delta_bytes = parity_delta.astype(
                    "<u2", copy=False
                ).tobytes()
                parity_delta_lz4_size = len(lz4_hc(parity_delta_bytes))
                parity_rle = zero_literal_rle(parity_delta)
                parity_rle_size = len(parity_rle)
                parity_rle_lz4_size = len(lz4_hc(parity_rle))
            else:
                parity_delta = None
                parity_delta_lz4_size = 0
                parity_rle_size = 0
                parity_rle_lz4_size = 0

            for gop in gops:
                key = is_keyframe(index, gop)
                if key or seq_delta is None:
                    seq_size = len(full_lz4)
                else:
                    seq_size = seq_delta_lz4_size
                stats[f"seq1_lz4hc_gop{gop}"].add(seq_size, key)

                if key or parity_delta is None:
                    parity_lz4_size = len(full_lz4)
                    rle_size = len(frame_bytes)
                    rle_lz4_size = len(full_lz4)
                else:
                    parity_lz4_size = parity_delta_lz4_size
                    rle_size = parity_rle_size
                    rle_lz4_size = parity_rle_lz4_size
                stats[f"parity2_lz4hc_gop{gop}"].add(
                    parity_lz4_size, key
                )
                stats[f"parity2_rle_gop{gop}"].add(rle_size, key)
                stats[f"parity2_rle_lz4hc_gop{gop}"].add(
                    rle_lz4_size, key
                )

            previous = frame
            parity_previous[index & 1] = frame
            if ((index + 1) % 30 == 0) or (index + 1 == args.frames):
                print(f"BENCH progress={index + 1}/{args.frames}", flush=True)

        raw._mmap.close()
        del raw

    raw_bytes = args.frames * FRAME_BYTES
    results = []
    for value in stats.values():
        item = asdict(value)
        item["ratio_raw_over_compressed"] = raw_bytes / value.total_bytes
        item["mib"] = value.total_bytes / (1024 * 1024)
        item["average_frame_bytes"] = value.total_bytes / args.frames
        item["fits_128mib"] = value.total_bytes <= 128 * 1024 * 1024
        results.append(item)

    report = {
        "format": "ARGB1555",
        "width": WIDTH,
        "height": HEIGHT,
        "fps": args.fps,
        "frames": args.frames,
        "duration_seconds": args.frames / args.fps,
        "frame_bytes": FRAME_BYTES,
        "raw_bytes": raw_bytes,
        "raw_mib": raw_bytes / (1024 * 1024),
        "raw_crc32": f"{raw_crc & 0xFFFFFFFF:08x}",
        "changed_pixels_seq_percent": (
            changed_pixels_seq * 100.0 /
            ((args.frames - 1) * PIXELS)
        ),
        "changed_pixels_parity_percent": (
            changed_pixels_parity * 100.0 /
            (delta_frames_counted * PIXELS)
        ),
        "elapsed_seconds": time.perf_counter() - start_time,
        "results": results,
    }
    print("RESULTS")
    for item in sorted(results, key=lambda row: row["total_bytes"]):
        print(
            f"{item['name']:30s} {item['mib']:8.2f} MiB "
            f"ratio={item['ratio_raw_over_compressed']:.2f} "
            f"max_frame={item['maximum_frame_bytes']} "
            f"fits128={int(item['fits_128mib'])}"
        )
    print(json.dumps(report, ensure_ascii=False))
    if args.json_output is not None:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(
            json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
