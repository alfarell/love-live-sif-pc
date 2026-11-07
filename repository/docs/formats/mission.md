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
