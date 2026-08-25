#!/usr/bin/env python3
"""Render a deterministic 128x64 battle frame from the firmware's source bitmap."""

from __future__ import annotations

import argparse
import importlib.util
import struct
import zlib
from pathlib import Path

W, H = 128, 64
DIGITS = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "010", "010", "010"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
    "-": ("000", "000", "111", "000", "000"),
}


def png(path: Path, pixels: list[list[int]]) -> None:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))
    rows = b"".join(b"\0" + bytes(row) for row in pixels)
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 0, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    path.write_bytes(data)


def rect(pixels: list[list[int]], x: int, y: int, w: int, h: int, color: int,
         outline: int | None = None) -> None:
    for py in range(max(0, y), min(H, y + h)):
        for px in range(max(0, x), min(W, x + w)):
            pixels[py][px] = color
    if outline is not None:
        for px in range(x, x + w):
            if 0 <= px < W:
                if 0 <= y < H: pixels[y][px] = outline
                if 0 <= y + h - 1 < H: pixels[y + h - 1][px] = outline
        for py in range(y, y + h):
            if 0 <= py < H:
                if 0 <= x < W: pixels[py][x] = outline
                if 0 <= x + w - 1 < W: pixels[py][x + w - 1] = outline


def text(pixels: list[list[int]], value: str, box_x: int, box_y: int, box_w: int) -> None:
    width = len(value) * 4 - 1
    x0 = box_x + (box_w - width) // 2
    for char_index, char in enumerate(value):
        for y, row in enumerate(DIGITS[char]):
            for x, bit in enumerate(row):
                if bit == "1": pixels[box_y + 2 + y][x0 + char_index * 4 + x] = 255


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bitmaps", type=Path, required=True)
    parser.add_argument("--generator", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--left", type=int, default=87)
    parser.add_argument("--right", type=int, default=62)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("fighter_gen", args.generator)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    packed = module.render(args.bitmaps / "roll_f" / "004.bmp")
    pixels = [[0] * W for _ in range(H)]

    # Frameless HUD root: 1px screen margin; content padding top=0, bottom=1.
    rect(pixels, 1, 1, 126, 13, 0)
    rect(pixels, 2, 2, 23, 10, 0, 255)
    rect(pixels, 103, 2, 23, 10, 0, 255)
    rect(pixels, 26, 3, 34, 7, 0, 255)
    rect(pixels, 68, 3, 34, 7, 0, 255)
    left_width = max(0, min(100, args.left)) * 30 // 100
    right_width = max(0, min(100, args.right)) * 30 // 100
    rect(pixels, 28, 5, left_width, 3, 255)
    rect(pixels, 100 - right_width, 5, right_width, 3, 255)
    text(pixels, str(args.left), 2, 2, 23)
    text(pixels, str(args.right), 103, 2, 23)

    # Fighter mid frame 4: x=53 from 78 -> 35 across eight frames, y=38.
    stride = 7
    for y in range(26):
        for x in range(50):
            if packed[8 + y * stride + x // 8] & (1 << (7 - x % 8)):
                pixels[38 + y][53 + x] = 255
    png(args.output, pixels)


if __name__ == "__main__":
    main()
