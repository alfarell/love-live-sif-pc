"""Shader census for SIF: every `SHP` program in the shader archives, deduplicated, optionally decoded with gcn.py.

Layout (M2): shader/ps4/sh_custom.pac -> bin <material> -> bin <material> -> SHG <material>-<permutation bits>
-> one SARC (the permutation name, padded) + SHP programs. Each SHP holds a PS4 "Shdr" header (pt-pc fsop.py
FILE_HEADER: magic, u16 major, u16 minor, u8 Gnm type, u8 header dwords, ...) and an "OrbShdr" v7 footer
(BINARY_INFO <7sBIBBBBQI; code length = info_bits >> 8, code directly before the footer).
Usage: py -3.14 -I shader_stats.py <game_root> [--decode [--opcodes shadPS4/.../opcodes.h]] [--csv out.csv]
  --decode disassembles each unique program with gcn.py (reads shadPS4's opcodes.h at run time) and counts failures.
"""
import csv
import hashlib
import os
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I drops the script folder from sys.path
from pac_list import parse_arc, unpack_cmp  # noqa: E402

GNM_TYPES = {1: "vs", 2: "ps", 3: "gs", 4: "cs", 5: "es", 6: "ls", 7: "hs", 8: "cs_vs"}  # pt-pc fsop.py
BINARY_INFO = struct.Struct("<7sBIBBBBQI")


def programs(b: bytes, path: str, out: list) -> None:
    if b[:4] == b"cmp\0":
        b = unpack_cmp(b)
    if b[:4] != b"ARC\0":
        return
    for tag, name, off, size, _ in parse_arc(b):
        payload = b[off:off + size]
        if tag == "SHP":
            out.append((path, payload))
        else:
            programs(payload, f"{path}/{name or tag}", out)


def describe(blob: bytes) -> dict:
    hdr = blob.find(b"Shdr")
    foot = blob.rfind(b"OrbShdr")
    assert hdr >= 0 and foot > hdr, (hdr, foot)
    _, version, info_bits, *_rest, info_hash, crc = BINARY_INFO.unpack_from(blob, foot)
    length = info_bits >> 8
    code = blob[foot - length:foot]
    return {"gnm": GNM_TYPES.get(blob[hdr + 8], str(blob[hdr + 8])), "version": version, "length": length,
            "footer_hash": f"{info_hash:016x}", "code_sha1": hashlib.sha1(code).hexdigest(), "code": code}


def main() -> None:
    root = Path(sys.argv[1])
    found = []
    for dirpath, _, names in os.walk(root / "shader"):
        for n in sorted(names):
            if n.lower().endswith(".pac"):
                p = Path(dirpath) / n
                programs(p.read_bytes(), p.relative_to(root).as_posix(), found)
    rows = [(path, describe(blob)) for path, blob in found]
    unique = {}
    for path, d in rows:
        unique.setdefault(d["code_sha1"], (path, d))
    per_material = defaultdict(set)
    for path, d in rows:
        per_material[path.split("/")[3]].add(d["code_sha1"])  # shader/ps4/sh_custom.pac/<material>/...
    print(f"{len(rows)} SHP programs, {len(unique)} unique code blobs, "
          f"{len({d['footer_hash'] for _, d in rows})} unique footer hashes, "
          f"{len({p for p, _ in rows})} permutations")
    print("by Gnm type (all / unique):",
          {t: (sum(d['gnm'] == t for _, d in rows), sum(d['gnm'] == t for _, d in unique.values()))
           for t in sorted({d['gnm'] for _, d in rows})})
    print("unique code per material:", {m: len(s) for m, s in sorted(per_material.items())})
    print("footer versions:", Counter(d["version"] for _, d in rows))
    if "--decode" in sys.argv:
        import gcn
        ops = gcn.load_opcodes(sys.argv[sys.argv.index("--opcodes") + 1] if "--opcodes" in sys.argv else None)
        bad = 0
        for path, d in unique.values():
            insts = gcn.decode(d["code"], ops)
            text = "\n".join(gcn.format_inst(i) for i in insts)
            if not insts or insts[-1].name != "S_ENDPGM" or ".word" in text or "UNHANDLED" in text:
                bad += 1
        print(f"decode: {len(unique) - bad}/{len(unique)} unique programs decode cleanly to s_endpgm")
    if "--csv" in sys.argv:
        with open(sys.argv[sys.argv.index("--csv") + 1], "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["permutation", "gnm", "length", "footer_hash", "code_sha1"])
            for path, d in rows:
                w.writerow([path, d["gnm"], d["length"], d["footer_hash"], d["code_sha1"]])


if __name__ == "__main__":
    main()
