"""Dump CRI `@UTF` tables (ACB cue sheets, ACF) and list `AFS2` (AWB) wave banks. Reads files in place.

@UTF (big-endian; offsets after the first 8 bytes): "@UTF", u32 size, u16 version, u16 rows_off, u32 strings_off,
u32 data_off, u32 name (string offset), u16 n_columns, u16 row_width, u32 n_rows, then columns: u8 flags, u32 name,
[constant value if storage 0x30]. Storage (high nibble): 0x10 zero/none, 0x30 constant, 0x50 per row.
Type (low nibble): 0 u8, 1 s8, 2 u16, 3 s16, 4 u32, 5 s32, 6 u64, 7 s64, 8 f32, 9 f64, 0xA string, 0xB data (u32 off, u32 size).
AFS2: "AFS2", u8 version, u8 offset_size, u16 id_size, u32 count, u16 align, u16 subkey, ids[count], offsets[count+1].
Usage: py -3.14 -I acb_dump.py <file.acb|file.acf|file.awb> [--tables]   (--tables prints every table and row)
"""
import struct
import sys

TYPES = {0: ">B", 1: ">b", 2: ">H", 3: ">h", 4: ">I", 5: ">i", 6: ">Q", 7: ">q", 8: ">f", 9: ">d"}


def read_utf(b: bytes) -> dict:
    assert b[:4] == b"@UTF", b[:4]
    size, _ver, rows_off, str_off, data_off, name_off, ncol, row_w, nrows = struct.unpack_from(">IHHIIIHHI", b, 4)
    base = 8
    t = b[:base + size]

    def cstr(o: int) -> str:
        s = base + str_off + o
        return t[s:t.index(b"\0", s)].decode("utf-8", "replace")

    def value(kind: int, pos: int):
        if kind in TYPES:
            v = struct.unpack_from(TYPES[kind], t, pos)[0]
            return v, struct.calcsize(TYPES[kind])
        if kind == 0xA:
            return cstr(struct.unpack_from(">I", t, pos)[0]), 4
        if kind == 0xB:
            o, n = struct.unpack_from(">II", t, pos)
            return t[base + data_off + o:base + data_off + o + n], 8
        raise ValueError(f"@UTF column type {kind:#x}")

    cols, pos = [], 0x20
    for _ in range(ncol):
        flags, name = t[pos], struct.unpack_from(">I", t, pos + 1)[0]
        pos += 5
        const = None
        if flags & 0xF0 == 0x30:
            const, n = value(flags & 0x0F, pos)
            pos += n
        cols.append((cstr(name), flags, const))
    rows = []
    for r in range(nrows):
        pos, row = base + rows_off + r * row_w, {}
        for name, flags, const in cols:
            storage = flags & 0xF0
            if storage == 0x50:
                row[name], n = value(flags & 0x0F, pos)
                pos += n
            elif storage == 0x30:
                row[name] = const
            else:
                row[name] = None
        rows.append(row)
    return {"name": cstr(name_off), "rows": rows}


def read_afs2(b: bytes) -> list:
    assert b[:4] == b"AFS2", b[:4]
    offsz, idsz = b[5], b[6]
    count, align = struct.unpack_from("<IH", b, 8)
    ids = [int.from_bytes(b[16 + i * idsz:16 + (i + 1) * idsz], "little") for i in range(count)]
    o = 16 + count * idsz
    offs = [int.from_bytes(b[o + i * offsz:o + (i + 1) * offsz], "little") for i in range(count + 1)]
    out = []
    for i in range(count):
        start = (offs[i] + align - 1) // align * align
        out.append((ids[i], start, offs[i + 1] - start))
    return out


def show(table: dict, depth: int = 0, full: bool = False) -> None:
    pad = "  " * depth
    rows = table["rows"]
    print(f"{pad}table {table['name']}: {len(rows)} rows, columns: {', '.join(rows[0]) if rows else '-'}")
    for i, row in enumerate(rows):
        for k, v in row.items():
            if isinstance(v, bytes) and v[:4] == b"@UTF":
                print(f"{pad}  [{i}].{k}:")
                show(read_utf(v), depth + 2, full)
            elif isinstance(v, bytes) and v[:4] == b"AFS2":
                print(f"{pad}  [{i}].{k}: AFS2 with {len(read_afs2(v))} waves")
            elif full:
                print(f"{pad}  [{i}].{k} = {v if not isinstance(v, bytes) else f'<{len(v)} bytes>'}")


def summary(acb: dict) -> None:
    row = acb["rows"][0]
    names = read_utf(row["CueNameTable"])["rows"] if row.get("CueNameTable") else []
    waves = read_utf(row["WaveformTable"])["rows"] if row.get("WaveformTable") else []
    print(f"cue sheet {row.get('Name')}: {len(names)} cue names, {len(waves)} waveforms")
    for w in waves[:5]:
        print("  waveform", {k: w.get(k) for k in ("MemoryAwbId", "StreamAwbId", "Id", "Streaming", "EncodeType",
                                                  "NumChannels", "SamplingRate", "NumSamples")})
    for n in names[:10]:
        print("  cue", n.get("CueIndex"), n.get("CueName"))
    awb = row.get("AwbFile")
    if awb:
        print(f"  embedded AWB: {len(read_afs2(awb))} waves")
    stream = row.get("StreamAwbAfs2Header") or row.get("StreamAwbHash")
    if stream:
        print(f"  stream AWB reference: {len(stream) if isinstance(stream, bytes) else stream} bytes")


def main() -> None:
    b = open(sys.argv[1], "rb").read()
    if b[:4] == b"AFS2":
        waves = read_afs2(b)
        print(f"AFS2: {len(waves)} waves")
        for wid, start, size in waves[:10]:
            print(f"  id {wid} at {start:#x}, {size} bytes, magic {b[start:start + 4]!r}")
        return
    table = read_utf(b)
    if "--tables" in sys.argv or table["name"] != "Header":
        show(table, full="--tables" in sys.argv)
    else:
        summary(table)


if __name__ == "__main__":
    # self-check: a one-column, one-row table with a per-row u16 = 0x1234 and a string table holding "<NULL>", "T", "c"
    strings = b"<NULL>\0T\0c\0"
    cols = bytes([0x52]) + struct.pack(">I", 9)
    rows_off, str_off = 0x18 + len(cols), 0x18 + len(cols) + 2
    body = struct.pack(">HHIIIHHI", 1, rows_off, str_off, str_off + len(strings), 7, 1, 2, 1) + cols + b"\x12\x34" + strings
    t = read_utf(b"@UTF" + struct.pack(">I", len(body)) + body)
    assert t == {"name": "T", "rows": [{"c": 0x1234}]}, t
    main()
