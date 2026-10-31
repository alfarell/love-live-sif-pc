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

All are 60 fps (60000/1000), with no audio (`@SFA`) and no alpha (`@ALP`) streams, and are not encrypted. They are short previews (costume previews, banners), not full-screen story movies.
