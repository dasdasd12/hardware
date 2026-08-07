#!/usr/bin/env python3
"""Convert video into full-16-bit LTDC frames for the H417 SDRAM test."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
import zlib
from pathlib import Path

import numpy as np


WIDTH = 800
HEIGHT = 480
DECODE_WIDTH = 854
CROP_X = (DECODE_WIDTH - WIDTH) // 2
FULL16_LANE_MASK = 0xFFFF
DMA_STAGE_BYTES = 16 * 1024
CDC_WINDOW_BYTES = 32 * 1024


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Decode, resize and pack video frames for the H417 SDRAM/LTDC "
            "CDC hardware test."
        )
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--format", choices=("ARGB8888", "ARGB1555"), default="ARGB1555"
    )
    parser.add_argument("--frames", type=int, default=16)
    parser.add_argument("--fps", type=int, default=15)
    parser.add_argument("--width", type=int, default=WIDTH)
    parser.add_argument("--height", type=int, default=HEIGHT)
    parser.add_argument("--ffmpeg", required=True, type=Path)
    parser.add_argument(
        "--no-rotate-180",
        action="store_true",
        help="Do not rotate frames for the board's physically rotated panel.",
    )
    return parser.parse_args()


def decode_rgb24(args: argparse.Namespace, raw_path: Path) -> None:
    command = [
        str(args.ffmpeg),
        "-ss",
        "0",
        "-i",
        str(args.input),
        "-r",
        str(args.fps),
        "-vframes",
        str(args.frames),
        "-s",
        f"{DECODE_WIDTH}x{args.height}",
        "-pix_fmt",
        "rgb24",
        "-f",
        "rawvideo",
        "-y",
        str(raw_path),
    ]
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    expected = args.frames * DECODE_WIDTH * args.height * 3
    actual = raw_path.stat().st_size if raw_path.exists() else 0
    if completed.returncode != 0 or actual != expected:
        details = completed.stderr.decode(errors="replace")[-4000:]
        raise RuntimeError(
            f"ffmpeg decode failed: exit={completed.returncode}, "
            f"bytes={actual}/{expected}\n{details}"
        )


def pack_frame_argb8888(rgb: np.ndarray) -> np.ndarray:
    """Pack standard little-endian LTDC 0xAARRGGBB pixels."""

    packed = np.empty((rgb.shape[0], rgb.shape[1], 4), dtype=np.uint8)
    packed[..., 0] = rgb[..., 2]
    packed[..., 1] = rgb[..., 1]
    packed[..., 2] = rgb[..., 0]
    packed[..., 3] = 0xFF
    return packed


def pack_frame_argb1555(rgb: np.ndarray) -> np.ndarray:
    """Pack standard opaque ARGB1555 for direct LTDC scanout."""

    red = rgb[..., 0].astype(np.uint16) >> 3
    green = rgb[..., 1].astype(np.uint16) >> 3
    blue = rgb[..., 2].astype(np.uint16) >> 3
    packed = np.uint16(0x8000) | (red << 10) | (green << 5) | blue
    return packed.astype("<u2", copy=False)


def file_crc32(path: Path) -> int:
    crc = 0
    with path.open("rb") as source:
        while True:
            block = source.read(1024 * 1024)
            if not block:
                break
            crc = zlib.crc32(block, crc)
    return crc & 0xFFFFFFFF


def main() -> int:
    args = parse_args()
    if args.width != WIDTH or args.height != HEIGHT:
        raise ValueError("This firmware target is fixed at 800x480")
    if args.frames <= 0 or not 1 <= args.fps <= 60:
        raise ValueError("frames must be positive and fps must be 1..60")
    if not args.input.is_file():
        raise FileNotFoundError(args.input)
    if not args.ffmpeg.is_file():
        raise FileNotFoundError(args.ffmpeg)

    bytes_per_pixel = 4 if args.format == "ARGB8888" else 2
    frame_bytes = args.width * args.height * bytes_per_pixel
    total_bytes = frame_bytes * args.frames
    if total_bytes > 32 * 1024 * 1024:
        raise ValueError("Packed frames exceed the 32 MiB SDRAM")
    if total_bytes % DMA_STAGE_BYTES or total_bytes % CDC_WINDOW_BYTES:
        raise ValueError(
            "Packed size must be divisible by both 16 KiB DMA stages and "
            "32 KiB CDC windows"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="h417_sdram_video_") as temp_dir:
        raw_path = Path(temp_dir) / "decoded.rgb24"
        decode_rgb24(args, raw_path)
        raw = np.memmap(
            raw_path,
            dtype=np.uint8,
            mode="r",
            shape=(args.frames, args.height, DECODE_WIDTH, 3),
        )
        with args.output.open("wb") as destination:
            for frame_index in range(args.frames):
                # The source is 16:9 while the 800x480 panel is 5:3. Scale to
                # 854x480 and crop 27 pixels from each side instead of
                # distorting the source to the panel aspect ratio.
                rgb = raw[frame_index, :, CROP_X:CROP_X + args.width, :]
                if not args.no_rotate_180:
                    rgb = rgb[::-1, ::-1]
                if args.format == "ARGB8888":
                    packed = pack_frame_argb8888(rgb)
                else:
                    packed = pack_frame_argb1555(rgb)
                destination.write(packed.tobytes(order="C"))
        # Windows will not remove the temporary RGB file while NumPy's mmap
        # (or the final frame view) is still alive.
        del rgb
        del packed
        raw._mmap.close()
        del raw

    actual_size = args.output.stat().st_size
    if actual_size != total_bytes:
        raise RuntimeError(f"packed size mismatch: {actual_size}/{total_bytes}")
    crc = file_crc32(args.output)
    metadata = {
        "input": str(args.input.resolve()),
        "output": str(args.output.resolve()),
        "format": args.format,
        "width": args.width,
        "height": args.height,
        "frames": args.frames,
        "fps": args.fps,
        "bytes_per_pixel": bytes_per_pixel,
        "frame_bytes": frame_bytes,
        "total_bytes": total_bytes,
        "crc32": f"{crc:08x}",
        "lane_mask": f"{FULL16_LANE_MASK:04x}",
        "ignored_lane_mask": "0000",
        "rotate_180": not args.no_rotate_180,
        "resize": f"{DECODE_WIDTH}x{args.height}",
        "crop": f"x={CROP_X},width={args.width}",
    }
    print(json.dumps(metadata, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
