# CRI middleware data: ACB / AWB / HCA / ADX / USM

Status: containers verified with `tools/acb_dump.py` and `tools/usm_probe.py` (M2, 2026-10-31). These are standard CRI formats; this note records how SIF uses them.

## Audio

| File | Content |
|---|---|
| `sound/sound.pac/music.acb` | Cue sheet `MUSIC`: 146 cues, 150 waveforms, all streamed. Its `StreamAwbAfs2Header` table lists **120** stream banks: `sound/music.awb` (31 waves) + 119 `sound/dlc/MUSIC_xxxx.awb` (1 wave each) |
| `sound/sound.pac/voice.acb` | Cue sheet `VOICE`: 10,613 cues (`vo_ac_001_001`, ...), 10,809 waveforms: 10,808 streamed from `sound/voice.awb` (ids 0–10,807 = its 10,808 waves) + 1 in an embedded AWB |
| `sound/sound.pac/sys.acb` | Cue sheet `SYS`: 296 cues, 286 waveforms in an embedded AWB: 151 ADX + 135 HCA |
| `sound/sound.pac/ll3.acf` | ACF (global settings), `@UTF` |

- **HCA** streams are v2.0, 48 kHz (music stereo, voice mono). The `ciph` chunk is type 0, so they are **not encrypted** and need no key.
- **ADX** (`80 00` magic, EncodeType 0) is used for 151 system sounds at 44.1 kHz.
- `acb`-tagged `ACGP` entries inside character motion archives (1,229) are **not CRI**: they are small chunked command lists (`ACGP` → `ACMD`) next to `.mot` motions. *Hypothesis:* motion event tracks.

`@UTF` tables are big-endian. Column storage: 0x10 none, 0x30 constant, 0x50 per row. Column types: 0–9 integers and floats, 0xA string, 0xB data. `acb_dump.py` has the exact layout and a self-check.

## Video (`movie/**/*.usm`, 202 files, 810 MB)

| Video codec | Size | Files |
|---|---|---|
| `mpeg_codec` 1 (MPEG-1/2 elementary stream, payload starts `00 00 01 b3`) | 256×144 | 180 |
| | 512×288 | 11 |
| | 1920×120 | 10 |
| | 1024×288 | 1 |

They have no audio (`@SFA`) and no alpha (`@ALP`) streams, and are not encrypted. They are short previews (costume previews, banners), not full-screen story movies.

**Correction (M8, 2026-11-09).** Not all are 60 fps. The stage monitors are 30 fps (`monitor_001`: 30000/1000, 150 frames), the song previews 59.94 fps (60000/1001) and the costume previews 60 fps.

### Decoding (M8, `src/engine/video/movie.cpp`)

**Stream.** The video is the `@SFV` channel 0 type-0 payloads, joined. It is plain **MPEG-1** video: sequence headers (`00 00 01 B3`) before every GOP, no MPEG-2 extensions (`B5`), and a sequence end code (`B7`) at the end. GOPs are 13 pictures: I P B B P B B … P B B.

**Sofdec's intra DC precision.** The `@SFV` header's `mpeg_dcprec` is **11**. CRI's encoder uses MPEG-2's higher intra DC precision inside MPEG-1, so a plain MPEG-1 decoder produces garbage: saturated colours, lost sync, dropped pictures. Decoding it needs:
- the MPEG-2 DC size codes for sizes 9-11:
  - luma `11111110`, `111111110`, `111111111`;
  - chroma `111111110`, `1111111110`, `1111111111`;
- a DC predictor reset of `1 << (precision − 1)`, which is 1024;
- a DC multiplier of `8 >> (precision − 8)`, which is 1.

The port patches pl_mpeg for this; see `THIRD_PARTY.md`.
