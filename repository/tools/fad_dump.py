"""Dump SIF `.fad` 2D animations (ui/ui_ingame.pac/anim.pac): header, record tags and a per-tag summary.

Layout as decoded so far (docs/formats/fad.md): a 24-byte header, then records of
u8 tag, u8 0xAA, u16 record length in 16-bit words (header included), payload.
Usage: py -3.14 -I fad_dump.py <file.fad> [--all]   (--all prints every record, otherwise a tag summary)
"""
import struct
import sys
from collections import Counter
from pathlib import Path


def records(b: bytes):
    off = 24
    while off + 4 <= len(b):
        tag, marker, words = b[off], b[off + 1], struct.unpack_from("<H", b, off + 2)[0]
        if marker != 0xAA or words < 2:
            raise ValueError(f"bad record at 0x{off:x}: {b[off:off + 4].hex()}")
        yield off, tag, b[off + 4:off + words * 2]
        off += words * 2


def describe(tag: int, p: bytes) -> str:
    if tag == 0x09:
        n = struct.unpack_from("<H", p, 0)[0]
        out, o = [], 2
        for _ in range(n):
            i = struct.unpack_from("<H", p, o)[0]
            e = p.index(b"\0", o + 2)
            out.append(f"{i}:{p[o + 2:e].decode()}")
            o = (e + 2) & ~1  # names are padded to an even length
        return "textures " + ", ".join(out)
    if tag == 0x05 and len(p) >= 28:
        d, f = struct.unpack_from("<HH", p, 0)
        m = struct.unpack_from("<6f", p, 4)
        return f"matrix depth {d} flag {f} " + " ".join(f"{v:g}" for v in m)
    if tag == 0x06 and len(p) >= 20:
        v = struct.unpack_from("<HH8h", p, 0)
        return f"colour depth {v[0]} flags 0x{v[1]:x} mul {v[2:6]} add {v[6:10]}"
    return p.hex(" ", 2)


def main() -> None:
    path = Path(sys.argv[1])
    b = path.read_bytes()
    ver, unk, x0, x1, y0, y1, fps, frames = struct.unpack_from("<HHiiiiHH", b, 0)
    print(f"{path.name}: {len(b)} bytes, version {ver}, header u16 {unk}, rect ({x0},{y0})-({x1},{y1}), "
          f"{fps} fps, {frames} frames")
    counts = Counter()
    shows = 0
    for off, tag, p in records(b):
        counts[tag] += 1
        shows += tag == 0x08
        if "--all" in sys.argv or tag not in (0x05, 0x06, 0x08):
            print(f"  0x{off:05x} tag {tag:02x} [{len(p):3d}] frame {shows:3d}: {describe(tag, p)}")
    print("  tags: " + ", ".join(f"{t:02x}x{n}" for t, n in sorted(counts.items())) + f"; show-frame records {shows}")


if __name__ == "__main__":
    main()
