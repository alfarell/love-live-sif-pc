# Skill cards, member cards and profile cards (`common/common.pac/card.pac`)

Status:
- **Read** by `src/game/card/card.cpp` (M11 S3) and `src/game/profile/profile.cpp` (frames and presets, M12 P3).
- **Profile cards and photos:** `analysis/202611161811-profile-cards/` (eboot P1, shadPS4 P2).
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

**`ll3::s_preset_profilecard`:** 20 records of 96 bytes. These are the NPC profile cards that support an offline live ("(NPC Profile Card)"). The fields are as the eboot copies them into a support player (`FUN_001f1210`; reader `src/game/profile/`):

| Offset | Content |
|---|---|
| +0x00 | index 0-19 |
| +0x04 | member card (10031, 111051, 212001; member = id / 10000) |
| +0x08 … +0x1C | three skill card ids, then three levels (all 0) |
| +0x20 | frame (the member's initial frame) |
| +0x24 | live (120, 1120, 1900) |
| +0x28 | Friendship level (0) |
| +0x2C, +0x30, +0x34 | accolade, badge, name plate |
| +0x38 | the card picture: `proca_preset/<name>` in the dump (a whole rendered card, 672 × 1100 JPEG; `_en`, `_cn` and `_tw` variants) |
| +0x40 … +0x58 | the player name: JP, EN, CN, TW ("Maki's fan") |

**Pick for a live** (`FUN_001d63d0`):
- **Candidates:** the presets whose member is in the played group (µ's members 1-9; Aqours and Saint Snow).
- **Two different picks,** each `index = (int)((x >> 16) / 65536 × candidates left)`. `x` comes from a xorshift128 that starts at `{0, 1, 1812433255, 1900727105}` once per game run.
- **Not reproducible:** the generator state is shared with other game code, so the exact picks of a run cannot be predicted. The shadPS4 runs got Nico + Maki and Nico + Umi.

## `profile_card_datas.bin`

**`s_profile_card_frame_datas`:** 183 records of 160 bytes.

| Offset | Content |
|---|---|
| +0x00 | frame id: 1001-1022 the members' initial frames; 1-76 and 5001+ birthday frames; 6011+, 7011+ group frames; 8011+, 9021+ birthday frames |
| +0x04 | 1 = owned from the start (the 20 initial frames) |
| +0x08 | member |
| +0x10 … +0x28 | name: JP, EN, CN, TW ("Honoka Kosaka initial\nProfile Card Frame") |
| +0x30 | a JP note ("shown and chosen only in <member>'s live history") |
| +0x38 … | text colours with on/off flags (*not decoded*) |

**Textures:** `ui/ui_profile_card.pac/texture.pac`, `pfc_bg_NNN` and `pfc_frame_*` (`_h` = landscape, *likely*). The profile card can be portrait or landscape with the same frame.

**Owned frames:** the initial frame, plus task rewards of type 5 (a frame id).

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
- **Photos** (M12): the JPEGs (1920 × 1080) in save slot 1, one per 0x57800 bytes; 30 records of 0x100 bytes at main `+0x60000` (stage, member card, live, favourite, then size, hash and time at +0x80); the count at `+0x61E00`.
- **The profile card** ("MyProca", one in all): a record at main `+0xA0` ("Prof"; frame, live, member card, Friendship level, accolade, badge, name plate, three skill id/level pairs; landscape flag at +0xB8). Its picture (538 × 880 or 880 × 538 JPEG) is in save slot 2.
- **Received cards** ("OtherProca"): the same fields from main `+0x5CD24` (online only).

**The port's save** (`save.txt`):
- **Copies:** `own 1 <card id> <copies>`. The level is min(copies, maximum level).
- **Set cards:** `skills <member> <card> <card> <card>`, with 0 for an empty slot.
- **Photos:** `photo <slot> <time> <stage> <member card> <live> <favourite>`; the picture is `photos/<slot, 2 digits>.png` in the data folder. A new photo takes the lowest free slot, else the oldest that is not a favourite.
- **Profile card:** `profile_card <frame> <landscape> <crop> <live> <member card> <accolade> <badge> <name plate>`, then three `<skill> <level>` pairs. Its photo is copied to `profile_card.png` when it is saved.
