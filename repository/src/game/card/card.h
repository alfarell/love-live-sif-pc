#pragma once
// Skill cards and member cards (M11, plan/202611151530-m11-skills-and-cards/): the card tables of
// common/common.pac/card.pac, the skill card drop pool, and what the player owns and has set. See
// docs/formats/card.md and analysis/202611151530-skills-and-cards/ (S1: eboot, S2: shadPS4).

#include <array>
#include <random>
#include <string>
#include <vector>

namespace sif {
class Vfs;
namespace save {
struct SaveData;
}

namespace card {

constexpr int kSkillCardReward = 1;  // mission reward type 1 = a skill card (save.owned key {1, card id} = copies)
constexpr int kSlots = 3;            // skill cards set per member (Prepare for the Live Show)

// Effect kinds (+0x10; the eboot's names at 0x5E9BE9). Only the kinds a card has are listed.
enum Kind : int {
    kJudgePlus = 0,            // extendEvaluation: Great -> Perfect, Good -> Great while active
    kComboSupport = 1,         // extendCombo: a Good keeps the combo while active
    kMemberFocus = 6,          // appealCamera
    kJudgeSupport = 13,        // decisionSoftening: the +20 ms windows while active
    kFinaleSupport = 14,       // autoFinaleIcon: missed Rhythm Icons collected, `amount` times
    kLongNotesSupport = 15,    // autoLongNortEnd: holds pressed while active end as Perfect
    kShoutout = 16,            // startVoiceCall
    kStartingComboSupport = 17,  // the +20 ms windows until the first combo break
    kDropSupport = 23,         // oshimenItemRate: % chance of a Wai-Wai! Booster
    kSkillDropBoost = 24,      // oshimenSkillRateInc: + weight of the played member in the skill card draw
    kGettingSkillsCharge = 25,  // skillAddRate: % chance of a second skill card at rank S or better
    kEffectShower = 36,
    kEffectFrame = 37,
    kJudgeSupportPlus = 38,    // the +20 ms windows, and a Good keeps the combo
};

// One level row (48 bytes, levels 1-10; the table's row 0 repeats row 1).
struct LevelRow {
    float seconds = -1;  // duration of a timed skill; < 0: not timed ("[Automatic activation]")
    int amount = 0;      // a use count or a percent/points value, by kind
    int share = 0;       // % that counts when the card belongs to a support profile card (0 = all)
    int amount_alt = 0;  // used instead of `amount` by kind 19
    std::array<std::string, 4> text;  // "5秒間ノーツ判定強化" ... (JP, EN, CN, TW)
};

// s_skill_card_datas: 128-byte records.
struct SkillCard {
    int id = 0;        // member x 10000 + series x 1000 + type (11000 ... 223027)
    int skill = 0;     // the "S-10144" number on the card
    int member = 0;    // 1-9 µ's, 11-19 Aqours, 21-22 Saint Snow
    int kind = 0;      // Kind
    int max_level = 1; // +0x6C: 10, or 1 for skills that do not grow
    bool no_drop = false;  // +0x7C: task and Birthday Event cards, never in the live drop pool
    std::array<std::string, 4> name, text;  // JP, EN, CN, TW
    std::string picture;                    // "skc_1100_chara"
    std::vector<LevelRow> levels;              // [0] = level 1
    bool Timed() const { return !levels.empty() && levels[0].seconds > 0; }
    const LevelRow& At(int level) const;       // clamped to 1..max_level
};

// s_member_card_datas: 28-byte records, one card per member and costume in two pictures.
struct MemberCard {
    int id = 0;       // costume key x 10 + picture (10011, 10010)
    int picture = 0;  // 2 or 1 (the meaning is not decoded)
    int member = 0;
    int costume = 0;  // the costume number within the member (costume key = member x 1000 + costume)
};

struct Data {
    std::vector<SkillCard> skills;   // table order
    std::vector<MemberCard> members;
    std::vector<int> drop_pool;      // ll3::reward::s_reward_datas pool 0: the skill cards a live can drop
};

bool Load(const Vfs& vfs, Data& out, std::string* error = nullptr);
const SkillCard* FindSkill(const Data& data, int id);

// The level of an owned skill card: the first copy is level 1 and each further copy adds one, up to its maximum
// (eboot FUN_0007cf30); 0 = not owned.
int Level(const Data& data, const save::SaveData& save, int id);
// Adds one copy (a task reward or a live drop) and returns the new level.
int AddSkill(const Data& data, save::SaveData& save, int id);
// The owned skill cards of one member, in table order.
std::vector<const SkillCard*> OwnedSkills(const Data& data, const save::SaveData& save, int member);
// A member's three set cards (0 = empty). Cards no longer owned or of another member read as empty.
std::array<int, kSlots> Slots(const Data& data, const save::SaveData& save, int member);
// The sum of the level-row amounts of `member`'s set cards of `kind` (eboot FUN_000caaa0; the played member's cards
// count in full, support players' at their share, which the port leaves out).
int SkillSum(const Data& data, const save::SaveData& save, int member, int kind);

// After-live skill card drop (eboot FUN_000a68d0 -> FUN_001f2ab0 pool 0; analysis 202611151530-skills-and-cards/):
// one member of the played member's group by weight (µ's 11 each, Aqours and Saint Snow 9 each, the played member
// + `boost`), then one of that member's cards in drop_pool uniformly. 0 if the member has none.
int DrawSkillCard(const Data& data, int member, int boost, std::mt19937& random);
// The cards one finished live drops: one draw (Skill Drop Boost as `boost`), and a second one at rank S or better
// (`rank` > 3) with Getting Skills Charge % chance. Does not change the save (AddSkill each).
std::vector<int> RollSkillDrops(const Data& data, const save::SaveData& save, int member, int rank, std::mt19937& random);

}
}
