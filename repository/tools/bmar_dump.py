"""Read SIF `BMAR104` model files (ARC tag `mdl`; "BladeModelPac"), list them, export OBJ, render a preview PNG.

Layout (M2 follow-up; see docs/formats/bmar.md):
  "BMAR104\\0" + an ARC table with parts BML (scene: nodes, meshes, materials), BMD, BMA (animation), BMT (textures).
  BML/BMA are chunk lists: char[4] tag, u32 body size, u32 0x00040000, body, padded to 4; last chunks POF0, then tag 0.
  Pointers inside bodies are u64 offsets from the part start, pointing at DATA chunk bodies.
  NODE: +0 name*, +8 parent name*, +0x10 f32[3] translate, +0x1C f32[3] scale, +0x28 f32[3] rotate (radians,
        R = Rz*Ry*Rx), +0x60 s32 parent index. Node 0 "bml_axisNode" turns Z-up into Y-up and is skipped here.
  MESH: +0 name*, +8 vertex buffer*, +0x10 u16 index buffer* (triangle list), +0x48 u32 vertex format bits,
        +0x50 u32 index count, +0x54 u32 vertex count, +0x5A u16 stride.  DRAW: u32 node, u32 mesh, u32 material.
  MATE: +0 name*, +8 texture name*.  TXTR: +0 texture name*.
Usage: py -3.14 -I bmar_dump.py <file.mdl> [--obj out.obj] [--preview out.png [--top]]   (--preview needs Pillow:
       work\\venv; front view by default, --top for a floor plan)
"""
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I drops the script folder from sys.path
from pac_list import parse_arc  # noqa: E402

# vertex format bit -> (attribute, encoding, size in bytes), in storage order. Sizes solved from all 22 format/stride
# pairs in the game (M2); the order is checked on the samples in docs/formats/bmar.md, partly Hypothesis for rare bits.
ATTRS = [(0x00000001, "position", "f32x3", 12), (0x40000000, "skin", "f32x4 weights + u8x4 bones + 20 bytes", 40),
         (0x00000004, "normal", "f32x3", 12), (0x00000008, "normal", "f16x4", 8), (0x00000020, "color", "u8x4", 4),
         (0x00000100, "uv0", "f32x2", 8), (0x00000400, "uv0", "f16x2", 4),
         (0x00001000, "uv1", "f32x2", 8), (0x00004000, "uv1", "f16x2", 4),
         (0x00040000, "tangent", "f32x3", 12), (0x00080000, "tangent", "f16x4", 8),
         (0x00200000, "binormal", "f32x3", 12), (0x00400000, "binormal", "f16x4", 8),
         (0x10000000, "extra16", "f32x4", 16)]
KNOWN_BITS = sum(bit for bit, *_ in ATTRS) | 0x00010000  # 0x10000 is always set together with 0x40000000 (skin)


def layout(fmt: int, extra4: int = 0) -> dict:
    """Attribute offsets for a format; extra4 = MESH +0x4C, each adds 4 bytes (placed last: Hypothesis)."""
    offs, pos = {}, 0
    for bit, name, enc, size in ATTRS:
        if fmt & bit:
            offs[name], offs[name + "_enc"] = pos, enc
            pos += size
    offs["stride"] = pos + 4 * extra4
    return offs


def chunks(p: bytes):
    pos = 0
    while pos + 12 <= len(p):
        tag, size = p[pos:pos + 4], struct.unpack_from("<I", p, pos + 4)[0]
        if tag == b"\0\0\0\0":
            return
        yield tag.decode("latin-1").rstrip("\0"), p[pos + 12:pos + 12 + size]
        pos = (pos + 12 + size + 3) & ~3


