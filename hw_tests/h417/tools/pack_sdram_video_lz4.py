#!/usr/bin/env python3
"""Pack and verify an H417 ARGB1555 keyframe/delta LZ4 video container."""

from __future__ import annotations

import argparse
import json
import struct
import subprocess
import tempfile
import time
import zlib
from pathlib import Path

import numpy as np

try:
    import lz4.block
except ModuleNotFoundError as exc:
    raise SystemExit(
        "python-lz4 is required; add the isolated dependency directory to "
        "PYTHONPATH"
    ) from exc


WIDTH = 800
HEIGHT = 480
DECODE_WIDTH = 854
CROP_X = (DECODE_WIDTH - WIDTH) // 2
FRAME_BYTES = WIDTH * HEIGHT * 2

MAGIC = b"H4V1"
VERSION = 1
HEADER_BYTES = 64
INDEX_ENTRY_BYTES = 24
DATA_ALIGNMENT = 4096
PIXEL_FORMAT_ARGB1555 = 1

CONTAINER_FLAG_XOR_DELTA = 1 << 0
CONTAINER_FLAG_ROTATE_180 = 1 << 1
CONTAINER_FLAG_LZ4_RAW_BLOCK = 1 << 2

FRAME_FLAG_KEY = 1 << 0
FRAME_FLAG_XOR_DELTA = 1 << 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--ffmpeg", required=True, type=Path)
    parser.add_argument("--frames", type=int, default=360)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--gop", type=int, default=30)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--upload-output", type=Path)
    parser.add_argument("--transfer-alignment", type=int, default=32768)
    parser.add_argument("--no-rotate-180", action="store_true")
    return parser.parse_args()


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def crc32_file(path: Path) -> int:
    crc = 0
    with path.open("rb") as source:
        while True:
            chunk = source.read(1024 * 1024)
            if not chunk:
                break
            crc = zlib.crc32(chunk, crc)
    return crc & 0xFFFFFFFF


def make_upload_image(source_path: Path,
                      upload_path: Path,
                      alignment: int) -> tuple[int, int]:
    if alignment <= 0 or alignment & (alignment - 1):
        raise ValueError("transfer alignment must be a positive power of two")
    if source_path.resolve() == upload_path.resolve():
        raise ValueError("upload output must differ from the H4V1 container")
    upload_path.parent.mkdir(parents=True, exist_ok=True)
    source_bytes = source_path.stat().st_size
    transfer_bytes = align_up(source_bytes, alignment)
    crc = 0
    with source_path.open("rb") as source, upload_path.open("wb") as target:
        while True:
            chunk = source.read(1024 * 1024)
            if not chunk:
                break
            target.write(chunk)
            crc = zlib.crc32(chunk, crc)
        padding = transfer_bytes - source_bytes
        zeroes = bytes(min(1024 * 1024, max(1, padding)))
        while padding:
            chunk = zeroes[:min(len(zeroes), padding)]
            target.write(chunk)
            crc = zlib.crc32(chunk, crc)
            padding -= len(chunk)
    return transfer_bytes, crc & 0xFFFFFFFF


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


def source_frame(raw: np.ndarray, index: int, rotate_180: bool) -> np.ndarray:
    rgb = raw[index, :, CROP_X:CROP_X + WIDTH, :]
    if rotate_180:
        rgb = rgb[::-1, ::-1]
    return pack_argb1555(rgb).copy()


def lz4_hc(data: bytes) -> bytes:
    return lz4.block.compress(
        data, mode="high_compression", compression=12, store_size=False
    )


