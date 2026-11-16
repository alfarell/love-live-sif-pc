// Skill cards and member cards (src/game/card/card.cpp) against the user's own dump, and the save's skill slots.
#include <cmath>
#include <cstdio>
#include <random>
#include <filesystem>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/card/card.h"
#include "game/mission/mission.h"
#include "game/save/save.h"

using namespace sif;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sif_card_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    card::Data data;
    std::string error;
    Check(card::Load(vfs, data, &error), "load: " + error);
    Check(data.skills.size() == 415, "415 skill cards: " + std::to_string(data.skills.size()));
    Check(data.members.size() == 1038, "1038 member cards: " + std::to_string(data.members.size()));
    Check(data.drop_pool.size() == 285, "285 cards in the drop pool: " + std::to_string(data.drop_pool.size()));

    // Honoka's Judge Support: 5 s at level 1 to 14 s at level 10, amount 82 -> 100, support share 50 %
    const card::SkillCard* judge = card::FindSkill(data, 11000);
    Check(judge && judge->member == 1 && judge->kind == card::kJudgeSupport && judge->max_level == 10 &&
              judge->name[1] == "Judge Support" && judge->picture == "skc_1100_chara" && judge->levels.size() == 10,
          "card 11000 Judge Support");
    Check(judge && judge->Timed() && judge->At(1).seconds == 5 && judge->At(1).amount == 82 && judge->At(1).share == 50 &&
              judge->At(10).seconds == 14 && judge->At(10).amount == 100 && judge->At(99).seconds == 14,
          "11000 levels 1 and 10 (clamped)");
    Check(judge && judge->At(1).text[1] == "Enhances timing window\nfor 5 seconds.", "11000 level 1 text");
    // Finale Support is untimed: [Automatic activation], amount = icons collected (1 at level 1, 9 at level 10)
    const card::SkillCard* finale = card::FindSkill(data, 11003);
    Check(finale && finale->kind == card::kFinaleSupport && !finale->Timed() && finale->At(1).amount == 1 &&
              finale->At(10).amount == 9, "card 11003 Finale Support");
    // Converting Threads (the card the shadPS4 save owns, S-10144)
    const card::SkillCard* threads = card::FindSkill(data, 12016);
    Check(threads && threads->skill == 10144 && threads->name[1] == "Converting Threads", "card 12016 is S-10144");

    // the 130 flagged cards are exactly the cards outside the drop pool (S1)
    int flagged = 0, flagged_in_pool = 0;
    for (const card::SkillCard& c : data.skills) {
        flagged += c.no_drop;
        for (int id : data.drop_pool) flagged_in_pool += c.no_drop && id == c.id;
    }
    Check(flagged == 130 && flagged_in_pool == 0, "130 cards never drop: " + std::to_string(flagged));

    // member cards: one per member and costume in two pictures (10011, 10010 = Honoka, costume 1)
    Check(data.members[0].id == 10011 && data.members[0].member == 1 && data.members[0].costume == 1 &&
              data.members[1].id == 10010, "member cards 10011, 10010");

    // task rewards of type 1 are skill cards
    mission::Missions missions;
    Check(mission::Load(vfs, missions, &error), "missions: " + error);
    int type1 = 0, known = 0;
    for (const mission::Mission& m : missions.rows) {
        if (m.reward_type != card::kSkillCardReward) continue;
        ++type1;
        known += card::FindSkill(data, m.reward_id) != nullptr;
    }
    Check(type1 == 92 && known == 92, "92 task rewards are skill cards: " + std::to_string(known) + "/" + std::to_string(type1));

    // copies -> level, capped at the card's maximum
    save::SaveData save;
    Check(card::Level(data, save, 11000) == 0, "not owned: level 0");
    Check(card::AddSkill(data, save, 11000) == 1 && card::AddSkill(data, save, 11000) == 2, "copies raise the level");
    for (int k = 0; k < 12; ++k) card::AddSkill(data, save, 11000);
    Check(card::Level(data, save, 11000) == 10, "level capped at 10");
    card::AddSkill(data, save, 11019);  // Starting Combo Support: max level 1
    card::AddSkill(data, save, 11019);
    Check(card::Level(data, save, 11019) == 1, "11019 stays at level 1");
    Check(card::OwnedSkills(data, save, 1).size() == 2 && card::OwnedSkills(data, save, 2).empty(), "Honoka owns two cards");

    // slots: a card of another member or not owned reads as empty
    save.skill_slots[1] = {11000, 21000, 11001};
    const auto slots = card::Slots(data, save, 1);
    Check(slots[0] == 11000 && slots[1] == 0 && slots[2] == 0, "slots keep only owned cards of the member");

    // the save keeps the slots
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "sif_card_test_save.txt";
    Check(save::Write(file, save, &error), "write: " + error);
    save::SaveData back;
    Check(save::Load(file, back, &error) && back.skill_slots == save.skill_slots && back.owned == save.owned,
          "save round trip of skill slots and copies");
    std::filesystem::remove(file);

    // after-live drops (S6): member weights 11 (µ's) / 9 (Aqours, Saint Snow), + Skill Drop Boost for the played member
    std::mt19937 random(12345);
    const auto member_share = [&](int member, int boost, int target) {
        int hits = 0, wrong_group = 0;
        constexpr int kDraws = 20000;
        for (int i = 0; i < kDraws; ++i) {
            const card::SkillCard* c = card::FindSkill(data, card::DrawSkillCard(data, member, boost, random));
            if (!c || (member <= 9) != (c->member <= 9) || c->no_drop) ++wrong_group;
            if (c && c->member == target) ++hits;
        }
        return wrong_group ? -1.0 : double(hits) / kDraws;
    };
    const double plain = member_share(1, 0, 1), boosted = member_share(1, 50, 1), aqours = member_share(11, 0, 21);
    Check(std::abs(plain - 1.0 / 9) < 0.01, "µ's: each member 1/9 of the draws: " + std::to_string(plain));
    Check(std::abs(boosted - 61.0 / 149) < 0.015, "boost 50: the played member 61/149: " + std::to_string(boosted));
    Check(std::abs(aqours - 1.0 / 11) < 0.01, "Aqours draw: Saint Snow's Sarah 1/11, never µ's: " + std::to_string(aqours));
    const card::SkillCard* charge = nullptr;
    for (const card::SkillCard& c : data.skills) {
        if (!charge && c.member == 1 && c.kind == card::kGettingSkillsCharge) charge = &c;
    }
    Check(charge != nullptr, "Honoka has a Getting Skills Charge card");
    if (charge) {
        save::SaveData s;
        for (int k = 0; k < 10; ++k) card::AddSkill(data, s, charge->id);  // level 10
        s.skill_slots[1] = {charge->id, 0, 0};
        const int pct = card::SkillSum(data, s, 1, card::kGettingSkillsCharge);
        int two_at_s = 0, two_at_a = 0;
        for (int i = 0; i < 10000; ++i) {
            two_at_s += card::RollSkillDrops(data, s, 1, 4, random).size() == 2;
            two_at_a += card::RollSkillDrops(data, s, 1, 3, random).size() == 2;
        }
        Check(pct == charge->At(10).amount && std::abs(two_at_s / 100.0 - pct) < 1.5 && two_at_a == 0,
              "Getting Skills Charge " + std::to_string(pct) + "%: a second card at rank S (" + std::to_string(two_at_s) +
                  " of 10000), never at A");
    }

    std::printf(failures ? "card_test FAILED\n" : "card_test OK\n");
    return failures ? 1 : 0;
}
