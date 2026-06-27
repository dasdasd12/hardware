from __future__ import print_function

import os


TEST_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
LTDC_ASSET_DIR = os.path.join(TEST_ROOT, "assets", "ltdc")
FLASH_ASSET_DIR = os.path.join(TEST_ROOT, "assets", "flash")
WIDTH = 800
HEIGHT = 480
IMAGE_BYTES = WIDTH * HEIGHT
CLUT_ENTRIES = 256
CLUT_BYTES = CLUT_ENTRIES * 3

GRAY_RAW = os.path.join(LTDC_ASSET_DIR, "v5f_ltdc_gray_800x480.raw")
PALETTE_RAW = os.path.join(LTDC_ASSET_DIR, "v5f_ltdc_palette_800x480.raw")
PALETTE_CLUT = os.path.join(LTDC_ASSET_DIR, "v5f_ltdc_palette_800x480_clut.rgb")
GRAY_LZSS = os.path.join(FLASH_ASSET_DIR, "v5f_ltdc_gray_800x480_left.lzss")
PALETTE_LZSS = os.path.join(FLASH_ASSET_DIR, "v5f_ltdc_palette_800x480.lzss")
HEADER = os.path.join(FLASH_ASSET_DIR, "v5f_ltdc_flash_assets.h")


def read_binary(path):
    with open(path, "rb") as handle:
        return bytearray(handle.read())


def write_binary(path, data):
    with open(path, "wb") as handle:
        handle.write(data)


def fnv1a(data):
    value = 2166136261
    for byte in bytearray(data):
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def grayscale_clut():
    data = bytearray()
    for index in range(CLUT_ENTRIES):
        data.extend(bytearray([index, index, index]))
    return data


def left_filter(data):
    filtered = bytearray()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            index = y * WIDTH + x
            left = data[index - 1] if x != 0 else 0
            filtered.append((data[index] - left) & 0xFF)
    return filtered


def lzss_compress(data):
    window_size = 4095
    min_length = 3
    max_length = 18
    chain_limit = 64
    table = {}
    output = bytearray()
    position = 0
    bit = 0
    flags = 0
    flags_position = 0
    length = len(data)

    def add_position(pos):
        if pos + 2 >= length:
            return
        key = bytes(bytearray(data[pos:pos + 3]))
        chain = table.setdefault(key, [])
        chain.append(pos)
        if len(chain) > chain_limit:
            del chain[:-chain_limit]

    while position < length:
        if bit == 0:
            flags_position = len(output)
            output.append(0)
            flags = 0

        best_length = 0
        best_offset = 0
        if position + min_length <= length:
            key = bytes(bytearray(data[position:position + 3]))
            for previous in reversed(table.get(key, [])):
                offset = position - previous
                if offset <= 0:
                    continue
                if offset > window_size:
                    break

                match_length = min_length
                limit = min(max_length, length - position)
                while (match_length < limit and
                       data[previous + match_length] == data[position + match_length]):
                    match_length += 1

                if match_length > best_length:
                    best_length = match_length
                    best_offset = offset
                    if match_length == max_length:
                        break

        if best_length >= min_length:
            flags |= (1 << bit)
            token = ((best_length - min_length) << 12) | best_offset
            output.append(token & 0xFF)
            output.append((token >> 8) & 0xFF)
            for add in range(position, position + best_length):
                add_position(add)
            position += best_length
        else:
            output.append(data[position])
            add_position(position)
            position += 1

        bit += 1
        if bit == 8:
            output[flags_position] = flags
            bit = 0

    if bit != 0:
        output[flags_position] = flags

    return output


def checked_read(path, expected_size):
    data = read_binary(path)
    if len(data) != expected_size:
        raise RuntimeError("{0} is {1} bytes, expected {2}".format(path, len(data), expected_size))
    return data


def write_header(gray, gray_lzss, palette, palette_lzss, palette_clut, gray_clut):
    text = """#ifndef V5F_LTDC_FLASH_ASSETS_H
#define V5F_LTDC_FLASH_ASSETS_H

#include <stdint.h>

#define V5F_LTDC_FLASH_ASSET_WIDTH  800u
#define V5F_LTDC_FLASH_ASSET_HEIGHT 480u
#define V5F_LTDC_FLASH_ASSET_IMAGE_BYTES \\
    (V5F_LTDC_FLASH_ASSET_WIDTH * V5F_LTDC_FLASH_ASSET_HEIGHT)
#define V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES 256u
#define V5F_LTDC_FLASH_ASSET_CLUT_BYTES   (V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES * 3u)

#define V5F_LTDC_FLASH_GRAY_LZSS_BYTES    {gray_lzss_len}u
#define V5F_LTDC_FLASH_PALETTE_LZSS_BYTES {palette_lzss_len}u
#define V5F_LTDC_FLASH_GRAY_IMAGE_FNV     0x{gray_fnv:08X}u
#define V5F_LTDC_FLASH_GRAY_CLUT_FNV      0x{gray_clut_fnv:08X}u
#define V5F_LTDC_FLASH_PALETTE_IMAGE_FNV  0x{palette_fnv:08X}u
#define V5F_LTDC_FLASH_PALETTE_CLUT_FNV   0x{palette_clut_fnv:08X}u

extern const uint8_t v5f_ltdc_flash_gray_lzss[];
extern const uint8_t v5f_ltdc_flash_gray_lzss_end[];
extern const uint8_t v5f_ltdc_flash_palette_lzss[];
extern const uint8_t v5f_ltdc_flash_palette_lzss_end[];
extern const uint8_t v5f_ltdc_flash_palette_clut_rgb888[];
extern const uint8_t v5f_ltdc_flash_palette_clut_rgb888_end[];

#endif
""".format(
        gray_lzss_len=len(gray_lzss),
        palette_lzss_len=len(palette_lzss),
        gray_fnv=fnv1a(gray),
        gray_clut_fnv=fnv1a(gray_clut),
        palette_fnv=fnv1a(palette),
        palette_clut_fnv=fnv1a(palette_clut),
    )
    with open(HEADER, "w") as handle:
        handle.write(text)


def main():
    gray = checked_read(GRAY_RAW, IMAGE_BYTES)
    palette = checked_read(PALETTE_RAW, IMAGE_BYTES)
    palette_clut = checked_read(PALETTE_CLUT, CLUT_BYTES)
    gray_clut = grayscale_clut()
    gray_lzss = lzss_compress(left_filter(gray))
    palette_lzss = lzss_compress(palette)

    write_binary(GRAY_LZSS, gray_lzss)
    write_binary(PALETTE_LZSS, palette_lzss)
    write_header(gray, gray_lzss, palette, palette_lzss, palette_clut, gray_clut)

    print("gray lzss: {0} bytes".format(len(gray_lzss)))
    print("palette lzss: {0} bytes".format(len(palette_lzss)))


if __name__ == "__main__":
    main()
