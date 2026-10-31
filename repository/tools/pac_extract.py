"""Extract SIF `.pac` archives (ARC\\0 / cmp\\0) to a folder under work/. Reads the game folder only.

Nested ARC entries become folders, cmp entries are decompressed; leaf payloads are written as files.
File names: the entry name, plus ".<tag>" when the name has no extension; nameless entries are "<index>.<tag>".
Usage: py -3.14 -I pac_extract.py <game_root> <out_dir> [--match SUBSTR ...]
  --match keeps only archives whose path (relative to game_root) contains one of the substrings.
"""
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I drops the script folder from sys.path
from pac_list import parse_arc, unpack_cmp  # noqa: E402


def safe(name: str) -> str:
    name = re.sub(r"[^A-Za-z0-9._@+-]", "_", name)
    return name if name.strip(".") else "_" + name


def extract(b: bytes, out: Path, counts: list) -> None:
    if b[:4] == b"cmp\0":
        b = unpack_cmp(b)
    if b[:4] != b"ARC\0":
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(b)
        counts[0] += 1
        counts[1] += len(b)
        return
    used = set()
    for i, (tag, name, off, size, _hash) in enumerate(parse_arc(b)):
        payload = b[off:off + size]
        if name:
            fname = safe(name if "." in name else f"{name}.{tag}")
        else:
            fname = safe(f"{i:04d}.{tag}")
        if fname.lower() in used:  # Windows paths are case-insensitive
            fname = safe(f"{i:04d}_{fname}")
        used.add(fname.lower())
        extract(payload, out / fname, counts)


def main() -> None:
    root, out = Path(sys.argv[1]), Path(sys.argv[2])
    match = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == "--match"]
    counts = [0, 0]
    archives = 0
    for dirpath, _, names in os.walk(root):
        for n in sorted(names):
            if not n.lower().endswith(".pac"):
                continue
            rel = (Path(dirpath) / n).relative_to(root).as_posix()
            if match and not any(m in rel for m in match):
                continue
            extract((root / rel).read_bytes(), out / rel, counts)
            archives += 1
    print(f"{archives} archives, {counts[0]} files, {counts[1]:,} bytes written to {out}")


if __name__ == "__main__":
    assert safe("a/../b") == "a_.._b" and safe("..") == "_.." and safe("mbc_0001_20_T.tex") == "mbc_0001_20_T.tex"
    main()
