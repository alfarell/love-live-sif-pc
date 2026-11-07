#pragma once
// Missions (common/common.pac/mission.pac): the mission rows of the four BLBN tables, the 49 condition kinds with their
// titles and descriptions, and progress worked out from the port's save. See docs/formats/mission.md.

#include <array>
#include <string>
#include <vector>

namespace sif {

class Vfs;
namespace save {
struct SaveData;
}

namespace mission {

// One row (28 bytes, seven u32). kind = code & 0xFF indexes the conditions; code >> 8 is a flag not decoded
// (Hypothesis: 1 = counted, 2/3 = first of a series). p1 = song id (kinds 35-39), member (20-34) or costume (40-48).
struct Mission {
    int id = 0, code = 0, p1 = 0, p2 = 0;
    int reward_type = 0, reward_id = 0, reward_amount = 0;  // Hypothesis: 2 item, 3 booster, 4 accolade, 6 card
    int Kind() const { return code & 0xFF; }
};

// One condition kind (s_mission_condition: 9 u64 per kind): title and description per language (0 Japanese,
// 1 English, 2 simplified, 3 traditional Chinese), with "[p1]" and "[p2]" for the row's parameters.
struct Condition {
    std::array<std::string, 4> title, text;
};

struct Missions {
    std::vector<Mission> rows;  // common (0-199), music (2000-3624), dress (4000-4768), recommended (300-1763)
    std::vector<Condition> conditions;  // 49
};
bool Load(const Vfs& vfs, Missions& out, std::string* error = nullptr);

// Awards a mission can give. Names are Japanese only in the dump (one text symbol per id, no language packages).
struct Award {
    int id = 0;
    std::string name;
    int group = 0;        // accolades: +0x08 (Hypothesis: plate id); badges: +0x04 category (id / 1000)
    unsigned color = 0;   // badges: ARGB; accolades: 0
};
// common/common.pac/honorific_title.pac s_honorific_title_datas (40-byte rows, 1665) and badge.pac s_badge_datas
// (32-byte rows, 229). Reward type 4 names an accolade id.
bool LoadAwards(const Vfs& vfs, std::vector<Award>& accolades, std::vector<Award>& badges, std::string* error = nullptr);
const Award* FindAward(const std::vector<Award>& awards, int id);

// How far the save is: have / need; have = -1 when the port does not track the condition yet.
struct Progress {
    int have = -1, need = 1;
    bool Done() const { return have >= need; }
};
Progress ProgressOf(const Mission& m, const save::SaveData& save);

// The row's title or description in `language`, with [p2] filled in and [p1] replaced by `p1_name` (a song title,
// member or costume name; the caller looks it up).
std::string Fill(const std::string& pattern, const Mission& m, const std::string& p1_name);

}
}
