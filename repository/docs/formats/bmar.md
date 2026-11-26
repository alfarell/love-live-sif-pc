# SIF models and motions: `BMAR104` ("BladeModelPac")

Status: container, scene graph, meshes and vertex formats **verified on all 8,241 `BMAR` files** (3,534 `mdl`/`.bmarc` models, 4,707 `mot` motions): all parse, and all 27,392 meshes match their stored stride (M2 follow-up, 2026-10-31). Bind-pose renders were checked visually for a character (`chr/c11/.../mod_11_chika`) and a stage (`stage/st017.pac/model.pac/017_livehall.bmarc`). Motion keyframes and skinning decoded in M5 (2026-11-01): `mod_01_honoka` posed with the song 0010 dance renders correctly. Reader: `tools/bmar_dump.py` (list, OBJ export, preview PNG). Little-endian.

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
| 0x20 | face meshes: blend-shape table* (see below) |
| 0x48 | u32 vertex format bits |
| 0x4C | u32 extra 4-byte attribute count |
| 0x50 | u32 index count |
| 0x54 | u32 vertex count |
| 0x58 | u16 2 |
| 0x5A | u16 stride |
| 0x5C | u32 flags |
| 0x60 | face meshes: u8 blend-shape count (46 for Honoka, 51 for Chika), u8 2 |

Rigid meshes are stored relative to their node (eye highlights sit at their eye only after the node transform). Skinned meshes are already in model space (bind pose).

**Triangle winding:** front faces are those whose (b−a)×(c−a) points away from the viewer in the Z-up model space. That is, culling the other way shows the stage interior from outside (*Hypothesis:* clockwise front faces, D3D style).

### Vertex format bits (storage order)

Sizes are solved from all 22 format/stride pairs in the game. Every stride matches `size(bits) + 4 × MESH+0x4C`.

| Bit | Attribute | Encoding | Bytes |
|---|---|---|---|
| 0x00000001 | position | f32×3 | 12 |
| 0x40000000 (+0x00010000) | skin | 8 influences: f32×4 weights, u8×4 bone indices, f32×4 weights, u8×4 bone indices (indices into the `MTXP` palette; all 8 weights sum to 1; the second set is zero on most body vertices, used on hair) | 40 |
| 0x00000004 | normal | f32×3 | 12 |
| 0x00000008 | normal | f16×4 | 8 |
| 0x00000020 | color | u8×4 | 4 |
| 0x00000100 / 0x00000400 | uv0 | f32×2 / f16×2 | 8 / 4 |
| 0x00001000 / 0x00004000 | uv1 | f32×2 / f16×2 | 8 / 4 |
| 0x00040000 / 0x00080000 | tangent | f32×3 / f16×4 | 12 / 8 |
| 0x00200000 / 0x00400000 | binormal | f32×3 / f16×4 | 12 / 8 |
| 0x10000000 | blend-shape texels (face meshes, M18 U4) | f32×4: (x, y) the position-offset texel, (z, w) the normal-offset texel in the shape textures | 16 |
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

### Face blend shapes (M18 U4)

Checked on `mod_01_honoka` and `mod_11_chika`, and drawn on the top-menu navigator:
- **The shape table:** `MESH +0x20` lists the face mesh's shapes as 16-byte records (name*, u64 0), with the count at `MESH +0x60`. The order is the same for every member: 0 `default`, 1 `a`, 2 `i`, 3 `i_small`, 4 `u`, 5 `e`, 6 `o`, 7 `n`, 8 `mouth_Smile`, 9 `mouth_happy`, 10 `eye_Blink`, 11 `eye_open`, 12-15 winks, 16-27 partial blinks, 28-44 brows, 45 `mouth_Smile`. Chika has 5 more after these.
- **The shape data:** each name is a texture in the same model's `BMT`. It is a btx of format 0x1B, RGBA16F, 128 × 64:
  - the top half holds the per-vertex position offset (x, y, z, w = 1);
  - the bottom half holds the normal offset.
  - Vertex k uses texel (k mod 128, k div 128) and that texel + 32 rows; the vertex's f32×4 attribute stores these as texel-centre UVs.
- **Blending:** position = bind + Σ weight × offset; the same for the normal, then normalized (*hypothesis* for the normal).
  - The bare mesh has an open mouth; shape 0 `default` closes it to the neutral face.
- **Costume models** (`chr/cNN/dNNN/*.mdl`) have the face mesh with its texel attribute, but no shape table or shape textures. The member's own `chr/cNN/cNN.pac/model.pac/mod_NN_<name>` supplies them (same vertex count).

