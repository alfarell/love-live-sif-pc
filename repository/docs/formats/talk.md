# Member talk (`talk/talk.pac`)

Voiced lines a member says on the home screens (no on-screen text). Read by `src/game/talk/talk.{h,cpp}` (voice, motion and mouth keys).

## Package

`talk/talk.pac` (ARC) holds:

- `talk_data.bin`: when each talk may play (below).
- `c01.pac` .. `c22.pac`: one package per member (1-9 µ's, 11-19 Aqours, 21-22 Saint Snow), each with `talk_NNNN.btlb` files (Honoka: 92).

## `talk_data.bin`

A `BLBN` file (see `blbn.md`) with one table, `ll3::talk::s_talk_datas`. Rows are 16 bytes from offset 0x40, four u32 each, until padding (the last id repeated):

| Field | Meaning |
|---|---|
| id | the talk number (`talk_NNNN`) |
| type | 0 = any time; 1 = hour range; 2-28 = other conditions (*Hypothesis:* dates, play counts, unlocks; not decoded) |
| param | type 1: `end << 16 \| start` in local hours, wrapping past midnight |
| last | `>> 16` = weight (*Hypothesis:* priority; 150 for the greetings, 1 for plain talk) |

The greetings: talk 50 (05-13 h), 51 (13-19 h), 52 (19-21 h), 53 (21-23 h), 6 (23-05 h). The 92 rows match Honoka's 92 talks.

## `talk_NNNN.btlb`

A `BTL` (version 3) timeline, the same family as the live scene timelines. Its tracks and key times are not decoded yet. Its events are records `u16 tag, u16 payload size, u32 0, payload` (M18 U6; M7 had the two tags swapped):

- tag `0x0028`, payload 4: **the voice**, a u32 value. The cue is `CueId` = the member's base + value in `voice.acb`:
  - µ's: 10000 + 1000 × (member − 1); Aqours: 40000 + 1000 × (member − 11); Saint Snow: 50000 + 1000 × (member − 21);
  - values under 700 are `vo_ac_*` cues (`vo_ac_001_001` = 10001), 700 and up are `vo_ps_*` (`vo_ps_001_007` = 10707);
  - 1780 of the 1840 talks have one; 1744 name a cue (µ's talks 99-102, values 781-784, have none).
  - Example: Honoka's morning greeting (talk 50) has 707, `vo_ps_001_007`.
- tag `0x0514`, payload 8: **the navigator's motion**, (motion, 0): 501 idle1, 502 idle2, 503-506 talk1-4, 507 login. The values are the same for every member.
- tag `0x0443`, payload 4: *Hypothesis:* an expression change (not used).

## Lip sync: `chr/cNN/cNN.pac/timeline.pac` `voice_NN_<name>_<CueId>.btlb`

A BTL (version 2) timeline per voiced line (Honoka: 185; 85 of her 89 talk voices have one). Each key is `u32 frame, u32 1` (frames at 60 per second from the voice's start), then one event:

| Tag | Payload | Meaning |
|---|---|---|
| `0x0b` | 12: shape, blend, f32 weight | Move the mouth to `shape` (1-7 = the face's `a i i_small u e o n`, U4) over `blend` frames (4) |
| `0x0c` | 8: 0, blend | Close the mouth over `blend` frames |

Example: `voice_01_honoka_10707`: close at 0, `a` at 7, close at 13, `a` at 19, `u` at 39, `o` at 54, `i` at 59, `e` at 64 …

The same keys are in `chr/cNN/cNN.pac/voice.pac` `voice_NN_<name>_<CueId>.acb` (`ACGP`/`ACMD` files, not read).

The cues are in `sound/sound.pac` `voice.acb` with the waves in `sound/voice.awb` (AFS2, 10808 waves; 10613 cues `vo_ac_*` and `vo_ps_*`). See `cri.md`.
