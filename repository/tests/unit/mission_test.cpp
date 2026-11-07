// Missions (src/game/mission/mission.cpp) against the user's own dump: table sizes, rows, condition text, progress.
#include <algorithm>
#include <cstdio>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
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
        const auto p = mission::ProgressOf(*ten, data);
        Check(p.have == 1 && p.need == 10 && !p.Done(), "one play: mission 1 at 1/10");
        const auto earned = mission::EarnedAwards(all, data);
        Check(std::find(earned.begin(), earned.end(), song->reward_id) != earned.end() || song->reward_type != 4,
              "one play of 0010: mission 2000's award earned");
        Check(mission::EarnedAwards(all, save::SaveData{}).empty(), "empty save: nothing earned");
        save::Record(data, {10, 4, 500000, 300, 4, 0.9, true, 250, 40, 31});
        for (const mission::Mission& m : all.rows) {
            if (m.Kind() == 6) Check(mission::ProgressOf(m, data).have == 250, "kind 6: PERFECT total");
            if (m.Kind() == 5) Check(mission::ProgressOf(m, data).have == 40, "kind 5: GREAT total");
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
    std::printf("%s\n", failures ? "mission_test FAILED" : "mission_test ok");
    return failures ? 1 : 0;
}
