// Live Show Medley (src/game/medley) against the user's own dump: the 20 recommended Set Lists as read in R1
// (work/m16/medley_rows.txt) and seen in the game (R2: Aqours "Wai-Wai! Live", Sunshine Arena, four songs).
#include <cstdio>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/medley/medley.h"

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
        std::printf("usage: sif_medley_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    std::string error;

    medley::Data d;
    Check(medley::Load(vfs, d, &error), "medley_data: " + error);
    Check(d.lists.size() == 20, "20 recommended lists");
    const auto mu = medley::OfGroup(d, 0), aq = medley::OfGroup(d, 1);
    Check(mu.size() == 10 && aq.size() == 10 && mu[0]->id == 1 && aq[0]->id == 11, "10 per group, in id order");
    if (d.lists.size() == 20) {
        const medley::SetList& w = d.lists[0];
        Check(w.name[0] == "わいわい！ライブ" && w.name[1] == "Wai-Wai! Live" && w.group == 0 && w.stage == 1,
              "list 1: its names, µ's, stage 1");
        Check(w.entries.size() == 4 && w.entries[0] == medley::Entry{120, 3, 0} && w.entries[3] == medley::Entry{130, 3, 0},
              "list 1: songs 120 340 670 130 in costume 3");
        const medley::SetList& a = d.lists[10];
        Check(a.name[1] == "Wai-Wai! Live" && a.group == 1 && a.stage == 102 && a.entries.size() == 4 &&
                  a.entries[0].song == 1120 && a.entries[0].costume == 105,
              "list 11 (as in shadPS4): Aqours, Sunshine Arena (st102), four songs in School Unity (105)");
        Check(d.lists[1].entries.size() == 20 && d.lists[3].entries.size() == 13, "a full list and a list of 13");
        const medley::SetList& c = d.lists[11];
        Check(c.entries[4].colour == 3 && c.entries[7].colour == 4 && c.entries[9].colour == 5 && c.entries[0].colour == 0,
              "list 12: the special colours per entry");
        Check(d.lists[6].entries[16].colour == 1 && d.lists[6].entries[15].colour == 0, "list 7: colour 1 on the last four");
        bool stages = true;
        for (const medley::SetList& l : d.lists) {
            stages = stages && vfs.Exists("stage/" + medley::StageFolder(l.stage) + ".pac/model.pac");
        }
        Check(stages, "every list's stage is a stage folder of the dump");
    }
    Check(medley::StageFolder(1) == "st001" && medley::DefaultStage(0) == 1 && medley::DefaultStage(1) == 102,
          "stage folders and the default stages");
    Check(medley::MissionCostume(217) == 117 && medley::MissionCostume(105) == 105, "kind 47's costume (217 as 117)");

    std::printf("%s\n", failures ? "medley_test FAILED" : "medley_test ok");
    return failures ? 1 : 0;
}
