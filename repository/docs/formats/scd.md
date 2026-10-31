# SIF charts: `Scor` (ARC tag `scd`)

Status: **structure verified on all 11,589 files** (size and event count consistent everywhere); field meanings below are partly *Hypothesis* (M2, 2026-10-31). Reader: `tools/chart_dump.py`. Little-endian. The format is close to a MIDI track.

## Where

`live/<song id>/<song id>.pac` holds `score.pac` (charts), `scene.pac` (`score_scene_*` files for stage direction) and `timeline.pac`. Chart names: `score_<song>_<variant>_<title>_ch<N>.scd`, plus `score_<song>_lyrics_<title>_ch1.scd`.

- Variants `00`–`04`: note counts rise 114 → 148 → 220 → 329 → 466 for song 0010. *Hypothesis:* five difficulties.
- Variants `10`–`13` have the same counts as `01`–`04`. *Hypothesis:* another mode.

## Header (0x40 bytes)

| Offset | Type | Field |
|---|---|---|
| 0x00 | char[4] | `Scor` |
| 0x04 | u32 | file size |
| 0x08 | u32 | event count (file size = 0x40 + 32 × count) |
| 0x0C | u32 | first tempo, µs per quarter note (285,714 = 210 BPM for song 0010) |
| 0x10 | u32 | ticks per quarter note: 480 in all files |
| 0x14, 0x18 | u32 | unknown (`0xD64`, `0x708` in song 0010) |
| 0x1C | u32 | channel number (= the `_chN` suffix) |
| 0x20 | u32 | ticks per bar: 1920, 1440 or 960 (4/4, 3/4, 2/4) |

## Events (32 bytes each, from 0x40)

| Offset | Type | Field |
|---|---|---|
| 0x00 | u16 | event id (increasing) |
| 0x02 | u16 | kind: 0 note, 2 tempo, 4 marker |
| 0x04 | u32 | tick |
| 0x08 | u32 | note: note number · tempo: µs per quarter · marker: 0 |
| 0x0C | u32 | note: velocity (mostly 100) |
| 0x10 | u32 | note: duration in ticks · marker: length |
| 0x14 | u32 | note: type/channel value · marker: value (some are 4 ASCII bytes, e.g. `\x01ata`) |
| 0x18 | u32[2] | 0 |

## Meaning so far (*Hypothesis*, for M4)

- `ch1` is the playable chart. Its note numbers for song 0010 variant 04 are 55–63: **nine values for SIF's nine buttons**.
- Duration 120 ticks (a 1/16 note) is a tap; longer durations look like holds.
- Simultaneous notes at the same tick are double notes.
- Other channels (`ch2`–`ch9`, lyrics) carry other cues; `ch4` uses note numbers 48–50 heavily.
- The note `type` field takes 21 values in `ch1` (mostly 1).

Times in ms are computed through the tempo map.