def build_header(
    *,
    fps: int,
    frame_count: int,
    gop: int,
    rotate_180: bool,
    index_offset: int,
    data_offset: int,
    file_bytes: int,
    raw_crc32: int,
    index_crc32: int,
    header_crc32: int,
) -> bytes:
    flags = CONTAINER_FLAG_XOR_DELTA | CONTAINER_FLAG_LZ4_RAW_BLOCK
    if rotate_180:
        flags |= CONTAINER_FLAG_ROTATE_180
    header = struct.pack(
        "<4sHHHHHHIIIIIIIIIIII",
        MAGIC,
        VERSION,
        HEADER_BYTES,
        WIDTH,
        HEIGHT,
        fps,
        PIXEL_FORMAT_ARGB1555,
        frame_count,
        FRAME_BYTES,
        gop,
        flags,
        index_offset,
        INDEX_ENTRY_BYTES,
        data_offset,
        file_bytes,
        raw_crc32,
        index_crc32,
        header_crc32,
        0,
    )
    if len(header) != HEADER_BYTES:
        raise AssertionError(f"header size {len(header)} != {HEADER_BYTES}")
    return header


def unpack_header(header: bytes) -> dict[str, int | bytes]:
    if len(header) != HEADER_BYTES:
        raise ValueError("short header")
    values = struct.unpack("<4sHHHHHHIIIIIIIIIIII", header)
    keys = (
        "magic", "version", "header_bytes", "width", "height", "fps",
        "pixel_format", "frame_count", "frame_bytes", "gop", "flags",
        "index_offset", "index_entry_bytes", "data_offset", "file_bytes",
        "raw_crc32", "index_crc32", "header_crc32", "reserved",
    )
    return dict(zip(keys, values))


def pack_index_entry(
    offset: int,
    compressed_bytes: int,
    raw_crc32: int,
    payload_crc32: int,
    flags: int,
) -> bytes:
    return struct.pack(
        "<IIIIII",
        offset,
        compressed_bytes,
        FRAME_BYTES,
        raw_crc32,
        payload_crc32,
        flags,
    )


def unpack_index_entry(data: bytes) -> dict[str, int]:
    values = struct.unpack("<IIIIII", data)
    keys = (
        "offset", "compressed_bytes", "uncompressed_bytes",
        "raw_crc32", "payload_crc32", "flags",
    )
    return dict(zip(keys, values))


