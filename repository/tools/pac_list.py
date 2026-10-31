"""List SIF `.pac` archives (ARC\\0 and cmp\\0) in place: entry tags, names, sizes, inner magic. No extraction.

Layout (from hex dumps, Part 9 and M1; checked by the asserts below; see repository/docs/formats/arc.md):
  ARC: 0x00 "ARC\\0", u16 BE 0x0100, u16 count, u16 count, 0x10 name[32], 0x30 u32 offsets[count],
       then entry records aligned to 0x20: tag[4], u16 index, u16 index, u32 hash, u16 ?, u16 name_len (incl. NUL),
       u32 0, name[name_len], record padded to 0x20. Entry data at offsets[i]: u32 size, u16 header_len, u16 flags,
       payload at offsets[i] + header_len (0x80; 4 in the 4 nameless message.pac entries).
       flags 1 exactly when the payload is cmp-wrapped (8,248 of 8,248), else 0.
  cmp: "cmp\\0", method[4] ("zlib" | "lzma"), u32 packed, u32 unpacked, stream. The lzma stream is LZMA-alone
       without its 8-byte size field (5 props bytes, then data). Decompressed in memory only.
Usage: py -3.14 -I pac_list.py <root> <out.csv> [--max-depth N]
"""
import csv
import lzma
import os
import struct
import sys
import zlib
from collections import Counter
from pathlib import Path


def magic_of(b: bytes) -> str:
    head = b[:4]
    return head.decode("latin-1") if all(32 <= c < 127 for c in head) else head.hex()


def unpack_cmp(b: bytes) -> bytes:
    method, packed, unpacked = b[4:8], *struct.unpack_from("<II", b, 8)
    if method == b"zlib":
        out = zlib.decompress(b[16:16 + packed])
    elif method == b"lzma":
        s = b[16:16 + packed]
        out = lzma.LZMADecompressor(lzma.FORMAT_ALONE).decompress(s[:5] + struct.pack("<Q", unpacked) + s[5:])
    else:
        raise ValueError(f"cmp method {method!r}")
    assert len(out) == unpacked, (len(out), unpacked)
    return out


def parse_arc(b: bytes):
    assert b[:4] == b"ARC\0" and b[4:6] == b"\x00\x01", b[:8]
    count = struct.unpack_from("<H", b, 6)[0]
    assert struct.unpack_from("<H", b, 8)[0] == count
    offsets = list(struct.unpack_from(f"<{count}I", b, 0x30))
    pos = (0x30 + 4 * count + 0x1F) & ~0x1F
    entries = []
    for i in range(count):
        rec = pos
        tag = b[pos:pos + 4].rstrip(b"\0").decode("latin-1")
        name_len = struct.unpack_from("<H", b, pos + 0x0E)[0]
        name = b[pos + 0x14:pos + 0x14 + name_len].split(b"\0")[0].decode("utf-8", "replace")
        pos = (pos + 0x14 + max(name_len, 1) + 0x1F) & ~0x1F
        off = offsets[i]
        size, hdr, _flags = struct.unpack_from("<IHH", b, off)
        assert off + hdr + size <= len(b), (name, hex(off), hex(hdr), size)
        entries.append((tag, name, off + hdr, size, b[rec + 8:rec + 12][::-1].hex()))
    if not offsets:  # empty nested archives exist (e.g. inside chr/c11/c11.pac)
        return entries
    assert all(a < c for a, c in zip(offsets, offsets[1:])), "offsets not ascending"
    assert offsets[0] >= pos and offsets[-1] < len(b), (hex(offsets[0]), hex(pos))
    return entries


def walk(b: bytes, path: str, depth: int, max_depth: int, rows: list, file: str):
    if b[:4] == b"cmp\0":
        method = b[4:8].decode()
        b = unpack_cmp(b)
        rows.append([file, depth, path, "cmp", method, len(b), magic_of(b)])
    if b[:4] != b"ARC\0":
        return
    for tag, name, off, size, name_hash in parse_arc(b):
        payload = b[off:off + size]
        inner = magic_of(payload)
        child = f"{path}/{name or tag}"
        rows.append([file, depth, child, tag, name, size, inner, name_hash])
        if depth < max_depth and payload[:4] in (b"ARC\0", b"cmp\0"):
            walk(payload, child, depth + 1, max_depth, rows, file)


def main() -> None:
    root, out = Path(sys.argv[1]), Path(sys.argv[2])
    max_depth = int(sys.argv[sys.argv.index("--max-depth") + 1]) if "--max-depth" in sys.argv else 3
    rows, errors = [], []
    for dirpath, _, names in os.walk(root):
        for n in sorted(names):
            if not n.lower().endswith(".pac"):
                continue
            p = Path(dirpath) / n
            rel = p.relative_to(root).as_posix()
            try:
                walk(p.read_bytes(), rel, 0, max_depth, rows, rel)
            except Exception as e:  # keep going; record the file that does not fit the hypothesis
                errors.append((rel, repr(e)[:120]))
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["file", "depth", "path", "tag", "name", "size", "inner_magic", "hash"])
        w.writerows(rows)
    tags = Counter((r[3], r[6]) for r in rows)
    print(f"{len(rows)} entries, {len(errors)} files failed")
    for e in errors[:10]:
        print("  FAIL", *e)
    for (tag, inner), c in tags.most_common(60):
        print(f"{c:7}  tag={tag:6} inner={inner}")


if __name__ == "__main__":
    main()
