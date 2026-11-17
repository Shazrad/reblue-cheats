#!/usr/bin/env python3
"""Extract Blue Dragon's .vso/.pso shaders from an IPK1 archive.

re:Blue's build recompiles the game's Xenos shaders ahead of time from the
files in reblue/assets/. With only default.xex there, the shader cache is
empty and the game renders a black screen. The shaders live zlib-compressed
in pack/!necessity.ipk.

IPK1 layout (little-endian):
  header (0x10): magic "IPK1", u32 alignment, u32 entry_count, u32 total_size
  entry  (0x60): char name[0x40], u32 flags (1 = zlib), u32 packed_size,
                 u32 offset, u32 unpacked_size, u32 timestamp, pad[0x0C]

Usage: extract_shaders.py <!necessity.ipk> <out_dir>
"""
import struct
import sys
import zlib
from pathlib import Path


def main():
    ipk, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    data = ipk.read_bytes()
    magic, _align, count, _total = struct.unpack_from("<4sIII", data, 0)
    if magic != b"IPK1":
        sys.exit(f"{ipk}: not an IPK1 archive")
    out_dir.mkdir(parents=True, exist_ok=True)
    written = 0
    for n in range(count):
        base = 0x10 + n * 0x60
        name = data[base:base + 0x40].split(b"\0", 1)[0].decode("ascii")
        flags, packed, offset, unpacked = struct.unpack_from("<IIII", data, base + 0x40)
        if not name.lower().endswith((".vso", ".pso")):
            continue
        blob = data[offset:offset + packed]
        if flags & 1:
            blob = zlib.decompress(blob)
        if len(blob) != unpacked:
            sys.exit(f"{name}: size {len(blob)} != expected {unpacked}")
        (out_dir / name.replace("\\", "/").rsplit("/", 1)[-1]).write_bytes(blob)
        written += 1
    print(f"extracted {written} shaders to {out_dir}")


if __name__ == "__main__":
    main()