def parse(b: bytes) -> dict:
    assert b[:8] == b"BMAR104\0", b[:8]
    parts = {tag: b[8 + off:8 + off + size] for tag, _n, off, size, _h in parse_arc(b[8:])}
    p = parts["BML"]

    def cstr(o: int) -> str:
        return p[o:p.index(b"\0", o)].decode("utf-8", "replace")

    model = {"parts": {k: len(v) for k, v in parts.items()}, "nodes": [], "meshes": [], "draws": [],
             "materials": [], "textures": []}
    for tag, body in chunks(p):
        if tag == "NODE":
            model["nodes"].append({"name": cstr(struct.unpack_from("<Q", body, 0)[0]),
                                   "t": struct.unpack_from("<3f", body, 0x10), "s": struct.unpack_from("<3f", body, 0x1C),
                                   "r": struct.unpack_from("<3f", body, 0x28), "parent": struct.unpack_from("<i", body, 0x60)[0]})
        elif tag == "MESH":
            name, vb, ib = struct.unpack_from("<3Q", body, 0)
            fmt, extra4, nidx, nvtx = struct.unpack_from("<4I", body, 0x48)
            stride = struct.unpack_from("<H", body, 0x5A)[0]
            model["meshes"].append({"name": cstr(name), "fmt": fmt, "extra4": extra4, "stride": stride, "nvtx": nvtx, "nidx": nidx,
                                    "vb": p[vb:vb + nvtx * stride], "ib": p[ib:ib + nidx * 2]})
        elif tag == "DRAW":
            model["draws"].append(struct.unpack_from("<3I", body, 0))
        elif tag == "MATE":
            model["materials"].append((cstr(struct.unpack_from("<Q", body, 0)[0]), cstr(struct.unpack_from("<Q", body, 8)[0])))
        elif tag == "TXTR":
            model["textures"].append(cstr(struct.unpack_from("<Q", body, 0)[0]))
    return model


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def local_matrix(n: dict):
    tx, ty, tz = n["t"]
    sx, sy, sz = n["s"]
    rx, ry, rz = n["r"]
    cx, sx_, cy, sy_, cz, sz_ = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    r = mat_mul(mat_mul([[cz, -sz_, 0, 0], [sz_, cz, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]],
                        [[cy, 0, sy_, 0], [0, 1, 0, 0], [-sy_, 0, cy, 0], [0, 0, 0, 1]]),
                [[1, 0, 0, 0], [0, cx, -sx_, 0], [0, sx_, cx, 0], [0, 0, 0, 1]])
    m = [[r[i][0] * sx, r[i][1] * sy, r[i][2] * sz, (tx, ty, tz)[i] if i < 3 else 1] for i in range(4)]
    m[3] = [0, 0, 0, 1]
    return m


def world_matrices(nodes: list) -> list:
    out = []
    for n in nodes:
        local = local_matrix(n)
        # skip the root axis node (index 0): keep the model's own Z-up space
        out.append(local if n["parent"] <= 0 else mat_mul(out[n["parent"]], local))
    return out


def geometry(model: dict):
    """Yields (mesh name, material index, positions, normals, uvs, triangles) in model space (Z up)."""
    world = world_matrices(model["nodes"])
    for node, mesh_i, mat_i in model["draws"]:
        m = model["meshes"][mesh_i]
        lay, w = layout(m["fmt"], m["extra4"]), world[node]
        assert lay["stride"] == m["stride"], (m["name"], hex(m["fmt"]), lay["stride"], m["stride"])
        pos, nrm, uv = [], [], []
        for v in range(m["nvtx"]):
            o = v * m["stride"]
            x, y, z = struct.unpack_from("<3f", m["vb"], o)
            pos.append(tuple(w[i][0] * x + w[i][1] * y + w[i][2] * z + w[i][3] for i in range(3)))
            if "normal" in lay:
                f = "<3f" if lay["normal_enc"] == "f32x3" else "<3e"
                nx, ny, nz = struct.unpack_from(f, m["vb"], o + lay["normal"])
                nrm.append(tuple(w[i][0] * nx + w[i][1] * ny + w[i][2] * nz for i in range(3)))
            if "uv0" in lay:
                uv.append(struct.unpack_from("<2f" if lay["uv0_enc"] == "f32x2" else "<2e", m["vb"], o + lay["uv0"]))
        idx = struct.unpack_from(f"<{m['nidx']}H", m["ib"])
        tris = [idx[i:i + 3] for i in range(0, len(idx) - 2, 3)]
        yield m["name"], mat_i, pos, nrm, uv, tris


