# Costume materials, recipes and drops

Reader: `src/game/costume/costume.{h,cpp}`; test `tests/unit/costume_test.cpp`. Found in M7 (`analysis/202611021940-m7-systems-breadth/202611071952-analysis.md`).

All three are BLBN tables (`blbn.md`) in `common/common.pac`.

## Items: `item.pac/item_data.bin`, `ll3::item::s_item_datas`

62 rows of 40 bytes; row 0 is "none" (無し) and the last is "max".

| Offset | Field |
|---|---|
| +0x00 | id (1–60) |
| +0x04 | type: 1 µ's key item, 2 Aqours key item, 3 common thread, 4 common cloth, 5 µ's thread, 6 µ's cloth, 7 Aqours thread, 8 Aqours cloth |
| +0x08 | 4 × u64 name offset: Japanese, English, simplified Chinese, traditional Chinese |

Icon: `ui/ui_material_icon.pac/texture.pac/material_icon_<kind>_<group>_NNN` (NNN = id). The kind and group come from the type: `key_ms`, `key_aq`, `yarn_com`, `clo_com`, `yarn_ms`, `clo_ms`, `yarn_aq`, `clo_aq`.

`item_combine_data.bin` (`ll3::item::s_itemCombineDatas`) is a 5 × 5 float table, [costume type][song type] (dress +0x04 by `s_music_datas` +0x158). It is 1.5 on the diagonal and falls to 1.0. Every chance of the material drop is multiplied by it (M11 S6b).

## Recipes: `dress.pac/dress_data.bin`, `ll3::dress::s_dress_datas`

192-byte rows after an empty first one (70 costumes).

| Offset | Field |
|---|---|
| +0x00 | costume id (`chr/cNN/dNNN`) |
| +0x04 | costume type 0–4: which pair of key materials it favours (messages 803–812) |
| +0x08 | 4 × name offset |
| +0x30 | 4 × description offset |
| +0x58 | u32 flags (see below) |
| +0x5C | 12 × u32 item id |
| +0x8C | 12 × u32 amount |

Flags (*Hypothesis*, from all 70 rows):
- **Bit 0:** made from materials.
- **Bit 1:** a reward costume.
- **Bit 16:** special colours.
- **Low byte 0:** owned from the start. That matches the "Obtained" mark on a new game: the two School Unity (3, 105) and Believe again (200).

An amount of 999 means the costume is not made from materials.

## Song drops: `live_common.pac/live_datas.bin`, `s_music_datas`

At row +0x15C there is one pair per item id 1–60: f32 chance, i32 cap (low 16 bits), the song's "Obtainable Materials". Row +0x158 is the song type 0–4.
- Every song has 18 non-zero pairs: the common items 1–8 plus its group's (µ's 9–34; Aqours and Saint Snow 35–60).
- Key items have the lowest chances (0.12–0.18).

## The drop after a live (eboot `FUN_000679a0`, M11 S6b)

Ported in `costume::RollDrops`; details in `analysis/202611151530-skills-and-cards/` (S6, S6b).

1. **Units:** N, uniform in the rank's range, `s_CommonParam` +0x150 (min, max) × 7: 5–8 at D up to 36–40 at SSS. A Wai-Wai! Booster adds +0x1F0 (10); not ported.
2. **Weights:** each material's weight is chance × the difficulty factor (+0xF4, 0.8–1.0 by difficulty index) × `s_itemCombineDatas`.
   - Keys get × +0x1A8 (1.2) when the costume type equals the song type. *Hypothesis:* the game's flag (save +0xF0A40 bit 1) is that match.
   - Material skills: see the next table.
3. **Order:** sorted by weight, highest first, with key materials moved to the front.
4. **Pass 1:** rounds over the list. Each try uses one of the material's cap and gives one unit when a uniform roll ≤ its weight. It stops at N units or when every cap is used.
5. **Pass 2:** the units still owed are drawn by weight among the five heaviest.
6. **Conversions:** Converting Threads and Converting Fabric.

**Material skills** (v = the sum of the played member's set cards of the kind):

| Kind | Effect |
|---|---|
| 27 | every weight × (1 + v/100) |
| 28, 29 | Converting Threads / Fabric: ceil(v % of each common yarn / cloth unit gained) moves to the group's yarns / cloths that dropped, spread evenly, the remainder one by one at random |
| 30 | common yarn and cloth weights × (1 + v/100) |
| 31, 32 | with chance v %: the cap of every yarn / cloth × 1.5 |
| 33 | key weights × (1 + v/100) |
| 34 | with chance v %: every key unit gained comes twice |
| 35 | key weights × (1 + v/100), only with the costume match |

## Mission reward types (from `mission.md`)

| Type | Reward |
|---|---|
| 2 | items |
| 7 | costumes |
| 4 | accolades |
| 5 | frames |
| 6 | name plates |

The save keeps all of them as `own <type> <id> <count>`.
