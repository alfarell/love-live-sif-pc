#pragma once
// The port's own save (save.txt in the data folder; plan/202611021940-m7-systems-breadth/): the player's name and a
// record per chart. Plain text, one line per item; lines this version does not know are kept as they are.

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace sif::save {

struct ChartRecord {
    int best_score = 0, best_combo = 0;
    int best_rank = -1;      // 0 D .. 6 SSS; -1 = never played to the end
    int best_rate = 0;       // clear rate x 10000
    int plays = 0, clears = 0, full_combos = 0;
};

struct SaveData {
    std::string player = "Player";
    int accolade = 0, badge = 0, name_plate = 0;  // the nameplate's choices (0 = not chosen)
    std::set<int> claimed;                        // missions whose reward was taken
    // rewards owned: (mission reward type, id) -> count. Types (M7 plan 202611071847): 2 item, 3 booster, 4 accolade
    // or badge, 5 profile-card frame, 6 name plate, 7 costume; 1 card (Hypothesis)
    std::map<std::pair<int, int>, int> owned;
    std::map<std::pair<int, int>, ChartRecord> records;  // (song id, chart variant)
    long long perfects = 0, greats = 0;                  // judged notes over every finished live
    std::map<int, int> costume_plays;                    // costume id (dNNN) -> finished lives wearing it
    std::map<int, int> wear;                             // member (1-9, 11-19, 21-22) -> the costume id they wear
    std::vector<std::string> unknown;                    // lines of a newer version, written back unchanged
};

// One finished live (played to its end; a quit live is not recorded).
struct Play {
    int song = 0, variant = 0;
    int score = 0, combo = 0, rank = 0;
    double rate = 0;   // clear rate (1.0 = all Perfect)
    bool full_combo = false;
    int perfects = 0, greats = 0;
    int costume = -1;  // dNNN worn; -1 = none (the base model)
};
struct NewBests {
    bool score = false, combo = false, rate = false;
};

// A missing file is an empty save (true); a file that is not a port save is an error (false, `out` empty).
bool Load(const std::filesystem::path& path, SaveData& out, std::string* error = nullptr);
// Writes path + ".tmp" and then replaces `path`, so a failed write leaves the old save intact.
bool Write(const std::filesystem::path& path, const SaveData& data, std::string* error = nullptr);
// Counts the play and keeps each best that improved; the result says which improved (for the "NEW" marks).
NewBests Record(SaveData& data, const Play& play);

}
