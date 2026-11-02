# `.fad` 2D animations

Decoded in M6 from the 9 files in `ui/ui_ingame.pac/anim.pac`:
- judgement texts: `ui_ingame_judge_text_{perfect,great,good,bad,miss}`, plus the `_support` variants of perfect and great;
- `ui_ingame_judgering_break`;
- `ui_ingame_end_text_liveclear`.

Reader: `src/engine/formats/fad.{h,cpp}`. Dump tool: `tools/fad_dump.py`. Test: `tests/unit/fad_test.cpp`.

The format is a Flash-like timeline: textures, one-frame sprites that each show one textured rectangle, and a main timeline that places sprites at depths and changes their matrix and colour per frame.

## Header (24 bytes, little-endian)

| Offset | Type | Value |
|---|---|---|
| 0 | u16 | 1 (version) |
| 2 | u16 | 9–12 (unknown) |
| 4 | i32 × 4 | canvas x0, x1, y0, y1: always 0, 400, 0, 400 |
| 20 | u16 | frames per second: 60 |
| 22 | u16 | main-timeline frame count (60–63 for the judgements, 200 for LIVE CLEAR) |

## Records

From offset 24 to the end of the file: u8 tag, u8 `0xAA`, u16 record length in **16-bit words including these 4 bytes**, then the payload. All nine files parse exactly to their end.

| Tag | Payload | Meaning |
|---|---|---|
| `09` | u16 count, then count × (u16 id, name, NUL, padded to an even length) | Texture names (entries of `ui_ingame.pac/texture.pac`) |
| `01` | 4 bytes (e.g. `ffffffff`, `999999ff`) | Background colour (*Hypothesis*; ignored) |
| `02` | u16 id, u16 width, u16 height | A textured rectangle (shape); its id is the texture's id |
| `03` | u16 sprite id, u16 frame count (always 1) | Starts a sprite definition; its records run to the next `00` |
| `00` | — | Ends a sprite definition |
| `04` | u16 character, u16 depth | Places a shape (inside a sprite) or a sprite (main timeline) at a depth; matrix and colour reset |
| `05` | u16 depth, u16 flag (0), f32 a, d, b, c, tx, ty | Matrix of the object at `depth`: x′ = a·x + c·y + tx, y′ = b·x + d·y + ty (y down) |
| `06` | u16 depth, u16 flags, i16 × 4 multiply RGBA (8.8 fixed), i16 × 4 add RGBA (0–255) | Colour transform. Flags: 0x80, plus 1 = multiply, 2 = add |
| `07` | u16 0, u16 depth | Removes the object at `depth` |
| `08` | — | Shows a frame (inside a sprite: the sprite's frame; else the next main-timeline frame) |

- Each sprite places its shape at depth 1 with matrix (1, 1, 0, 0, −w/2, −h/2), i.e. centred on the sprite's origin.
- Rotation appears as a = d = s·cos θ and b = −c = s·sin θ. LIVE CLEAR also uses negative scales. The rotation sign is not checked against the game.
- Matrices and colours stay until changed, and the main-timeline frame count equals the header's frame count in every file.

## Content

| File | Main frames | Sprites | Notes |
|---|---|---|---|
| judge_text_miss | 61 | 1 | Text at (203, 101); drifts down about 7 px and fades out over frames 10–24 |
| judge_text_good | 61 | 3 | Rings 01/02 expand from (200, 200) and fade in; text rises to about (198, 110) |
| judge_text_perfect | 63 | 11 | 4 rings, kira, text, 5 stars |
| judgering_break | 60 | 6 | — |
| end_text_liveclear | 200 | 11 | Colour add (flags 0x82) for a brightening flash |

The canvas centre (200, 200) is the centre of the rings. The text sits about 85–100 px above it, arched to follow the rings.

## Placement (port)

The port draws a judgement's animation with its canvas centre on the combo counter (canvas (960, 470)). It restarts on each judgement. With this anchor, the rings frame the combo and the arched text follows them (`work/m6/fad_a.png`). The game's own anchor is not traced (*Hypothesis*). The colour "add" part is not drawn yet.