### Other `BML` chunks
- `DRAW` (16 bytes): u32 node, u32 mesh, u32 material, u32 0.
- `MATE` (72 bytes): name*, texture name*, then colors/params (not decoded).
- `TXTR`: texture name*.
- `MTXP` (one per skinned model): +0 node index list* (u32 each), +8 inverse bind matrices* (f32[16], column-major, in the Z-up model space without `bml_axisNode`), +0x12 u16 count. Palette matrix k = posed world(node[k]) × inverse bind[k]; it is the identity in the bind pose (checked on 4 entries of `mod_01_honoka`, error 0).

## Motions (`BMA` part)

| Chunk | Content |
|---|---|
| `ANHD` (12 bytes) | char[4] version `103`, f32 frame rate (60), f32 frame count (e.g. 210 = 3.5 s for `mot_11_chika_010_idle`; 9,060 = 151 s for the song 0010 dance) |
| `ANSK` (16 bytes) | track table*, u16 track count (91), u16 ? |
| `ANMT` | Material tracks (M6): u64 table*, u16 count; see below |
| `ANVS` | Visibility tracks (M6): same layout |
| `ANMS` | Blend-shape weights (M18 U4; the facial clips `mot_NN_<name>_facial_*`): same layout as `ANMT`, see below |

- **Track (24 bytes):** node name*, channel table*, u32 node name hash (= `NODE` +0x5C), u32 channel count (3).
- **Channel (24 bytes):** key table*, value array*, u32 key count, u32 channel type: 0 translate, 3 rotate, 2 scale.
- **Key (12 bytes):** f32 frame, u32 byte offset into the value array, u32 interpolation flag.
- **Values:** translate and scale f32[3]; rotate a quaternion f32[4] in order x, y, z, w.
  - Rotation keys match the `NODE` Euler rotation (local = T · Rz · Ry · Rx · S) for all 27 nodes that don't move at frame 0; the other 56 already move at frame 0.
  - Translation keys match the bind pose for 81 of 83 nodes (bone lengths are kept).
- **Frames:** increasing in all 319 extracted animation files. The song 0010 dance keys every 2 frames (4,235 keys per moving channel); static channels have 1 key.
- **Interpolation flag** (over 319 files, 34,682 channels of each type):
  - 0 in all but 10 channels;
  - 1 and 2 also occur, and channels with flag 2 can have 36-byte values (*Hypothesis:* value + in/out tangents).
  - The port treats every key as linear (normalized lerp for quaternions).
- **`ANMS`:** one record per shape. The record's last u32 is a u16 channel count (1) and a u16 **shape index** (the face mesh's shape table). The name is the mesh (`Honoka_face`). Its one channel is of type 12, f32 weights. Examples:
  - `001_a`: shape 1, weight 0 → 1 over frames 0-6;
  - `100_blink`: shape 10, weights 0, 0.5, 1, 0.74, 0.26, 0 at frames 0-10 (2 apart).

### `ANMT` and `ANVS` (M6, decoded on the ui2 clips)

- **Record (24 bytes):** name*, channel table*, u32 name hash, u32 channel count. The name is a material name for `ANMT` and a node name for `ANVS`.
- **Channels:** as in `ANSK` (keys*, values*, u32 key count, u32 type). Keys are 12 bytes (f32 frame, u32 byte offset into the values, u32 flag).
- **Key flags:** 1 and 0 are linear; 2 is step (hold until the next key). `ANVS` uses 2 throughout (*Hypothesis* from the 0/1 values).
- **Channel types** (survey of the 282 extracted ui2 clips):

| Type | Chunk | Values | Meaning |
|---|---|---|---|
| 6 | `ANMT` | RGB 0–255 (22 channels) | Material colour (e.g. a banner fading in from black over frames 10–15) |
| 4 | `ANMT` | 0–0.75 (15) | **U offset** of the material's texture (first component; label atlas column: 0, 0.25, 0.5, 0.75) |
| 5 | `ANMT` | 0–0.62 (15) | **V offset** (label atlas row: steps of 0.125 or 0.25) |
| 11 | `ANVS` | 0 / 1 (120) | Node visible |

- **Value width:** usually 12 bytes per `ANMT` key, but a few channels store one float (the port replicates it); `ANVS` keys are 4 bytes.
- Types 4/5 were first read as transparency/incandescence; the top menu's label clips settle them as UV offsets. Clip `a020` ("switch") of `uip0005_SubMenuList00` steps U through 0, 0.25, 0.5, 0.75 per frame and V by 0.125 every 4 frames over a 0.25 × 0.125 cell, so frame *N* shows atlas cell *N*. The game poses that clip at the label id (M6, `analysis/202611011550-m6-ui-boot-flow/`).
