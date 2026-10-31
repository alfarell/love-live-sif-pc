# SIF textures: `btx\0`

Status: **verified on all 40,101 textures** (ARC tags `tex`, `.btx` names; 8,248 of them inside `cmp`) of CUSA24620 v1.02: all decode with `tools/tex2png.py`, and one or more samples per format were checked visually (M2, 2026-10-31). Little-endian.

| Offset | Type | Field |
|---|---|---|
| 0x00 | char[4] | `btx\0` |
| 0x04 | u32 | version: 0x102 (most) or 0x101 |
| 0x08 | u16 | width |
| 0x0A | u16 | height |
| 0x0C | u32 | row pitch in bytes (per block row for BCn). Uncompressed rows can be padded: 469-wide card text has a 470-pixel pitch |
| 0x10 | u8 | format (table below) |
| 0x11 | u8 | 0 in all files |
| 0x12 | u8 | mip count − 1 |
| 0x13 | u8 | 1 in all files |
| 0x14 | bytes[4] | `01 05 00 00` (19 files `01 05 00 01`), meaning unknown |
| 0x18 | u32 | data offset (0x40, 0x50, 0x60, 0x70) |
| 0x1C | u32 | 0xFF |
| 0x20 | u32 | size; equals the file size in 17,397 files, not in the rest (meaning not settled) |
| 0x24 | u32 | name offset (0x30) |
| 0x30 | char[] | texture name, NUL-terminated |

## Formats

| Code | Format | Count | Check |
|---|---|---|---|
| 0 | BGRA8, 32 bpp | 1,701 | card art, member card text (channel order fixed after a blue-skin result) |
| 1 | BGR8, 24 bpp | 7 | `normaldef` decodes to (134, 125, 250), a flat normal map |
| 7 | BC1 (DXT1), 4 bpp | 13,442 | dress thumbnails |
| 9 | BC3 (DXT5), 8 bpp | 80 | event banners; alpha blocks `ff ff 00…` |
| 13 | BC7, 8 bpp | 24,860 | character body textures, UI plates |
| 16 | 8-bit single channel | 11 | member card frame masks (grey) |

## Layout

- **Linear, not GPU-tiled.** PS4 textures are often stored in the GPU's tiled layout; these are not, so no detiling is needed.
- Mip levels follow level 0, largest first; block formats round each level up to whole 4×4 blocks.
- **Cubemaps** (e.g. `stage/st017.pac/texture.pac/cubemap.btx`) store 6 faces one after another, each followed by its own mip chain. No header flag for this was found; `tex2png.py` detects a cube from the data size (6 × chain).
