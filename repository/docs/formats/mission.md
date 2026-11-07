# Missions (`common/common.pac/mission.pac`)

Read by `src/game/mission/mission.{h,cpp}`. All five files are `BLBN` tables (see `blbn.md`), each with one symbol.

## Mission rows

Four tables share one row layout: 28 bytes, seven 32-bit integers.

| File | Symbol | Rows | Ids | Condition kinds |
|---|---|---|---|---|
| `mission_common.bin` | `ll3::mission::s_mission_common_datas` | 200 | 0-199 | 0-19 (general) |
| `mission_music.bin` | `ll3::mission::s_mission_music_datas` | 1625 | 2000-3624 | 35-39 (per song, 13 per song) |
| `mission_dress.bin` | `ll3::mission::s_mission_dress_datas` | 769 | 4000-4768 | 40-48 (per costume) |
| `mission_recommended.bin` | `ll3::mission::s_mission_recommended_datas` | 1464 | 300-1763 | 20-34 (per member) |

| Field | Meaning |
|---|---|
| id | mission id |
| code | low byte = condition kind (index into `s_mission_condition`); high byte 0-3 not decoded (*Hypothesis:* 1 = counted, 2/3 = first of a series) |
| p1 | song id (kinds 35-39), member (20-34), costume (40-48). In the common table it is 0 for rows 0-86 and 1 for rows 87-199 (*Hypothesis:* the group, µ's or Aqours: the reward ids are 71xxxx and 72xxxx) |
| p2 | the target count or score (`[p2]` in the text) |
| reward type, id, amount | *Hypothesis:* type 2 item, 3 booster, 4 accolade or badge, 6 card |

## `mission_condition.bin`

`ll3::mission::s_mission_condition`: 49 kinds × 9 u64. The first 8 are file offsets of UTF-8 strings: titles in Japanese, English, simplified Chinese and traditional Chinese, then the descriptions in the same order. The 9th is a flag word (`0x10200`, `0x200`, `0x10300`, …; *Hypothesis:* the high part says whether the text has `[p2]`, the middle byte the row table). The texts use `[p1]` and `[p2]` for the row's parameters.

Kinds, by their English titles:

- 0-19 (general): Live Shows played, FULL COMBO, rank S, rank SSS, score, GREAT and PERFECT totals, costumes, costume items, viewing mode, medleys, profile cards, HR cards, login bonuses, likes.
- 20-34 (member): plays with the member, phrases heard, skill cards, profile cards, Finale Focus, member camera, costumes, accolades, boosters, Friendship level, photos, birthday.
- 35-39 (song): plays, viewings, medley, FULL COMBO, score.
- 40-48 (costume): obtained, plays wearing it, main menu, viewing, profile card, photos, medley, special colour.

## Awards

Mission reward type 4 names an accolade: 1642 of the 1651 type-4 rows match an accolade id. Names are Japanese only: each id has one text symbol, and there are no language packages.

- **Accolades** (`common/common.pac/honorific_title.pac/honorific_title_datas.bin`):
  - `s_honorific_title_datas`: 1665 rows of 40 bytes. +0x00 id, +0x04 1, +0x08 *Hypothesis:* plate id (100, 110, 1802, …), +0x10 u64 file offset of the name (`s_title_text_<id>`), +0x18 four u32 not decoded.
  - `s_honorific_title_plate_datas`: 16-byte rows (id, ARGB colour, two u32); *Hypothesis:* the plate colours.
- **Badges** (`common/common.pac/badge.pac/badge_datas.bin`):
  - `s_badge_datas`: 229 rows of 32 bytes. +0x00 id, +0x04 category (id / 1000, e.g. 901 for 901001 "μ’s大好き"), +0x08 u64 name offset (`s_text_<id>`), +0x10 ARGB colour, +0x14 *Hypothesis:* group mask (1 µ's, 2 Aqours, 4 Saint Snow), +0x18 flags.
  - Reward type 3 ids also match badge ids (120 rows). Badges may come from Friendship levels instead (message: "Raise their levels to get different Badges!"); not checked.
