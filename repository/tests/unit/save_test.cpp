// The port's own save (src/game/save/save.cpp): round trip, update rule, kept unknown lines, missing file.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "engine/platform/settings.h"
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
    s.plates[0] = {2503, 901001, 130201};
    s.plates[1] = {501001, 902001, 0};
    s.claimed = {1, 2000};
    s.owned[{2, 4}] = 35;
    s.owned[{7, 31}] = 1;
    s.wear[1] = 31;
    s.wear[11] = 105;
    s.special.insert(11);
    Check(Navigator(s, 0) == 1 && Navigator(s, 1) == 11 && Current(s) == 1, "a new game: Honoka and Chika");
    s.mode = 1;
    s.navigator[0] = 4;
    s.navigator[1] = 21;
    s.bonds[11] = {197, 1};
    s.boosters = 4, s.boosters_used = 1;
    s.login_date = 20261119, s.login_day = 3, s.logins = 2;
    s.birthdays[11] = {2026, 120, true};
    Add(s, GroupKey(1, "plays"), 3);
    Best(s, GroupKey(1, "best_rank"), 7);
    Best(s, GroupKey(1, "best_rank"), 4);  // a worse rank keeps the best
    CountCollected(s, 2, 55, 10);           // an Aqours key item
    CountCollected(s, 7, 47, 4);            // an Aqours thread
    CountCollected(s, 4, 5, 2);             // a common cloth
    Mark(s, "talk.11", 50);
    Mark(s, "talk.11", 50);
    s.unknown.push_back("medal 3 gold");  // a line from a newer version

    std::string error;
    Check(Write(path, s, &error), "write: " + error);
    Check(!std::filesystem::exists(dir / "save.txt.tmp"), "no temporary file left");
    SaveData back;
    Check(Load(path, back, &error), "load: " + error);
    Check(back.player == "Ayumu Uehara" && back.records.size() == 1 && back.records[{10, 4}].best_score == 199220 &&
              back.records[{10, 4}].full_combos == 1 && back.plates == s.plates &&
              back.perfects == 280 && back.greats == 20 && back.costume_plays[31] == 1 &&
              back.claimed == std::set<int>{1, 2000} && back.owned.size() == 2 &&
              back.owned[{2, 4}] == 35 && back.owned[{7, 31}] == 1 &&
              back.wear.size() == 2 && back.wear[1] == 31 && back.wear[11] == 105 &&
              back.special == std::set<int>{11} && back.mode == 1 && Current(back) == 21 && Navigator(back, 0) == 4 &&
              back.bonds[11].points == 197 && back.bonds[11].lives == 1 && back.boosters == 4 && back.boosters_used == 1 &&
              back.login_date == 20261119 && back.login_day == 3 && back.logins == 2 && back.birthdays[11].year == 2026 &&
              back.birthdays[11].points == 120 && back.birthdays[11].received && Count(back, "g1.plays") == 3 &&
              Count(back, "g1.best_rank") == 7 && Count(back, "collected.key") == 10 && Marked(back, "keys", 55) &&
              Count(back, "collected.thread") == 4 && Count(back, "collected.cloth") == 2 && SetSize(back, "talk.11") == 1 &&
              back.unknown.size() == 1 && back.unknown[0] == "medal 3 gold",
          "round trip keeps the name (with spaces), the record, the nameplate and unknown lines");

    // M16: your own Set Lists and the medley counters
    SaveData m;
    MedleyList& mine = m.medleys[{1, 3}];
    mine.name = "My  Aqours set";
    mine.stage = 112;
    mine.entries[0] = {1120, 105, 0};
    mine.entries[2] = {1910, 200, 1};  // a gap stays a gap
    m.medleys[{0, 0}].entries[0] = {10, 3, 0};  // no name, no stage
    CountMedley(m, 1, 245, {{1120, 105, 0}, {1460, 217, 0}});
    CountMedley(m, 1, 30, {{1120, 105, 0}});
    Check(Write(path, m, &error), "write medleys: " + error);
    SaveData mb;
    Check(Load(path, mb, &error) && mb.medleys.size() == 2 && mb.medleys[{1, 3}] == mine &&
              mb.medleys[{0, 0}].name.empty() && mb.medleys[{0, 0}].stage == 0 && mb.medleys[{0, 0}].entries[0].song == 10 &&
              Count(mb, "g1.medley_seconds") == 275 && Count(mb, "g0.medley_seconds") == 0 &&
              mb.sets["medley_songs"] == std::set<int>{1120, 1460} && mb.sets["medley_costumes"] == std::set<int>{105, 117} &&
              mb.unknown.empty(),
          "medley lists round trip (name with spaces, a gap, the stage) and the medley counters (217 as 117)");
    { std::ofstream(path) << "sif-port save 1\nmedley_name 0 2 Later\nmedley 0 2 5 10 3 0\nmedley 0 12 1\n"; }
    SaveData bad_medley;
    Check(Load(path, bad_medley) && bad_medley.medleys.size() == 1 && bad_medley.medleys[{0, 2}].name == "Later" &&
              bad_medley.medleys[{0, 2}].entries[0].song == 0 && bad_medley.unknown.size() == 2,
          "a short or out-of-range medley line is kept as unknown, not read");

    { std::ofstream(path) << "sif-port save 1\nwear 1 31\n"; }  // a save before M13: no colour, no mode
    SaveData old;
    Check(Load(path, old) && old.wear[1] == 31 && old.special.empty() && old.mode == 0 && Current(old) == 1,
          "an older save reads with the defaults");

    { std::ofstream(path) << "not a save\n"; }
    SaveData bad;
    Check(!Load(path, bad, &error) && bad.records.empty(), "another file is refused, not read as a save");

    // settings.ini: the Rhythm Icon Speed level (F3); a pre-F3 approach_ms becomes the nearest level of 1700 .. 800 ms
    const std::filesystem::path ini = dir / "settings.ini";
    for (const auto& [text, level] : {std::pair<const char*, int>{"[play]\napproach_ms = 1300\n", 2},
                                      {"[play]\napproach_ms = 900\n", 4}, {"[play]\napproach_ms = 3000\n", 0},
                                      {"[play]\nnote_speed = 3\napproach_ms = 800\n", 3}, {"[play]\nnote_speed = 9\n", 4}}) {
        { std::ofstream(ini) << text; }
        sif::AppSettings settings;
        Check(sif::LoadAppSettings(ini, settings) && settings.play.note_speed == level,
              std::string("settings note_speed from: ") + text);
    }
    sif::AppSettings kept;
    kept.play.note_speed = 1;
    sif::AppSettings back_settings;
    Check(sif::SaveAppSettings(ini, kept) && sif::LoadAppSettings(ini, back_settings) && back_settings.play.note_speed == 1,
          "settings note_speed round trip");

    std::filesystem::remove_all(dir);
    std::printf("%s\n", failures ? "save_test FAILED" : "save_test ok");
    return failures ? 1 : 0;
}
