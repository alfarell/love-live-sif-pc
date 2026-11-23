#pragma once
// Live Show Medley (M16): the recommended Set Lists (common/common.pac/medley.pac/medley_data.bin,
// ll3::medley::s_medley_datas) and the rules shared by the Set List screen, the medley live and the save.
// analysis/202611231413-live-show-medley/202611231426-analysis.md (R1) and 202611231507-analysis.md (R2).

#include <array>
#include <string>
#include <vector>

namespace sif {

class Vfs;

namespace medley {

constexpr int kEntries = 20;  // songs per Set List
constexpr int kOwnLists = 10;  // your own Set Lists per group (eboot: save +0x531d8, 0xf7c per group)

// One song of a Set List. song 0 = an empty slot ("(Not Set)").
struct Entry {
    int song = 0;
    int costume = -1;  // dNNN; -1 = none chosen
    int colour = 0;    // the special colour (0 = the normal one)
    bool operator==(const Entry&) const = default;
};

// A recommended Set List: one 288-byte row.
struct SetList {
    int id = 0;
    std::array<std::string, 4> name;  // 0 Japanese, 1 English, 2 Chinese (simplified), 3 Chinese (traditional)
    int group = 0;                    // +0x28: 0 µ's, 1 Aqours
    int stage = 0;                    // +0x2c: the stage of the whole list (st001-st019, st101-st112)
    std::vector<Entry> entries;       // +0x30 songs, +0x80 costumes, +0xd0 colours; up to the first empty song
};

struct Data {
    std::vector<SetList> lists;  // in id order
};
bool Load(const Vfs& vfs, Data& out, std::string* error = nullptr);

// The recommended lists of a group (0 µ's, 1 Aqours and Saint Snow), in id order.
std::vector<const SetList*> OfGroup(const Data& data, int group);

// The stage of a list that has none: 1 (µ's) or 102 (Aqours), eboot FUN_00185ee0.
inline int DefaultStage(int group) { return group == 1 ? 102 : 1; }
// "stNNN" for a stage number
std::string StageFolder(int stage);
// The costume a medley mission (kind 47) counts: costume % 1000, with 217 counted as 117 (eboot FUN_001c6780).
inline int MissionCostume(int costume) {
    const int c = costume % 1000;
    return c == 217 ? 117 : c;
}

}
}
