// Missions (src/game/mission/mission.cpp) against the user's own dump: table sizes, rows, condition text, progress.
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
    }
    std::printf("%s\n", failures ? "mission_test FAILED" : "mission_test ok");
    return failures ? 1 : 0;
}
