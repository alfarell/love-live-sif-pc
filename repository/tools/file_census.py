"""Inventory a folder tree in place: sizes per folder, extension census, magic bytes.

Read-only: walks the tree and reads only the first 16 bytes of each file (no hashing, no copying).
Usage: py -3.14 -I file_census.py <root> <out.json> [--depth N]
Output JSON: per-file list (path, size, head hex), per-folder totals to depth N, per-extension and per-magic census.
"""
import json
import os
import sys
from collections import defaultdict
from pathlib import Path


def main() -> None:
    root, out = Path(sys.argv[1]), Path(sys.argv[2])
    depth = int(sys.argv[sys.argv.index("--depth") + 1]) if "--depth" in sys.argv else 2
    files = []
    folders = defaultdict(lambda: [0, 0])
    exts = defaultdict(lambda: [0, 0])
    magics = defaultdict(lambda: {"files": 0, "bytes": 0, "exts": defaultdict(int), "example": ""})
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            p = Path(dirpath) / name
            rel = p.relative_to(root).as_posix()
            size = p.stat().st_size
            with open(p, "rb") as f:
                head = f.read(16)
            ext = p.suffix.lower() or "(none)"
            files.append({"path": rel, "size": size, "head": head.hex()})
            parts = rel.split("/")[:-1]
            for d in range(0, min(depth, len(parts)) + 1):
                key = "/".join(parts[:d]) or "."
                folders[key][0] += 1
                folders[key][1] += size
            exts[ext][0] += 1
            exts[ext][1] += size
            m = magics[head[:4].hex()]
            m["files"] += 1
            m["bytes"] += size
            m["exts"][ext] += 1
            m["example"] = m["example"] or rel
    total = sum(f["size"] for f in files)
    assert folders["."] == [len(files), total]
    result = {
        "root": str(root),
        "files": len(files),
        "bytes": total,
        "by_folder": {k: {"files": v[0], "bytes": v[1]} for k, v in sorted(folders.items())},
        "by_extension": {k: {"files": v[0], "bytes": v[1]} for k, v in sorted(exts.items(), key=lambda kv: -kv[1][1])},
        "by_magic4": {k: {"ascii": bytes.fromhex(k).decode("latin-1").translate({i: "." for i in range(32)}),
                          "files": v["files"], "bytes": v["bytes"], "exts": dict(v["exts"]), "example": v["example"]}
                      for k, v in sorted(magics.items(), key=lambda kv: -kv[1]["bytes"])},
        "file_list": files,
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=1), encoding="utf-8")
    print(f"{len(files)} files, {total:,} bytes")
    for k, v in result["by_folder"].items():
        if k.count("/") < depth:
            print(f"{v['bytes']:>15,} {v['files']:>7}  {k}")


if __name__ == "__main__":
    main()
