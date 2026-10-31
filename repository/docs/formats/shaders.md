# SIF shaders: `sh_custom.pac`

Status: census with `tools/shader_stats.py` (M2, 2026-10-31).

## Layout

`shader/ps4/sh_custom.pac` (ARC) → 9 `bin` material archives → `SHG` permutation archives named `<material>-<bits>` (e.g. `fur_simple-00101`) → one nameless `SARC` (the permutation name, zero-padded to 268 bytes) and nameless `SHP` programs.

Each `SHP` is a PS4 shader binary, the same format pt-pc documents for P.T. (`pt-pc/tools/fsop.py`):
- a `Shdr` header (major 7, minor 2, Gnm type byte at +8);
- GCN code;
- an `OrbShdr` v7 footer (`BINARY_INFO`, code length = `info_bits >> 8`, code directly before the footer).

`shader/common_shader_tex.pac` holds 7 helper textures (black, white, cell 0–3, normaldef).

## Counts

| Item | Count |
|---|---|
| `SHP` programs | 26,196 (25,115 pixel, 1,081 vertex) |
| Permutations (`SHG`) | 876 with programs |
| **Unique code blobs** | **6,046** (5,767 pixel, 279 vertex) |
| Unique footer hashes | 6,658 (more than unique code: the hash covers more than the code) |
| Decode check | 6,046/6,046 disassemble cleanly to `s_endpgm` with `gcn.py` |

## Unique code per material

| Material | Unique programs |
|---|---|
| `ll3model` | 5,808 |
| `ll3model_nofog` | 120 |
| `fur_simple` | 92 |
| `sea` | 17 |
| `ll3monitor` | 6 |
| `water` | 5 |
| `projectionmap`, `spotlight`, `testvtf` | 2 each |

**Consequence for the port:** almost all variety is the permutation set of one model shader (`ll3model`). A rewrite needs about 9 hand-written shaders whose features are switched by the permutation bits, not thousands of translated programs. The bits' meanings come from the `SHG` names and the eboot (`BlShaderArchive`).
