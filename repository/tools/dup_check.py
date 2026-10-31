"""Find duplicate files between two folder trees, in place (read-only).

Candidates are files with the same base name and size; candidates are then confirmed with SHA-256.
Also reports same-name files whose size differs.
Usage: py -3.14 -I dup_check.py <rootA> <rootB> <out.json>
"""
import hashlib
import json
import os
import sys
from collections import defaultdict
from pathlib import Path


def walk(root: Path) -> dict:
    by_name = defaultdict(list)
    for dirpath, _dirs, files in os.walk(root):
        for n in files:
            p = Path(dirpath) / n
            by_name[n.lower()].append((p, p.stat().st_size))
    return by_name


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def main() -> None:
    a, b, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    wa, wb = walk(a), walk(b)
    same, differ, size_differs = [], [], []
    for name in sorted(set(wa) & set(wb)):
        for pa, sa in wa[name]:
            for pb, sb in wb[name]:
                rel = (pa.relative_to(a).as_posix(), pb.relative_to(b).as_posix())
                if sa != sb:
                    size_differs.append({"a": rel[0], "b": rel[1], "size_a": sa, "size_b": sb})
                elif sha256(pa) == sha256(pb):
                    same.append({"a": rel[0], "b": rel[1], "size": sa})
                else:
                    differ.append({"a": rel[0], "b": rel[1], "size": sa})
    res = {"a": str(a), "b": str(b), "identical": same, "same_size_different_content": differ, "size_differs": size_differs}
    out.write_text(json.dumps(res, indent=1), encoding="utf-8")
    print(f"identical {len(same)}, same size but different {len(differ)}, size differs {len(size_differs)}")


if __name__ == "__main__":
    main()
