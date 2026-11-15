# Skill cards and member cards (`common/common.pac/card.pac`)

Status:
- **Read** by `src/game/card/card.cpp` (M11 S3).
- **Sources:** the eboot trace (`analysis/202611151530-skills-and-cards/202611151548-analysis.md`) and a shadPS4 run (`202611151632-analysis.md`).

All tables are BLBN (`blbn.md`). They are little-endian, and their string fields are u64 file offsets of UTF-8 text.

## `skill_card_datas.bin`

**`s_skill_card_datas`:** 415 records of 128 bytes.

| Offset | Content |
|---|---|
| +0x00 | card id = member × 10000 + series × 1000 + type (`11000` … `223027`). Series 1 live skills, 2 material and booster skills, 3 friendship, camera and shoutout skills |
| +0x04 | skill number, shown as `S-10144` |
| +0x08 | member (1-9 µ's, 11-19 Aqours, 21-22 Saint Snow) |
| +0x10 | effect kind (below) |
| +0x18 … +0x30 | name: JP, EN, CN, TW |
| +0x38 … +0x50 | description: JP, EN, CN, TW |
| +0x58 | picture name (`skc_1100_chara`) |
| +0x64, +0x68 | 58, 58 (not decoded) |
| +0x6C | maximum level: 10, or 1 for skills that do not grow |
| +0x70 | file offset of the level table |
| +0x78 | 2000 or a larger value (not decoded) |
| +0x7C | byte 1 on the 130 cards that never drop (task and Birthday Event cards) |

**Level table:** 11 rows of 48 bytes; row 0 repeats row 1, so levels are 1-10. The symbols `s_skill_card_level_params_0` … `_414` name these tables.

| Offset | Content |
|---|---|
| +0x00 f32 | duration in seconds of a timed skill (×1000 → ms); −1 = not timed ("[Automatic activation]") |
| +0x04 i32 | amount: a use count (Finale Support: icons, Starting Combo Support: 1) or a percent/points value |
| +0x08 i32 | share in % when the card belongs to a support profile card (50 or 25; 0 = full) |
| +0x0C i32 | an alternative amount (kind 19) |
| +0x10 … +0x28 | level text: JP, EN, CN, TW ("Enhances timing window for 5 seconds.") |

**Effect kinds:** the eboot's names at `0x5E9BE9`; kinds with a card:

| Kind | Eboot name | English card name |
|---|---|---|
| 0 | extendEvaluation | Judge Plus |
| 1 | extendCombo | Combo Support |
| 6 | appealCamera | Member Focus |
| 13 | decisionSoftening | Judge Support |
| 14 | autoFinaleIcon | Finale Support |
| 15 | autoLongNortEnd | Long Notes Support |
| 16 | startVoiceCall | Shoutout |
| 17 | startComboSoftening | Starting Combo Support |
| 18-21 | favorabilityRatingInc … perfectFavorabilityBonus | Friendship Points skills |
| 22, 23 | oshimenItemInc, oshimenItemRate | Wai-Wai! Booster Boost, Drop Support |
| 24, 25 | oshimenSkillRateInc, skillAddRate | Skill Drop Boost, Getting Skills Charge |
| 27-35 | costumeItemRateInc … costumeItemFinaleFocus | material skills |
| 36, 37 | effectShower, effectFrame | Special Effect Shower, Special Effect Frame |
| 38 | judgeSupportPlus | Judge Support + |

**Constants** (`ll3::s_SKILL_LEVEL_MIN/MAX/NUM`, `s_INVALID_SKILL_ID`): 1, 10, 10, 0.

## `member_card_datas.bin`

**`s_member_card_datas`:** 1,038 records of 28 bytes. There is one card per member and costume, each in two pictures; the costume step shows them.

| Offset | Content |
|---|---|
| +0x00 | card id = costume key × 10 + picture (`10011`, `10010`) |
| +0x04 | index 1-1038 |
| +0x08 | picture kind 2 or 1 (not decoded) |
| +0x0C | costume key = member × 1000 + costume number |
| +0x10 | member |
| +0x14 | costume number |
| +0x18 | 0, or 2 on 20 cards (not decoded) |

## `preset_profilecard.bin`

**`ll3::s_preset_profilecard`:** 20 records of 96 bytes. These are the NPC profile cards that support an offline live: index, member card id (`10031`), then skill slots that are empty in all presets.

## `reward.pac/reward_data.bin`

**`ll3::reward::s_reward_datas`:** 404 records of 20 bytes.

| Offset | Content |
|---|---|
| +0x00 | id |
| +0x04 | pool |
| +0x10 | item |

**Pools:**
- **0:** the 285 skill cards a live can drop (19 per µ's member, 12 per Aqours member, 3 per Saint Snow member);
- **1:** Wai-Wai! Boosters, one per member;
- **2, 3:** not decoded.

## Save (the game's, for reference)

- **Owned skill cards:** 16-byte entries (card id, level, …) at save `+0x466c`, with the count at `+0x628c` (room for 450).
- **Set cards:** three per member, at `+0x8fd7c + member × 0xdc`.

**The port's save** (`save.txt`):
- **Copies:** `own 1 <card id> <copies>`. The level is min(copies, maximum level).
- **Set cards:** `skills <member> <card> <card> <card>`, with 0 for an empty slot.
