# SIF charts: `Scor` (ARC tag `scd`)

Status: **structure verified on all 11,589 files** (size and event count consistent everywhere); field meanings below are partly *Hypothesis* (M2, 2026-10-31). Reader: `tools/chart_dump.py`. Little-endian. The format is close to a MIDI track.

## Where

`live/<song id>/<song id>.pac` holds `score.pac` (charts), `scene.pac` (`score_scene_*` files for stage direction) and `timeline.pac`. Chart names: `score_<song>_<variant>_<title>_ch<N>.scd`, plus `score_<song>_lyrics_<title>_ch1.scd`.

- Variants `00`–`04`: note counts rise 114 → 148 → 220 → 329 → 466 for song 0010. The eboot names the arcade difficulties `ac_easy`, `ac_normal`, `ac_hard`, `ac_extreme`, `ac_challenge` (order by note count: *Hypothesis*).
- Variants `10`–`13` have the same counts as `01`–`04`: the home-version difficulties `cs_basic`, `cs_regular`, `cs_expert`, `cs_master`.
- The song's music is the `music.acb` cue whose **CueId equals the song id** (cue 10 = song 0010, all songs; M4).

## Header (0x40 bytes)

| Offset | Type | Field |
|---|---|---|
| 0x00 | char[4] | `Scor` |
| 0x04 | u32 | file size |
| 0x08 | u32 | event count (file size = 0x40 + 32 × count) |
| 0x0C | u32 | first tempo, µs per quarter note (285,714 = 210 BPM for song 0010) |
| 0x10 | u32 | ticks per quarter note: 480 in all files |
| 0x14 | u32 | **lead-in in ms: audio time = chart time + this value** (M4: always a whole number of bars at the first tempo, e.g. 3,428 = 12 beats at 210 BPM; confirmed against audio onsets of songs 0010, 0020, 0050 to the 5 ms analysis step) |
| 0x18 | u32 | 700–2,000 in `ch1` files, 2,000 in others; meaning unknown (*Hypothesis:* note approach or preview time) |
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

- `ch1` is the playable chart. Its note numbers are 55–63: **nine values for SIF's nine buttons** (all 130 songs). Note number 0 occurs only in home-version charts `11`–`13` (3,092 notes; meaning unknown). The note type field is 1 in every `ch1` note.
- Duration 120 ticks (a 1/16 note) is a tap; longer durations look like holds.
- Simultaneous notes at the same tick are double notes.
- Other channels (`ch2`–`ch9`, lyrics) carry other cues; `ch4` uses note numbers 48–50 heavily.
- The note `type` field takes 21 values in `ch1` (mostly 1).

Times in ms are computed through the tempo map.

## Channels 2-5: Finale Rhythm Icons and phases (2026-11-08)

Checked on songs 0010, 0020, 0050 and 0340 (all variants), in the eboot, and in shadPS4. Notes: `analysis/202611072212-gameplay-feedback/202611081816-analysis.md`.

- **`ch2`, `ch3`, `ch4`** each hold 9 tap notes, 3 in each of three phases. Their note type field equals the channel number.
  - The game loads them like `ch1` (eboot `0x8F670`: channels 1, then 2, 3, 4, 6, 7 from the table at `0x5E7830`).
  - They are **played as extra notes**: song 0340 Normal has 167 `ch1` notes, and the result counts 194.
  - In each phase the notes of one channel carry the Finale Rhythm Icon mark (seen: phase 1 `ch2`, phase 2 `ch4`, phase 3 `ch3`). The others are plain.
- **`ch5`** is a timeline, opened separately (`0x98A50` → `0xABAC0`). It has 10 long notes:
  - phases 1-3 start with three notes each, at the same time: 60/63/66, then 61/64/67, then 62/65/68;
  - **note 69 starts the finale section**, which runs to the end of the song.

  Phase times are the same for all variants of a song.
- `ch6` and `ch7` are also loaded as notes. They were empty in every chart checked.
