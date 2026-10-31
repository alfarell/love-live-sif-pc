"""Convert SIF `btx\\0` textures (ARC tags `tex`, `.btx`) to PNG. Needs Pillow (work\\venv, M2 decision).

btx header (M2, census of all 40,101 textures; see docs/formats/btx.md):
  0x00 "btx\\0", 0x04 u32 version (0x102 or 0x101), 0x08 u16 width, 0x0A u16 height, 0x0C u32 row pitch (bytes;
  block rows for BCn), 0x10 u8 format, 0x12 u8 mip count - 1, 0x18 u32 data offset, 0x20 u32 size, 0x24 u32 name
  offset (0x30). Data is linear (not GPU-tiled), mips smallest-last; cubemaps store 6 faces, each with its mip chain.
Formats: 0 BGRA8, 1 BGR8, 7 BC1, 9 BC3, 13 BC7, 16 8-bit single channel (written as grey).
Usage: work\\venv\\Scripts\\python.exe -I tex2png.py <out_dir> <file_or_dir> [...]   (all mips but level 0 skipped)
"""
import struct
import sys
from pathlib import Path

from PIL import Image

# format -> (Pillow decoder args or raw mode, bytes per 4x4 block or per pixel, is_block)
FORMATS = {0: ("BGRA", 4, False), 1: ("BGR", 3, False), 16: ("L", 1, False),
           7: (1, 8, True), 9: (3, 16, True), 13: (7, 16, True)}


def level_size(w: int, h: int, fmt: int) -> int:
    _, unit, block = FORMATS[fmt]
    return max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * unit if block else w * h * unit


def decode(b: bytes) -> Image.Image:
    assert b[:4] == b"btx\0", b[:4]
    w, h, pitch = struct.unpack_from("<HHI", b, 8)
    fmt, mips = b[0x10], b[0x12] + 1
    if fmt not in FORMATS:
        raise ValueError(f"btx format {fmt}")
    data = b[struct.unpack_from("<I", b, 0x18)[0]:]
    chain = sum(level_size(max(1, w >> i), max(1, h >> i), fmt) for i in range(mips))
    # shortcut: cubemaps are detected by size only (no header flag found yet); revisit if a non-cube ever has 6 chains
    faces = 6 if len(data) >= 6 * chain else 1
    codec, unit, block = FORMATS[fmt]
    out = Image.new("RGBA", (w * faces, h))
    for f in range(faces):
        # uncompressed rows may be padded (469-wide card art has a 470-pixel pitch)
        lvl = data[f * chain:f * chain + (level_size(w, h, fmt) if block else pitch * h)]
        if block:
            im = Image.frombytes("RGBA", (w, h), lvl, "bcn", codec)
        else:
            rows = b"".join(lvl[y * pitch:y * pitch + w * unit] for y in range(h)) if pitch != w * unit else lvl
            mode = {"BGRA": "RGBA", "BGR": "RGB"}.get(codec, codec)
            im = Image.frombytes(mode, (w, h), rows, "raw", codec).convert("RGBA")
        out.paste(im, (f * w, 0))
    return out


def main() -> None:
    out = Path(sys.argv[1])
    done = failed = 0
    for arg in sys.argv[2:]:
        src = Path(arg)
        files = [p for p in src.rglob("*") if p.suffix in (".tex", ".btx")] if src.is_dir() else [src]
        for p in files:
            rel = p.relative_to(src) if src.is_dir() else Path(p.name)
            dst = (out / (src.name if src.is_dir() else "") / rel).with_suffix(".png")
            try:
                img = decode(p.read_bytes())
            except Exception as e:  # keep going; list what does not decode
                print("FAIL", p, repr(e)[:100])
                failed += 1
                continue
            dst.parent.mkdir(parents=True, exist_ok=True)
            img.save(dst)
            done += 1
    print(f"{done} PNGs written, {failed} failed")


if __name__ == "__main__":
    # self-check: a 4x4 RGBA8 btx and a 4x4 BC1 block chain size
    hdr = bytearray(0x40)
    hdr[:4] = b"btx\0"
    struct.pack_into("<IHHI", hdr, 4, 0x102, 4, 4, 16)
    struct.pack_into("<I", hdr, 0x18, 0x40)
    img = decode(bytes(hdr) + bytes([10, 20, 30, 40]) * 16)
    assert img.size == (4, 4) and img.getpixel((3, 3)) == (30, 20, 10, 40)  # stored B, G, R, A
    assert level_size(8, 8, 7) == 32 and level_size(2, 2, 13) == 16
    main()
