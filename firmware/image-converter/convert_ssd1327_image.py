#!/usr/bin/env python3
"""Convert images to 128x128 SSD1327-friendly Arduino arrays."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps


WIDTH = 128
HEIGHT = 128


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Input PNG/JPG image")
    parser.add_argument("--output", "-o", type=Path, required=True, help="Output .h file")
    parser.add_argument("--symbol", "-s", required=True, help="C array symbol name")
    parser.add_argument(
        "--mode",
        choices=("mono", "gray4"),
        default="mono",
        help="mono matches display.drawBitmap(); gray4 stores two 4-bit pixels per byte",
    )
    parser.add_argument(
        "--fit",
        choices=("cover", "contain"),
        default="cover",
        help="cover crops to square; contain letterboxes inside 128x128",
    )
    parser.add_argument("--invert", action="store_true", help="Invert luminance before export")
    parser.add_argument("--contrast", type=float, default=1.18, help="Contrast multiplier")
    parser.add_argument("--brightness", type=float, default=1.0, help="Brightness multiplier")
    parser.add_argument("--sharpness", type=float, default=1.25, help="Sharpness multiplier")
    parser.add_argument(
        "--threshold",
        type=int,
        default=128,
        help="Mono threshold used when --dither none",
    )
    parser.add_argument(
        "--dither",
        choices=("floyd", "none"),
        default="floyd",
        help="Dither method for mono output",
    )
    return parser.parse_args()


def c_symbol(value: str) -> str:
    symbol = re.sub(r"\W+", "_", value.strip())
    if not symbol or symbol[0].isdigit():
        symbol = f"img_{symbol}"
    return symbol


def prepare_image(path: Path, fit: str, invert: bool, contrast: float, brightness: float, sharpness: float) -> Image.Image:
    img = Image.open(path).convert("RGBA")

    if fit == "cover":
        img = ImageOps.fit(img, (WIDTH, HEIGHT), method=Image.Resampling.LANCZOS, centering=(0.5, 0.5))
    else:
        img.thumbnail((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 255))
        canvas.alpha_composite(img, ((WIDTH - img.width) // 2, (HEIGHT - img.height) // 2))
        img = canvas

    background = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 255))
    background.alpha_composite(img)
    gray = background.convert("L")
    gray = ImageEnhance.Contrast(gray).enhance(contrast)
    gray = ImageEnhance.Brightness(gray).enhance(brightness)
    gray = ImageEnhance.Sharpness(gray).enhance(sharpness)
    if invert:
        gray = ImageOps.invert(gray)
    return gray


def export_mono(gray: Image.Image, threshold: int, dither: str) -> bytes:
    if dither == "floyd":
        bw = gray.convert("1", dither=Image.Dither.FLOYDSTEINBERG)
    else:
        bw = gray.point(lambda p: 255 if p >= threshold else 0, mode="1")

    data = bytearray()
    pixels = bw.load()
    for y in range(HEIGHT):
        for byte_x in range(0, WIDTH, 8):
            value = 0
            for bit in range(8):
                if pixels[byte_x + bit, y]:
                    value |= 0x80 >> bit
            data.append(value)
    return bytes(data)


def export_gray4(gray: Image.Image) -> bytes:
    data = bytearray()
    pixels = gray.load()
    for y in range(HEIGHT):
        for x in range(0, WIDTH, 2):
            hi = pixels[x, y] >> 4
            lo = pixels[x + 1, y] >> 4
            data.append((hi << 4) | lo)
    return bytes(data)


def write_header(output: Path, symbol: str, mode: str, data: bytes) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    bytes_per_line = 16
    lines = []
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i : i + bytes_per_line]
        lines.append("\t" + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")

    comment = "1-bit dithered bitmap for Adafruit_GFX drawBitmap" if mode == "mono" else "4-bit SSD1327 grayscale, two pixels per byte"
    output.write_text(
        "\n".join(
            [
                "#pragma once",
                "#include <Arduino.h>",
                "",
                f"// {comment}",
                f"// Size: {WIDTH}x{HEIGHT}, bytes: {len(data)}",
                f"const unsigned char {symbol}[] PROGMEM = {{",
                *lines,
                "};",
                "",
            ]
        ),
        encoding="utf-8",
    )


def main() -> None:
    args = parse_args()
    symbol = c_symbol(args.symbol)
    gray = prepare_image(args.input, args.fit, args.invert, args.contrast, args.brightness, args.sharpness)
    data = export_mono(gray, args.threshold, args.dither) if args.mode == "mono" else export_gray4(gray)
    write_header(args.output, symbol, args.mode, data)
    print(f"Wrote {args.mode} {WIDTH}x{HEIGHT} image: {args.output} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
