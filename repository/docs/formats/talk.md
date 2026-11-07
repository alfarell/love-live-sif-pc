# Member talk (`talk/talk.pac`)

Voiced lines a member says on the home screens (no on-screen text). Read by `src/game/talk/talk.{h,cpp}`.

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

A `BTL` (version 3) timeline, the same family as the live scene timelines. It is not decoded yet. Its events are 16-byte records `u16 tag, u16 kind, u32 0, u32 value, u32 0`:

- tag `0x0514`, kind 8: play voice cue `value`. The cue name is `vo_ac_%03d_%03d` (member, value), or `vo_ac_%02d_%03d` for members 21 and up. 1380 of the 1840 talks have one; 1374 of those name a cue that exists.
- tag `0x0028` kind 4 and tag `0x0443` kind 4: *Hypothesis:* motion and expression changes of the 3D navigator.

The cues are in `sound/sound.pac` `voice.acb` with the waves in `sound/voice.awb` (AFS2, 10808 waves; 10613 cues `vo_ac_*` and `vo_ps_*`). See `cri.md`.