def write_obj(model: dict, out: Path) -> None:
    lines, base = [f"mtllib {out.stem}.mtl"], 1
    for name, mat_i, pos, nrm, uv, tris in geometry(model):
        lines.append(f"o {name}\nusemtl {model['materials'][mat_i][0]}")
        lines += [f"v {x:.6f} {z:.6f} {-y:.6f}" for x, y, z in pos]  # OBJ viewers expect Y up
        lines += [f"vn {x:.4f} {z:.4f} {-y:.4f}" for x, y, z in nrm]
        lines += [f"vt {u:.5f} {1 - v:.5f}" for u, v in uv]
        lines += ["f " + " ".join(f"{base + i}/{base + i}/{base + i}" for i in t) for t in tris]
        base += len(pos)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    mtl = [f"newmtl {n}\nmap_Kd {t}.png" for n, t in model["materials"]]
    out.with_suffix(".mtl").write_text("\n".join(mtl) + "\n", encoding="utf-8")


def preview(model: dict, out: Path, size: int = 768, top: bool = False) -> None:
    from PIL import Image, ImageDraw
    faces = []
    for _name, _mat, pos, nrm, _uv, tris in geometry(model):
        if top:  # look down from +Z: (x, y, z) -> (x, -z, y), then the front-view code below draws the floor plan
            pos = [(x, -z, y) for x, y, z in pos]
        for t in tris:
            a, b, c = (pos[i] for i in t)
            ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
            vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
            n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
            ln = math.sqrt(sum(x * x for x in n)) or 1
            depth = (a[1] + b[1] + c[1]) / 3  # camera looks along +Y (model faces -Y)
            faces.append((depth, (a, b, c), abs(n[1]) / ln * 0.8 + 0.2))
    xs = [p[0] for _, tri, _ in faces for p in tri]
    zs = [p[2] for _, tri, _ in faces for p in tri]
    scale = (size - 20) / max(max(xs) - min(xs), max(zs) - min(zs))
    cx, cz = (max(xs) + min(xs)) / 2, (max(zs) + min(zs)) / 2
    img = Image.new("RGB", (size, size), (40, 40, 48))
    draw = ImageDraw.Draw(img)
    for _d, tri, shade in sorted(faces, key=lambda f: -f[0]):  # far first
        g = int(255 * shade)
        draw.polygon([(size / 2 + (p[0] - cx) * scale, size / 2 - (p[2] - cz) * scale) for p in tri], fill=(g, g, g))
    img.save(out)


def main() -> None:
    model = parse(Path(sys.argv[1]).read_bytes())
    print("parts:", model["parts"])
    print(f"{len(model['nodes'])} nodes, {len(model['meshes'])} meshes, {len(model['draws'])} draws, "
          f"{len(model['materials'])} materials, {len(model['textures'])} textures")
    for node, mesh_i, mat_i in model["draws"]:
        m = model["meshes"][mesh_i]
        print(f"  node {node:3} {model['nodes'][node]['name']:24} mesh {m['name']:24} fmt {m['fmt']:08x} "
              f"{m['nvtx']:6} vtx {m['nidx'] // 3:6} tris  material {model['materials'][mat_i]}")
    if "--obj" in sys.argv:
        write_obj(model, Path(sys.argv[sys.argv.index("--obj") + 1]))
    if "--preview" in sys.argv:
        preview(model, Path(sys.argv[sys.argv.index("--preview") + 1]), top="--top" in sys.argv)


if __name__ == "__main__":
    # self-check: the 44-byte eye format and the 84-byte skinned body format seen in chr/c11
    assert layout(0x00480429)["stride"] == 44 and layout(0x40490429)["normal"] == 52
    assert layout(0x40494429)["stride"] == 88 and layout(0x10480429)["stride"] == 60
    assert layout(0x125)["stride"] == 36 and layout(0x241125)["stride"] == 68 and layout(0x40490429, 1)["stride"] == 88
    main()
