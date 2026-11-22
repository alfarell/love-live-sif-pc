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

namespace friendship {
struct Tables;
}
namespace card {
struct Data;
}
namespace costume {
struct Data;
}
namespace dlc {
struct Ownership;
}

namespace mission {

// One row (28 bytes, seven u32). kind = code & 0xFF indexes the conditions; code >> 8 is a flag not decoded
// (Hypothesis: 1 = counted, 2/3 = first of a series). p1 = song id (kinds 35-39), member (20-34) or costume (40-48).
struct Mission {
    int id = 0, code = 0, p1 = 0, p2 = 0;
    int reward_type = 0, reward_id = 0, reward_amount = 0;  // 2 item, 3 Wai-Wai! Booster (M14), 4 accolade (a few badges), 5 frame, 6 name plate, 7 costume; 1 skill card (M11 S1)
    int Kind() const { return code & 0xFF; }
    int Rule() const { return (code >> 8) & 0xFF; }  // row byte +5, the show rule (see Shown)
};

// One condition kind (s_mission_condition: 9 u64 per kind): title and description per language (0 Japanese,
// 1 English, 2 simplified, 3 traditional Chinese), with "[p1]" and "[p2]" for the row's parameters.
struct Condition {
    std::array<std::string, 4> title, text;
    // +0x41 / +0x42: what p1 / p2 are (Q1): 1 the count needed, 2 a group (0 µ's, 1 Aqours), 3 a member, 4 a song,
    // 5 a costume; 0 unused
    int p1_type = 0, p2_type = 0;
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

// Whether the Task List lists a row for the current member (eboot FUN_001c6bd0): each parameter by its type, a group
// = her mode, a member = her, a song of her mode (`song_mode`: the song's mode, -1 unknown), a costume of her group
// (by id range: µ's 0-99, Aqours 100-199, Saint Snow 200-; shortcut: the eboot reads the costume's group mask +0x54).
bool Fits(const Mission& m, const Condition& c, int member, int song_mode);
// The show rule (row byte +5, eboot FUN_001c6ea0): 0 shown, but once claimed hidden when the next row (id + 1) is its
// next step; 1 a series step, shown once the previous row (id - 1) is claimed, then as 0; 2 / 3 shown when the song /
// costume is owned (`owned`). So a series shows one step at a time.
bool Shown(const Mission& m, const Missions& all, const save::SaveData& save, bool owned);

// How far the save is: have / need; have = -1 when the port does not track the condition yet.
struct Progress {
    int have = -1, need = 1;
    bool Done() const { return have >= need; }
};
// The tables some kinds need (M15); a kind whose table is missing never completes.
struct Context {
    const friendship::Tables* levels = nullptr;  // 32 Friendship level
    const card::Data* cards = nullptr;           // 22, 23, 31 skill cards
    const costume::Data* costumes = nullptr;     // 7, 11, 28, 40 costumes and key items
    const dlc::Ownership* addons = nullptr;      // add-on costumes for 7, 40
    const Missions* missions = nullptr;          // 29: the accolades her member missions give
};
Progress ProgressOf(const Mission& m, const save::SaveData& save, const Context* context = nullptr);
// Claims a completed mission: adds its reward to save.owned[(type, id)] (the amount, at least 1) and marks it claimed.
// False, and nothing changes, if it is not complete or was claimed before.
// Reward type 3 is the Wai-Wai! Booster (eboot FUN_001c7100 case 3; its id 5003 is the booster picture reward_5003):
// it adds to save.boosters, capped at 9 (M14 F1).
bool Claim(const Mission& m, save::SaveData& save, const Context* context = nullptr);
// The ids the save owns of one reward type (4 accolades and badges, 6 name plates, 7 costumes ...), ascending.
std::vector<int> Owned(const save::SaveData& save, int reward_type);

// The row's title or description in `language`, with [p2] filled in and [p1] replaced by `p1_name` (a song title,
// member or costume name; the caller looks it up).
std::string Fill(const std::string& pattern, const Mission& m, const std::string& p1_name);

}
}
