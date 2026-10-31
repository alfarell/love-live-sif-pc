"""Probe CRI Sofdec2 `.usm` movies: chunk census, stream header tables, first payload bytes. No decoding.

Chunk (big-endian): char[4] signature (CRID, @SFV video, @SFA audio, @ALP alpha, @SBT subtitles, @CUE),
u32 chunk size (after these 8 bytes), u8 ?, u8 payload offset (from byte 8), u16 padding size, u8 channel,
u8 ?, u8 ?, u8 type (0 data, 1 header table, 2 end/metadata, 3 seek). Header payloads are @UTF tables.
Usage: py -3.14 -I usm_probe.py <file.usm|dir> [...]   (summary per file; first file in detail)
"""
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I drops the script folder from sys.path
from acb_dump import read_utf  # noqa: E402


def chunks(b: bytes):
    pos = 0
    while pos + 0x20 <= len(b):
        sig = b[pos:pos + 4]
        size, _, off, pad, chno, _, _, ctype = struct.unpack_from(">IBBHBBBB", b, pos + 4)
        start, end = pos + 8 + off, pos + 8 + size - pad
        yield sig.decode("latin-1"), ctype, chno, b[start:end]
        pos += 8 + size


def probe(p: Path, detail: bool) -> dict:
    b = p.read_bytes()
    census, info, first = Counter(), {}, {}
    for sig, ctype, chno, payload in chunks(b):
        census[(sig, ctype)] += 1
        if ctype == 1 and payload[:4] == b"@UTF":
            t = read_utf(payload)
            if t["name"] in ("VIDEO_HDRINFO", "AUDIO_HDRINFO", "ALPHA_HDRINFO"):
                info[(sig, chno)] = t["rows"][0]
        if ctype == 0 and (sig, chno) not in first:
            first[(sig, chno)] = payload[:16]
    if detail:
        for k, v in sorted(census.items()):
            print(f"  {k[0]} type {k[1]}: {v} chunks")
        for k, row in info.items():
            print(f"  {k[0]} ch{k[1]} header:", {c: v for c, v in row.items() if not isinstance(v, bytes)})
        for k, v in first.items():
            print(f"  {k[0]} ch{k[1]} first data: {v.hex(' ')}")
    return info


def main() -> None:
    files = []
    for a in sys.argv[1:]:
        p = Path(a)
        files += sorted(p.rglob("*.usm")) if p.is_dir() else [p]
    codecs = Counter()
    for i, p in enumerate(files):
        info = probe(p, i == 0)
        v = next((r for (s, _), r in info.items() if s == "@SFV"), {})
        a = next((r for (s, _), r in info.items() if s == "@SFA"), {})
        alpha = any(s == "@ALP" for s, _ in info)
        key = (v.get("mpeg_codec"), v.get("width"), v.get("height"), a.get("audio_codec"), alpha)
        codecs[key] += 1
        if i == 0:
            print(p.name, "->", key)
    print(f"{len(files)} files; (video codec, width, height, audio codec, alpha): count")
    for k, n in codecs.most_common():
        print(f"  {k}: {n}")


if __name__ == "__main__":
    main()