def verify_container(
    path: Path,
    raw: np.ndarray,
    rotate_180: bool,
) -> dict[str, int | str]:
    with path.open("rb") as source:
        header_bytes = source.read(HEADER_BYTES)
        header = unpack_header(header_bytes)
        if (
            header["magic"] != MAGIC
            or header["version"] != VERSION
            or header["header_bytes"] != HEADER_BYTES
            or header["width"] != WIDTH
            or header["height"] != HEIGHT
            or header["pixel_format"] != PIXEL_FORMAT_ARGB1555
            or header["frame_bytes"] != FRAME_BYTES
            or header["index_entry_bytes"] != INDEX_ENTRY_BYTES
            or header["file_bytes"] != path.stat().st_size
        ):
            raise RuntimeError(f"invalid header: {header}")

        crc_header = bytearray(header_bytes)
        struct.pack_into("<I", crc_header, 56, 0)
        if zlib.crc32(crc_header) & 0xFFFFFFFF != header["header_crc32"]:
            raise RuntimeError("header CRC mismatch")

        source.seek(int(header["index_offset"]))
        index_bytes = source.read(
            int(header["frame_count"]) * INDEX_ENTRY_BYTES
        )
        if zlib.crc32(index_bytes) & 0xFFFFFFFF != header["index_crc32"]:
            raise RuntimeError("index CRC mismatch")
        entries = [
            unpack_index_entry(index_bytes[i:i + INDEX_ENTRY_BYTES])
            for i in range(0, len(index_bytes), INDEX_ENTRY_BYTES)
        ]

        previous: np.ndarray | None = None
        stream_crc = 0
        for index, entry in enumerate(entries):
            source.seek(entry["offset"])
            payload = source.read(entry["compressed_bytes"])
            if len(payload) != entry["compressed_bytes"]:
                raise RuntimeError(f"frame {index}: short payload")
            if zlib.crc32(payload) & 0xFFFFFFFF != entry["payload_crc32"]:
                raise RuntimeError(f"frame {index}: payload CRC mismatch")
            decoded = lz4.block.decompress(
                payload, uncompressed_size=entry["uncompressed_bytes"]
            )
            words = np.frombuffer(decoded, dtype="<u2").copy()
            if entry["flags"] & FRAME_FLAG_XOR_DELTA:
                if previous is None:
                    raise RuntimeError(f"frame {index}: delta without base")
                words ^= previous
            elif not entry["flags"] & FRAME_FLAG_KEY:
                raise RuntimeError(f"frame {index}: invalid flags")

            expected = source_frame(raw, index, rotate_180)
            if not np.array_equal(words, expected):
                mismatch = int(np.flatnonzero(words != expected)[0])
                raise RuntimeError(
                    f"frame {index}: pixel mismatch at {mismatch}, "
                    f"expected={int(expected[mismatch]):04x}, "
                    f"actual={int(words[mismatch]):04x}"
                )
            frame_bytes = words.astype("<u2", copy=False).tobytes()
            frame_crc = zlib.crc32(frame_bytes) & 0xFFFFFFFF
            if frame_crc != entry["raw_crc32"]:
                raise RuntimeError(f"frame {index}: reconstructed CRC mismatch")
            stream_crc = zlib.crc32(frame_bytes, stream_crc)
            previous = words
            if ((index + 1) % 30 == 0) or (index + 1 == len(entries)):
                print(f"VERIFY progress={index + 1}/{len(entries)}", flush=True)

        if stream_crc & 0xFFFFFFFF != header["raw_crc32"]:
            raise RuntimeError("raw stream CRC mismatch")
        return {
            "frames_verified": len(entries),
            "raw_crc32": f"{stream_crc & 0xFFFFFFFF:08x}",
            "header_crc32": f"{int(header['header_crc32']):08x}",
            "index_crc32": f"{int(header['index_crc32']):08x}",
        }


