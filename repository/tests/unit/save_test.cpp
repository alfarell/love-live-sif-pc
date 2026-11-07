// The port's own save (src/game/save/save.cpp): round trip, update rule, kept unknown lines, missing file.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "game/save/save.h"

using namespace sif::save;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "sif_save_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "save.txt";

    SaveData empty;
    Check(Load(path, empty) && empty.records.empty() && empty.player == "Player", "missing file: an empty save");

    SaveData s;
    s.player = "Ayumu Uehara";
    NewBests n = Record(s, {10, 4, 150000, 300, 2, 0.75, false});
    Check(n.score && n.combo && n.rate, "first play: every best is new");
    n = Record(s, {10, 4, 199220, 250, 4, 1.00001, true, 280, 20, 31});
    Check(n.score && !n.combo && n.rate, "second play: better score and rate, worse combo");
    const ChartRecord& r = s.records[{10, 4}];
    Check(r.best_score == 199220 && r.best_combo == 300 && r.best_rank == 4 && r.best_rate == 10000 && r.plays == 2 &&
              r.clears == 2 && r.full_combos == 1,
          "record keeps the bests and counts");
    Check(s.perfects == 280 && s.greats == 20 && s.costume_plays[31] == 1 && s.costume_plays.size() == 1,
          "note totals and costume plays (none counted without a costume)");
    s.accolade = 2503, s.badge = 902001, s.name_plate = 130201;
    s.claimed = {1, 2000};
    s.owned[{2, 4}] = 35;
    s.owned[{7, 31}] = 1;
    s.wear[1] = 31;
    s.wear[11] = 105;
    s.unknown.push_back("medal 3 gold");  // a line from a newer version

    std::string error;
    Check(Write(path, s, &error), "write: " + error);
    Check(!std::filesystem::exists(dir / "save.txt.tmp"), "no temporary file left");
    SaveData back;
    Check(Load(path, back, &error), "load: " + error);
    Check(back.player == "Ayumu Uehara" && back.records.size() == 1 && back.records[{10, 4}].best_score == 199220 &&
              back.records[{10, 4}].full_combos == 1 && back.accolade == 2503 && back.badge == 902001 &&
              back.perfects == 280 && back.greats == 20 && back.costume_plays[31] == 1 &&
              back.name_plate == 130201 && back.claimed == std::set<int>{1, 2000} && back.owned.size() == 2 &&
              back.owned[{2, 4}] == 35 && back.owned[{7, 31}] == 1 &&
              back.wear.size() == 2 && back.wear[1] == 31 && back.wear[11] == 105 &&
              back.unknown.size() == 1 && back.unknown[0] == "medal 3 gold",
          "round trip keeps the name (with spaces), the record, the nameplate and unknown lines");

    { std::ofstream(path) << "not a save\n"; }
    SaveData bad;
    Check(!Load(path, bad, &error) && bad.records.empty(), "another file is refused, not read as a save");

    std::filesystem::remove_all(dir);
    std::printf("%s\n", failures ? "save_test FAILED" : "save_test ok");
    return failures ? 1 : 0;
}
