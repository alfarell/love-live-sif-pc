# SIF camera animations: `BSCM` (`.bscam`)

Status: decoded in M5 (2026-11-01) on `live/0010/0010.pac/scene.pac/camera_anim_0010_00_main.bscam`. Reader: `src/engine/formats/bscm.{h,cpp}` (checked in `sif_bmar_test`). Little-endian.

## Where

Each song has `live/<song>/<song>.pac/scene.pac/camera_anim_<song>_00_main.bscam`. Next to it is a `.bmarc` (BMAR) of the same name, whose animation moves one node, `Camera_Interest` (the look-at point). Both run on the song's 60 fps dance clock (9,060 frames for 0010). That clock starts `s_music_datas` +0x104 frames before the music (`blbn.md`).

## Layout

Chunks like BMAR's: char[4] tag, u32 body size, u32 `0x00040000`, body padded to 4. The list is `BSCM` header, three `DATA` chunks, `POF0`, and a zero tag. Pointers are u64 **file offsets** of `DATA` bodies.

### `BSCM` body (0x68 bytes)

| Offset | Type | Field |
|---|---|---|
| 0x00 | char[4] | version `101` |
| 0x04 | f32 | frame rate (60) |
| 0x08 | f32 | frame count (9,060) |
| 0x0C | u32 | position key count |
| 0x10 | u32 | rotation key count |
| 0x14 | u32 | 0 (*Hypothesis:* a fourth, unused track) |
| 0x18 | u32 | field-of-view key count |
| 0x1C | f32[3] | frame-0 position |
| 0x34 | f32[3] | 1, 1, 1 (*Hypothesis:* scale) |
| 0x40 | f32 | frame-0 field of view (30.17) |
| 0x48 | u64 | position keys* |
| 0x50 | u64 | rotation keys* |
| 0x58 | u64 | 0 |
| 0x60 | u64 | field-of-view keys* |

### Keys

Every key is u32 frame, u32 flag (0 in this file), then the value. Song 0010 has one key per frame.

| Track | Key size | Value |
|---|---|---|
| Position | 20 | f32[3] |
| Rotation | 24 | quaternion f32[4], x, y, z, w |
| Field of view | 12 | f32 degrees (18–30 for 0010) |

## Space

- Position and rotation are in a **Y-up** world: frame 0 is (0, 1.29, 11.05), 1.29 m high and 11 m in front of the stage.
- The camera looks along its local **−Z**. Check: the direction to `Camera_Interest` (Z-up in the `.bmarc`, turned to Y-up as bml_axisNode does) matches the camera's −Z with cos = 1.000 at frames 0, 1311, 2000, 3000, 4500, 6000 and 8000.
- BMAR models are Z-up, so the port's view matrix is inverse(T · R) · Rx(−90°) (`bscm::ViewMatrix`).
- The field of view is used as the **vertical** angle (*Hypothesis*). Shots frame the members sensibly with it; not compared against the game's screen.
