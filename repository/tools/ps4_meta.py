"""Read PS4 metadata in place: param.sfo files and PKG headers/entry tables (no extraction, no decryption).

Usage:
  py -3.14 -I ps4_meta.py sfo <param.sfo>
  py -3.14 -I ps4_meta.py pkg <file.pkg> [--json out.json]   (also parses the PKG's plaintext param.sfo entry in memory)
  py -3.14 -I ps4_meta.py pkgdir <folder> <out.json>         (pkg header summary for every .pkg in a folder)
PKG layout follows pt-pc docs/formats/pkg.md (header big-endian, 32-byte entries; flags1 bit 31 = encrypted).
Only header fields, entry names/sizes/flags and param.sfo values are read; encrypted entries are never read.
"""
import json
import struct
import sys
from pathlib import Path

SFO_FMT = {0x0004: "utf8-special", 0x0204: "utf8", 0x0404: "u32"}
ENTRY_NAMES = {0x0001: "digests", 0x0010: "entry_keys", 0x0020: "image_key", 0x0080: "general_digests",
               0x0100: "metas", 0x0200: "entry_names"}


def parse_sfo(data: bytes) -> dict:
    magic, version, key_off, data_off, count = struct.unpack_from("<4sIIII", data, 0)
    assert magic == b"\0PSF", magic
    out = {}
    for i in range(count):
        k, fmt, length, _max, d = struct.unpack_from("<HHIII", data, 20 + 16 * i)
        key = data[key_off + k:data.index(b"\0", key_off + k)].decode()
        raw = data[data_off + d:data_off + d + length]
        out[key] = struct.unpack("<I", raw)[0] if fmt == 0x0404 else raw.rstrip(b"\0").decode("utf-8", "replace")
    return out


def parse_pkg(path: Path, with_entries: bool = True) -> dict:
    with open(path, "rb") as f:
        h = f.read(0x1000)
        u32 = lambda o: struct.unpack_from(">I", h, o)[0]
        u64 = lambda o: struct.unpack_from(">Q", h, o)[0]
        if h[:4] != b"\x7fCNT":
            return {"file": path.name, "size": path.stat().st_size, "magic": h[:4].hex(), "error": "not a PKG"}
        info = {
            "file": path.name, "size": path.stat().st_size, "pkg_type": hex(u32(0x4)),
            "file_count": u32(0xC), "entry_count": u32(0x10), "entry_table_offset": hex(u32(0x18)),
            "body_offset": hex(u64(0x20)), "body_size": hex(u64(0x28)),
            "content_id": h[0x40:0x64].rstrip(b"\0").decode(), "drm_type": hex(u32(0x70)),
            "content_type": hex(u32(0x74)), "content_flags": hex(u32(0x78)), "version_date": hex(u32(0x80)),
            "pfs_image_count": u32(0x404), "pfs_image_flags": hex(u64(0x408)), "pfs_image_offset": hex(u64(0x410)),
            "pfs_image_size": hex(u64(0x418)), "pkg_size": hex(u64(0x430)),
        }
        if not with_entries:
            return info
        f.seek(u32(0x18))
        raw = f.read(32 * u32(0x10))
        entries = [dict(zip(("id", "name_offset", "flags1", "flags2", "offset", "size"), struct.unpack_from(">6I", raw, 32 * i)))
                   for i in range(u32(0x10))]
        names = b""
        for e in entries:
            if e["id"] == 0x0200:
                f.seek(e["offset"])
                names = f.read(e["size"])
        for e in entries:
            if e["id"] in ENTRY_NAMES:
                e["name"] = ENTRY_NAMES[e["id"]]
            elif names and e["name_offset"] < len(names):
                e["name"] = names[e["name_offset"]:names.index(b"\0", e["name_offset"])].decode()
            e["encrypted"] = bool(e["flags1"] & 0x80000000)
            e["key_slot"] = (e["flags2"] >> 12) & 0xF
            if e["id"] == 0x1000 and not e["encrypted"]:
                f.seek(e["offset"])
                info["param_sfo"] = parse_sfo(f.read(e["size"]))
        info["entries"] = [{**e, "id": f"{e['id']:04X}", "flags1": f"{e['flags1']:08X}", "flags2": f"{e['flags2']:X}",
                            "offset": hex(e["offset"])} for e in entries]
        po = u64(0x410)
        f.seek(po)
        pfs = f.read(0x40)
        version, magic, _id, _fmode, _clean, read_only, _rsv, mode = struct.unpack_from("<QQQBBBBH", pfs, 0)
        assert magic == 20130315, magic
        # mode bits per pt-pc docs/formats/pkg.md: 0x0D = signed + encrypted (+ an always-set bit)
        info["pfs_header"] = {"version": version, "magic": magic, "read_only": read_only, "mode": hex(mode),
                              "block_size": hex(struct.unpack_from("<I", pfs, 0x20)[0])}
    return info


def main() -> None:
    cmd = sys.argv[1]
    if cmd == "sfo":
        print(json.dumps(parse_sfo(Path(sys.argv[2]).read_bytes()), indent=1, ensure_ascii=False))
    elif cmd == "pkg":
        info = parse_pkg(Path(sys.argv[2]))
        text = json.dumps(info, indent=1, ensure_ascii=False)
        if "--json" in sys.argv:
            Path(sys.argv[sys.argv.index("--json") + 1]).write_text(text, encoding="utf-8")
        print(text)
    elif cmd == "pkgdir":
        rows = [parse_pkg(p) for p in sorted(Path(sys.argv[2]).glob("*.pkg"))]
        Path(sys.argv[3]).write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")
        print(len(rows), "pkgs")


if __name__ == "__main__":
    main()
