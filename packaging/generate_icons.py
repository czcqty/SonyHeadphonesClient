#!/usr/bin/env python3
"""Generate and validate all platform icon files from the canonical SVG."""

from __future__ import annotations

import argparse
import io
import struct
from pathlib import Path


SIZES = (16, 24, 32, 48, 64, 128, 256, 512, 1024)
ICNS_TYPES = {
    16: b"icp4",
    32: b"icp5",
    64: b"icp6",
    128: b"ic07",
    256: b"ic08",
    512: b"ic09",
    1024: b"ic10",
}


def render(svg: Path, output_dir: Path) -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Generation requires Pillow") from exc
    try:
        import cairosvg
    except ImportError:
        cairosvg = None

    output_dir.mkdir(parents=True, exist_ok=True)
    images: dict[int, Image.Image] = {}
    fallback = output_dir / "app-icon-1024.png"
    if cairosvg is None and not fallback.exists():
        raise SystemExit(
            "Generation requires cairosvg, or a pre-rendered app-icon-1024.png")

    base = Image.open(fallback).convert("RGBA") if cairosvg is None else None
    for size in SIZES:
        if cairosvg is not None:
            png = cairosvg.svg2png(
                url=str(svg), output_width=size, output_height=size)
            image = Image.open(io.BytesIO(png)).convert("RGBA")
        else:
            image = base.resize((size, size), Image.Resampling.LANCZOS)
        image.save(output_dir / f"app-icon-{size}.png", optimize=True)
        images[size] = image

    images[1024].save(
        output_dir / "app-icon.ico",
        format="ICO",
        sizes=[(size, size) for size in SIZES if size <= 256],
    )

    chunks = []
    for size, type_code in ICNS_TYPES.items():
        payload = io.BytesIO()
        images[size].save(payload, format="PNG", optimize=True)
        data = payload.getvalue()
        chunks.append(type_code + struct.pack(">I", len(data) + 8) + data)
    body = b"".join(chunks)
    (output_dir / "app-icon.icns").write_bytes(
        b"icns" + struct.pack(">I", len(body) + 8) + body)


def validate(svg: Path, output_dir: Path) -> None:
    if not svg.exists() or "#364FA8" not in svg.read_text(encoding="utf-8"):
        raise SystemExit("Canonical SVG is missing or has the wrong primary color")

    for size in SIZES:
        path = output_dir / f"app-icon-{size}.png"
        data = path.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
            raise SystemExit(f"Invalid PNG header: {path}")
        width, height, _, color_type = struct.unpack(">IIBB", data[16:26])
        if (width, height) != (size, size) or color_type != 6:
            raise SystemExit(f"Invalid PNG dimensions or alpha mode: {path}")

    ico = (output_dir / "app-icon.ico").read_bytes()
    reserved, image_type, count = struct.unpack("<HHH", ico[:6])
    if (reserved, image_type) != (0, 1) or count < 7:
        raise SystemExit("ICO header or image count is invalid")
    ico_sizes = set()
    for index in range(count):
        width, height = struct.unpack("BB", ico[6 + index * 16:8 + index * 16])
        ico_sizes.add((width or 256, height or 256))
    expected = {(size, size) for size in SIZES if size <= 256}
    if not expected.issubset(ico_sizes):
        raise SystemExit("ICO does not contain every required size")

    icns = (output_dir / "app-icon.icns").read_bytes()
    if not icns.startswith(b"icns"):
        raise SystemExit("Invalid ICNS header")
    for type_code in ICNS_TYPES.values():
        if type_code not in icns:
            raise SystemExit(f"ICNS is missing {type_code.decode()}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    output_dir = root / "gui" / "assets" / "icons"
    svg = output_dir / "app-icon.svg"
    if not args.validate_only:
        render(svg, output_dir)
    validate(svg, output_dir)


if __name__ == "__main__":
    main()
