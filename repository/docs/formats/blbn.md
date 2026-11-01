# SIF data tables: `BLBN` (`.bin` entries)

Status: container read by `src/engine/formats/blbn.cpp` (M4). 1,698 `bin` entries use it (e.g. everything in `common/common.pac`: `live_params.bin`, `live_datas.bin`, `skill_card_datas.bin`, `mission_*.bin`, `dlc_music_data.bin`). Little-endian.

## Layout

| Offset | Content |
|---|---|
| 0x00 | `BLBN`, then 8 bytes (`00 00 00 00 00 00 04 00`) |
| 0x0C | `SYM\0` chunks, one per exported symbol, until the first other chunk |
| … | `DATA` chunks: the symbols' data and their names |

SYM chunk: char[4] `SYM\0`, u32 0x18, u32 flags (`0x00040000`), u64 data offset, u64 name offset, u32 name hash, u32 data size.

Offsets are from the file start. Names are the C++ symbols of the game's static tables (`s_CommonParam`, `ll3::s_SKILL_LEVEL_MAX`); the eboot carries the same names (`work/eboot-strings.txt` around 0x5EB95E). The table contents are plain arrays; pointers inside them (if any) are not decoded yet.

## `common/common.pac/live_common.pac/live_params.bin`

| Symbol | Size | Content |
|---|---|---|
| `s_CommonParam` | 500 | Per-difficulty integer groups (2,300 … 650; *Hypothesis:* note approach times in ms). From the eboot (U5): +0xB4 base points 100, +0xB8 hold-tick points 10, **+0xC0…+0xCC score weights Bad 0.1 / Good 0.2 / Great 0.7 / Perfect 1.0**, +0xD0 score multiplier per difficulty index (9 floats, 1.0–3.0), +0x118/+0x11C note-type multipliers, +0x120 skill multiplier 3.0, +0x128 hold-tick fraction 0.033, +0x130 tick length 1/24 s, +0x138…+0x14C rank thresholds (0.45 … 1.2) |
| **`s_EvaluateRangeDatas`** | 128 | **Judgement windows in ms**, 4 groups × (early row, late row) × {Perfect, Great, Good, Miss} (see below). Early rows are stored Miss-first, late rows Perfect-first |
| `s_CalibrationData` | 8 | 32, 16: built-in timing offset in ms per input-device type (type 1 → 32, types 0/2 → 16), added to the user's adjustment (eboot 0x9F7C0) |
| `s_UserCalibrationData` | 44 | −80 … +80 in steps of 16: the user-selectable timing offsets in ms (11 values) |
| `s_SkillAdjustDatas`, `s_LedControlParam`, `s_Camera*`, `s_liveView*`, `s_skill*` | | Skills, LEDs (arcade cabinet), cameras, live-view, skill voices and effects |
| `ll3::s_SKILL_LEVEL_*`, `s_INVALID_SKILL_ID`, `live::s_skillVoiceCallNum` | 4 each | Constants |

## `common/common.pac/live_common.pac/live_datas.bin`

`s_live_datas`: 1,125 records of 904 bytes, one per chart (eboot 0x945B0 searches it by id). +0x00 id = song × 1000 + variant, +0x04 song, +0x08 variant, +0x0C 100, +0x14 difficulty index 0–8 (indexes `s_CommonParam` +0xD0), +0x18 *Hypothesis:* star level. The rest is not decoded.

`s_music_datas`: 125 records of 832 bytes, one per song. +0x00 song id, +0x18 member count (9), +0x1C member slots (song 0010: 4 6 2 7 1 3 8 9 5; *Hypothesis:* formation order), +0x48…+0xE8 string pointers (titles in 4 languages, phonetic, singers, composer, arranger, lyricist), **+0x104 f32 the dance/camera frame at which the music starts** (972 for 0010; M5, `analysis/202611011121-m5-live-presentation/202611011509-analysis.md`).

### Judgement windows (`s_EvaluateRangeDatas`)

| Group | Perfect | Great | Good | Bad | Use |
|---|---|---|---|---|---|
| 0 | ±40 | ±133 | ±200 | ±267 | presses |
| 1 | ±133 | ±200 | ±267 | ±333 | *Hypothesis:* hold releases |
| 2 | ±60 | ±153 | ±220 | ±287 | group 0 + 20 (*Hypothesis:* skill `extendEvaluation`) |
| 3 | ±153 | ±220 | ±287 | ±353 | group 1 + 20 |

133, 200, 267 and 333 are 8, 12, 16 and 20 frames at 60 fps, which supports ms as the unit. Early and late bounds are equal in all groups.

**Correction (M4, shadPS4 sweep):** the four bounds are **Perfect, Great, Good, Bad**. The game's result screen has five grades (Perfect, Great, Good, BAD, Miss), and a note not hit inside the Bad bound is a Miss. A judgement sweep in shadPS4 put each lane's hits into exactly the grade these bounds predict. The column headers above read Perfect / Great / Good / Bad accordingly.
