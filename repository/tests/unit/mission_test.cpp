// Missions (src/game/mission/mission.cpp) against the user's own dump: table sizes, rows, condition text, progress.
#include <algorithm>
#include <cstdio>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/card/card.h"
#include "game/costume/costume.h"
#include "game/friendship/friendship.h"
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

static const mission::Mission* ById(const mission::Missions& all, int id) {
    for (const auto& m : all.rows) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sif_mission_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    mission::Missions all;
    std::string error;
    Check(mission::Load(vfs, all, &error), "load: " + error);
    Check(all.rows.size() == 200 + 1625 + 769 + 1464, "4058 mission rows");
    Check(all.conditions.size() == 49, "49 condition kinds");
    if (all.conditions.size() == 49) {
        Check(all.conditions[0].title[1] == "Play a Live Show!", "condition 0 English title");
        Check(all.conditions[38].text[1] == "Achieve a FULL COMBO or better on \"[p1]\".", "condition 38 English text");
    }
    const mission::Mission* song = ById(all, 2000);
    const mission::Mission* ten = ById(all, 1);
    Check(song && song->Kind() == 35 && song->p1 == 10 && song->p2 == 1, "mission 2000: play song 0010 once");
    Check(ten && ten->Kind() == 0 && ten->p2 == 10, "mission 1: play 10 Live Shows");
    if (song && ten && all.conditions.size() == 49) {
        Check(mission::Fill(all.conditions[35].text[1], *song, "START:DASH!!") ==
                  "Play a Live Show of \"START:DASH!!\" 1 times.",
              "fill [p1] and [p2]: " + mission::Fill(all.conditions[35].text[1], *song, "START:DASH!!"));
        save::SaveData data;
        Check(!mission::ProgressOf(*song, data).Done(), "empty save: 2000 not done");
        save::Record(data, {10, 4, 500000, 300, 4, 0.9, true});
        Check(mission::ProgressOf(*song, data).Done(), "one play of 0010: 2000 done");
        save::Add(data, save::GroupKey(0, "plays"));  // main.cpp feeds the group counters after a live (M15 Q3)
        const auto p = mission::ProgressOf(*ten, data);
        Check(p.have == 1 && p.need == 10 && !p.Done(), "one play: mission 1 at 1/10");
        save::SaveData fresh;
        Check(!mission::Claim(*song, fresh), "empty save: 2000 cannot be claimed");
        Check(mission::Claim(*song, data) && !mission::Claim(*song, data), "2000 claimed once");
        Check(data.claimed.contains(2000) && data.owned[{song->reward_type, song->reward_id}] == std::max(1, song->reward_amount),
              "the claim gives 2000's reward");
        const auto owned = mission::Owned(data, song->reward_type);
        Check(std::find(owned.begin(), owned.end(), song->reward_id) != owned.end(), "Owned lists it");
        save::Record(data, {10, 4, 500000, 300, 4, 0.9, true, 250, 40, 31});
        save::Add(data, save::GroupKey(0, "perfects"), 250);
        save::Add(data, save::GroupKey(0, "greats"), 40);
        for (const mission::Mission& m : all.rows) {
            if (m.Kind() == 6) Check(mission::ProgressOf(m, data).have == (m.p1 == 1 ? 0 : 250), "kind 6: the group's PERFECTs");
            if (m.Kind() == 5) Check(mission::ProgressOf(m, data).have == (m.p1 == 1 ? 0 : 40), "kind 5: the group's GREATs");
            if (m.Kind() == 41) {
                Check(mission::ProgressOf(m, data).have == (m.p1 == 31 ? 1 : 0), "kind 41: plays wearing the costume");
            }
        }
    }
    std::vector<mission::Award> accolades, badges;
    Check(mission::LoadAwards(vfs, accolades, badges, &error), "awards: " + error);
    Check(accolades.size() == 1665 && badges.size() == 229, "1665 accolades, 229 badges");
    const mission::Award* debut = mission::FindAward(accolades, 1002);
    const mission::Award* love = mission::FindAward(badges, 901001);
    // "デビューシングル" (debut single) and "大好き" (love), written as UTF-8 bytes
    Check(debut && debut->name == "\xe3\x83\x87\xe3\x83\x93\xe3\x83\xa5\xe3\x83\xbc\xe3\x82\xb7\xe3\x83\xb3\xe3\x82\xb0\xe3\x83\xab",
          "accolade 1002 (debut single)");
    Check(love && love->group == 901 && love->color == 0xff8636c5u &&
              love->name.find("\xe5\xa4\xa7\xe5\xa5\xbd\xe3\x81\x8d") != std::string::npos,
          "badge 901001: category 901, colour, name");
    const mission::Mission* second = ById(all, 2001);
    Check(second && second->reward_type == 4 && second->reward_id == 1002, "mission 2001 gives accolade 1002");

    // M14: the login, booster, Friendship and birthday kinds, and reward type 3 (Wai-Wai! Boosters)
    friendship::Tables levels;
    Check(friendship::Load(vfs, levels), "Friendship tables");
    save::SaveData m14;
    m14.logins = 3, m14.boosters_used = 1, m14.boosters = 8;
    m14.bonds[11] = {811, 2};  // level 4
    m14.birthdays[11] = {2026, 0, false};
    const mission::Mission login{1, 18, 0, 3}, boost{2, 30, 11, 2}, bond{3, 32, 11, 4, 3, 5003, 3}, bd{4, 34, 11, 1};
    Check(mission::ProgressOf(login, m14).Done() && !mission::ProgressOf(boost, m14).Done(), "kinds 18 (3 logins) and 30 (1 of 2)");
    const mission::Context with_levels{&levels};
    Check(mission::ProgressOf(bond, m14, &with_levels).Done() && !mission::ProgressOf(bond, m14).Done() &&
              mission::ProgressOf(bd, m14).Done() && !mission::ProgressOf({5, 34, 12, 1}, m14).Done(),
          "kinds 32 (Friendship Lv. 4) and 34 (Chika's event seen)");
    Check(mission::Claim(bond, m14, &with_levels) && m14.boosters == 9 && !m14.owned.contains({3, 5003}),
          "reward type 3 adds boosters, up to 9");

    // M15 Q4: the remaining kinds, on small made-up tables (ids are not the game's)
    card::Data cards;
    cards.skills = {{.id = 11001, .member = 1, .max_level = 10}, {.id = 11002, .member = 1, .max_level = 1},
                    {.id = 21001, .member = 2, .max_level = 10}};
    costume::Data costumes;
    costumes.items = {{.id = 29, .type = 1}, {.id = 30, .type = 1}, {.id = 55, .type = 2}};
    costumes.recipes = {{.dress = 31, .flags = 1}, {.dress = 32, .flags = 1}, {.dress = 105, .flags = 1}, {.dress = 201, .flags = 1}};
    mission::Missions table;
    table.rows = {{9001, 22, 1, 1, 4, 2501}, {9002, 24, 1, 1, 4, 2502}, {9003, 22, 2, 1, 4, 2503}};
    const mission::Context c{&levels, &cards, &costumes, nullptr, &table};
    save::SaveData q4;
    save::Add(q4, "g1.plays", 5);
    save::Best(q4, "g1.best_rank", 5);
    save::Add(q4, "g0.full_combos");
    q4.owned[{1, 11001}] = 10, q4.owned[{1, 11002}] = 1;
    q4.owned[{7, 32}] = 1, q4.owned[{7, 201}] = 1;
    q4.owned[{4, 2501}] = 1, q4.owned[{4, 2503}] = 1;
    save::Mark(q4, "keys", 29);
    save::Mark(q4, "keys", 30);
    save::Mark(q4, "talk.1", 7);
    save::Mark(q4, "menu.1", 31);
    save::Mark(q4, "menu.1", 32);
    save::Mark(q4, "camera_photo", 1);
    save::Mark(q4, "view_costumes", 31);
    save::Mark(q4, "card_costumes", 32);
    save::Mark(q4, "special", 105);
    save::Add(q4, "camera.1", 130);
    save::Add(q4, "view.10", 2);
    save::Add(q4, "photo.31", 3);
    save::Add(q4, "views", 4);
    save::Add(q4, "photos", 6);
    save::Add(q4, "cards.1", 2);
    save::Add(q4, "collected.thread", 44);
    q4.bonds[1] = {0, 3};
    const auto have = [&](int kind, int p1) { return mission::ProgressOf({1, kind, p1, 1}, q4, &c).have; };
    const auto done = [&](int kind, int p1) { return mission::ProgressOf({1, kind, p1, 1}, q4, &c).Done(); };
    Check(have(0, 1) == 5 && have(0, 0) == 0 && done(2, 1) && !done(3, 1) && done(1, 0) && !done(1, 1),
          "kinds 0-3 read the row's group");
    Check(have(7, 0) == 1 && have(7, 1) == 1, "kind 7: owned costumes per group (Saint Snow with Aqours)");
    Check(have(9, 0) == 44 && have(12, 0) == 4 && have(13, 0) == 6, "kinds 9, 12, 13: totals");
    Check(done(11, 0) && !done(11, 1), "kind 11: every key item of the group");
    Check(have(20, 1) == 3 && have(21, 1) == 1 && have(24, 1) == 2 && have(26, 1) == 2 && have(27, 1) == 2,
          "kinds 20, 21, 24, 26 (minutes), 27");
    Check(have(22, 1) == 2 && !done(23, 2) && done(23, 1) && have(31, 1) == 1, "kinds 22, 23, 31: her skill cards");
    Check(mission::ProgressOf({1, 28, 1, 1}, q4, &c).need == 2 && done(28, 1), "kind 28: every µ's costume shown");
    Check(have(29, 1) == 1 && have(29, 2) == 1, "kind 29: the accolades her member missions give");
    Check(done(33, 1) && !done(33, 2) && have(36, 10) == 2 && done(40, 32) && !done(40, 31) && done(42, 32) &&
              done(43, 31) && have(45, 31) == 3 && done(46, 32) && done(48, 105),
          "kinds 33, 36, 40, 42, 43, 45, 46, 48");
    Check(!mission::ProgressOf({1, 22, 1, 1}, q4).Done(), "without the tables a kind that needs them stays open");
    std::printf("%s\n", failures ? "mission_test FAILED" : "mission_test ok");
    return failures ? 1 : 0;
}
