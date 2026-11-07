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

`item_combine_data.bin` (`ll3::item::s_itemCombineDatas`) is a 5 × 5 float table of 1.0–1.5. *Hypothesis:* combine rates. Not used.

## Recipes: `dress.pac/dress_data.bin`, `ll3::dress::s_dress_datas`

192-byte rows after an empty first one (70 costumes).

| Offset | Field |
|---|---|
| +0x00 | costume id (`chr/cNN/dNNN`) |
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

At row +0x15C there is one pair per item id 1–60: f32 chance, i32 amount (*Hypothesis*: the song's "Obtainable Materials" on the song select).
- Every song has 18 non-zero pairs: the common items 1–8 plus its group's (µ's 9–34; Aqours and Saint Snow 35–60).
- Key items have the lowest chances (0.12–0.18).

The port gives each drop with its chance after a finished live. The game's bonuses (for example the costume effect "Materials drop more easily …") are not traced.

## Mission reward types (from `mission.md`)

| Type | Reward |
|---|---|
| 2 | items |
| 7 | costumes |
| 4 | accolades |
| 5 | frames |
| 6 | name plates |

The save keeps all of them as `own <type> <id> <count>`.
