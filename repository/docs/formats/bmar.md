# SIF models and motions: `BMAR104` ("BladeModelPac")

Status: container, scene graph, meshes and vertex formats **verified on all 8,241 `BMAR` files** (3,534 `mdl`/`.bmarc` models, 4,707 `mot` motions): all parse, and all 27,392 meshes match their stored stride (M2 follow-up, 2026-10-31). Bind-pose renders were checked visually for a character (`chr/c11/.../mod_11_chika`) and a stage (`stage/st017.pac/model.pac/017_livehall.bmarc`). Motion keyframes are surveyed, not decoded. Reader: `tools/bmar_dump.py` (list, OBJ export, preview PNG). Little-endian.

"Blade" fits the eboot's `Bl*` class names (`BlShaderArchive`). *Hypothesis:* it is the in-house engine's name. The chunk style (`POF0` pointer-fixup table) resembles Sega's NN formats; SIF AC is a Sega title (*Hypothesis*).

## Container

`"BMAR104\0"`, then an ordinary `ARC\0` table (see `arc.md`) named `BladeModelPac`, offsets relative to byte 8. Parts:

| Part | In | Content |
|---|---|---|
| `BML` | models | Scene: nodes, meshes, materials, draws, textures |
| `BMA` | all | Animation (models carry a bind/default clip; motions have only `BMA`) |
| `BMD` | 828 models | Starts `mad\0` `200\0`; not decoded (*Hypothesis:* morph/dynamics data) |
| `BMT` | 24 models | Embedded texture archive (`ARC` "BmlTexArchive" of `btx`). Other models use textures from a sibling `texture.texs`/`texture.pac` |

## Chunk lists (`BML`, `BMA`)

Each chunk is: char[4] tag, u32 body size, u32 `0x00040000`, then the body, padded to 4 bytes. The list ends with `POF0` (pointer offsets) and a chunk with tag 0.

Pointers in bodies are **u64 offsets from the part start**, pointing at the body of a `DATA` chunk (strings, buffers, tables).

`BML` chunk counts over all models:

| Chunk | Count |
|---|---|
| `NODE` | 256,846 |
| `MESH` | 27,392 |
| `DRAW` | 27,392 |
| `MATE` | 16,143 |
| `TXTR` | 35,879 |
| `MTXP` | 3,633 |
| `BML` header, `MDL` (floats; *Hypothesis:* bounding data) | one each per model |

### `BML` header (16 bytes)
u16 mesh count, u16 node count, u16 material count, u16 ?, u16 draw count, u16 texture count, u32 0. These match the chunk counts in the checked sample.

### `NODE` (104 bytes)

| Offset | Field |
|---|---|
| 0x00 | name* |
| 0x08 | parent name* (empty for the root) |
| 0x10 | f32[3] translation |
| 0x1C | f32[3] scale |
| 0x28 | f32[3] rotation, radians; local matrix = T · Rz · Ry · Rx · S |
| 0x34, 0x40 | f32[3] bounding min / max (±FLT_MAX when unused) |
| 0x5C | u32 name hash (motion tracks bind by this hash) |
| 0x60 | s32 parent index (−1 root); matches the parent name in 206/206 nodes of the sample |
| 0x64 | u32 flags (0x100, 0x106 seen) |

Node 0 is `bml_axisNode`, rotated −90° about X. Model data is **Z-up** (eyes at z ≈ 1.39 m); this node turns it Y-up for the renderer.

### `MESH` (104 bytes)

| Offset | Field |
|---|---|
| 0x00 | name* |
| 0x08 | vertex buffer* |
| 0x10 | index buffer*: u16 triangle list |
| 0x18 | count record* |
| 0x48 | u32 vertex format bits |
| 0x4C | u32 extra 4-byte attribute count |
| 0x50 | u32 index count |
| 0x54 | u32 vertex count |
| 0x58 | u16 2 |
| 0x5A | u16 stride |
| 0x5C | u32 flags |

Rigid meshes are stored relative to their node (eye highlights sit at their eye only after the node transform). Skinned meshes are already in model space (bind pose).

**Triangle winding:** front faces are those whose (b−a)×(c−a) points away from the viewer in the Z-up model space. That is, culling the other way shows the stage interior from outside (*Hypothesis:* clockwise front faces, D3D style).

### Vertex format bits (storage order)

Sizes are solved from all 22 format/stride pairs in the game. Every stride matches `size(bits) + 4 × MESH+0x4C`.

| Bit | Attribute | Encoding | Bytes |
|---|---|---|---|
| 0x00000001 | position | f32×3 | 12 |
| 0x40000000 (+0x00010000) | skin | f32×4 weights (sum 1), u8×4 bone indices (into the `MTXP` palette), 20 bytes zero in samples | 40 |
| 0x00000004 | normal | f32×3 | 12 |
| 0x00000008 | normal | f16×4 | 8 |
| 0x00000020 | color | u8×4 | 4 |
| 0x00000100 / 0x00000400 | uv0 | f32×2 / f16×2 | 8 / 4 |
| 0x00001000 / 0x00004000 | uv1 | f32×2 / f16×2 | 8 / 4 |
| 0x00040000 / 0x00080000 | tangent | f32×3 / f16×4 | 12 / 8 |
| 0x00200000 / 0x00400000 | binormal | f32×3 / f16×4 | 12 / 8 |
| 0x10000000 | extra (face meshes) | f32×4 | 16 |
| MESH+0x4C × | extra | 4 bytes each, placed last (*Hypothesis*) | 4 |

Normal check over all meshes: the decoded normal is unit length in most meshes. The rest have **consistent** non-unit lengths (e.g. 1.13 on eye meshes), i.e. stored unnormalized, not misread. 89 of 27,392 meshes have first-vertex UVs outside ±8 (not investigated).

Most common formats:

| Format | Meshes | Use |
|---|---|---|
| `0x125` | 7,612 | stages/props, float normals and UVs |
| `0x40490429` | 5,570 | skinned characters |
| `0x40494429` | 5,205 | skinned characters with uv1 |
| `0x480429` | 3,348 | |
| `0x480129` | 3,180 | |

### Other `BML` chunks
- `DRAW` (16 bytes): u32 node, u32 mesh, u32 material, u32 0.
- `MATE` (72 bytes): name*, texture name*, then colors/params (not decoded).
- `TXTR`: texture name*.
- `MTXP`: node-index list* + matrix array* (64-byte matrices; *Hypothesis:* inverse bind matrices for the skin palette).

## Motions (`BMA` part)

| Chunk | Content |
|---|---|
| `ANHD` (12 bytes) | char[4] version `103`, f32 frame rate (60), f32 frame count (e.g. 210 = 3.5 s for `mot_11_chika_010_idle`) |
| `ANSK` (16 bytes) | track table*, u16 track count (91), u16 ? |
| `ANMS`, `ANMT`, `ANVS` | in some files; not decoded (*Hypothesis:* morph, material, visibility tracks) |

- **Track (24 bytes):** node name*, channel table*, u32 node name hash (= `NODE` +0x5C), u32 channel count (3).
- **Channel (24 bytes):** key times*?, key values*?, u32 key count, u32 channel type (0, 3, 2; *Hypothesis:* translate, rotate, scale).

Key encoding: M5 work.
