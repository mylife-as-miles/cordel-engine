#!/usr/bin/env python3
"""Convert the native PPM readbacks into small lossless PNGs, using stdlib only."""
from pathlib import Path
import struct
import sys
import zlib


def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


for path in sorted(Path(sys.argv[1]).glob("*.ppm")):
    with path.open("rb") as stream:
        if stream.readline() != b"P6\n":
            raise SystemExit("Expected native P6 PPM")
        width, height = map(int, stream.readline().split())
        if stream.readline() != b"255\n":
            raise SystemExit("Expected 8-bit pixels")
        pixels = stream.read()
    if len(pixels) != width * height * 3:
        raise SystemExit("Invalid native readback length")
    filtered = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(filtered, 9)) + chunk(b"IEND", b""))
    path.with_suffix(".png").write_bytes(png)
    print(f"{path.name}: {width}x{height}, PNG {len(png)} bytes")