def main() -> int:
    args = parse_args()
    if args.frames <= 0 or args.fps <= 0 or args.gop <= 0:
        raise ValueError("frames, fps and gop must be positive")
    if not args.input.is_file() or not args.ffmpeg.is_file():
        raise FileNotFoundError("input video or ffmpeg not found")

    rotate_180 = not args.no_rotate_180
    index_offset = HEADER_BYTES
    data_offset = align_up(
        index_offset + args.frames * INDEX_ENTRY_BYTES, DATA_ALIGNMENT
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()

    with tempfile.TemporaryDirectory(prefix="h417_video_pack_") as temp_dir:
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
        entries: list[bytes] = []
        previous: np.ndarray | None = None
        raw_stream_crc = 0
        compressed_data_bytes = 0
        keyframes = 0
        delta_frames = 0
        maximum_payload = 0

        with args.output.open("wb+") as destination:
            destination.write(b"\x00" * data_offset)
            for index in range(args.frames):
                frame = source_frame(raw, index, rotate_180)
                frame_bytes = frame.tobytes()
                key = previous is None or (index % args.gop) == 0
                if key:
                    encoded_input = frame_bytes
                    frame_flags = FRAME_FLAG_KEY
                    keyframes += 1
                else:
                    delta = np.bitwise_xor(frame, previous)
                    encoded_input = delta.astype("<u2", copy=False).tobytes()
                    frame_flags = FRAME_FLAG_XOR_DELTA
                    delta_frames += 1
                payload = lz4_hc(encoded_input)
                offset = destination.tell()
                destination.write(payload)
                entries.append(pack_index_entry(
                    offset,
                    len(payload),
                    zlib.crc32(frame_bytes) & 0xFFFFFFFF,
                    zlib.crc32(payload) & 0xFFFFFFFF,
                    frame_flags,
                ))
                compressed_data_bytes += len(payload)
                maximum_payload = max(maximum_payload, len(payload))
                raw_stream_crc = zlib.crc32(frame_bytes, raw_stream_crc)
                previous = frame
                if ((index + 1) % 30 == 0) or (index + 1 == args.frames):
                    print(f"PACK progress={index + 1}/{args.frames}", flush=True)

            file_bytes = destination.tell()
            index_blob = b"".join(entries)
            index_crc = zlib.crc32(index_blob) & 0xFFFFFFFF
            header = build_header(
                fps=args.fps,
                frame_count=args.frames,
                gop=args.gop,
                rotate_180=rotate_180,
                index_offset=index_offset,
                data_offset=data_offset,
                file_bytes=file_bytes,
                raw_crc32=raw_stream_crc & 0xFFFFFFFF,
                index_crc32=index_crc,
                header_crc32=0,
            )
            header_crc = zlib.crc32(header) & 0xFFFFFFFF
            header = build_header(
                fps=args.fps,
                frame_count=args.frames,
                gop=args.gop,
                rotate_180=rotate_180,
                index_offset=index_offset,
                data_offset=data_offset,
                file_bytes=file_bytes,
                raw_crc32=raw_stream_crc & 0xFFFFFFFF,
                index_crc32=index_crc,
                header_crc32=header_crc,
            )
            destination.seek(0)
            destination.write(header)
            destination.seek(index_offset)
            destination.write(index_blob)
            destination.flush()

        print("VERIFY START", flush=True)
        verification = verify_container(args.output, raw, rotate_180)
        print("VERIFY PASS", flush=True)
        raw._mmap.close()
        del raw

    container_bytes = args.output.stat().st_size
    container_crc32 = crc32_file(args.output)
    if args.upload_output is not None:
        transfer_path = args.upload_output
        transfer_bytes, transfer_crc32 = make_upload_image(
            args.output, transfer_path, args.transfer_alignment
        )
        print(
            f"UPLOAD IMAGE bytes={transfer_bytes} crc={transfer_crc32:08x} "
            f"alignment={args.transfer_alignment}",
            flush=True,
        )
    else:
        transfer_path = args.output
        transfer_bytes = container_bytes
        transfer_crc32 = container_crc32

    metadata = {
        "input": str(args.input.resolve()),
        "output": str(args.output.resolve()),
        "format": "ARGB1555",
        "width": WIDTH,
        "height": HEIGHT,
        "fps": args.fps,
        "frames": args.frames,
        "duration_seconds": args.frames / args.fps,
        "gop": args.gop,
        "keyframes": keyframes,
        "delta_frames": delta_frames,
        "frame_bytes": FRAME_BYTES,
        "raw_bytes": args.frames * FRAME_BYTES,
        "file_bytes": container_bytes,
        "file_mib": container_bytes / (1024 * 1024),
        "container_crc32": f"{container_crc32:08x}",
        "upload_output": str(transfer_path.resolve()),
        "transfer_bytes": transfer_bytes,
        "transfer_crc32": f"{transfer_crc32:08x}",
        "transfer_alignment": (
            args.transfer_alignment if args.upload_output is not None else 1
        ),
        "compressed_data_bytes": compressed_data_bytes,
        "compression_ratio": (
            args.frames * FRAME_BYTES / args.output.stat().st_size
        ),
        "maximum_payload_bytes": maximum_payload,
        "average_flash_mib_s": (
            container_bytes /
            (args.frames / args.fps) /
            (1024 * 1024)
        ),
        "data_offset": data_offset,
        "rotate_180": rotate_180,
        "elapsed_seconds": time.perf_counter() - start,
        "verification": verification,
    }
    metadata_path = args.json_output
    if metadata_path is None:
        metadata_path = args.output.with_suffix(args.output.suffix + ".json")
    metadata_path.parent.mkdir(parents=True, exist_ok=True)
    metadata_path.write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(metadata, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
