#pragma once
// The port's own save (save.txt in the data folder; plan/202611021940-m7-systems-breadth/): the player's name and a
// record per chart. Plain text, one line per item; lines this version does not know are kept as they are.

#include <algorithm>
#include <array>
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

// A photo taken in View Live Show (M12). The picture is photos/NN.png in the data folder (NN = slot); the fields
// follow the game's own "LiveViewSS" record (analysis/202611161811-profile-cards/202611162102-analysis.md).
struct Photo {
    long long time = 0;  // when it was taken (seconds since 1970), newest first in the gallery
    int stage = 0;
    int member_card = 0;  // the played member's card (10030 = Honoka, School Unity)
    int live = 0;         // the game's live id (340011 = song 0340, live 011)
    bool favourite = false;
};

// The player's one profile card (M12; the game's "MyProca" record). Its photo is copied to profile_card.png when the
// card is saved, so later photos do not change it.
struct ProfileCard {
    int frame = 0;
    bool landscape = false;
    int crop = 0;         // the photo's left edge in the photo position adjustment, in photo pixels
    int live = 0, member_card = 0;  // from the photo
    int accolade = 0, badge = 0, name_plate = 0;
    std::array<int, 3> skill{}, skill_level{};  // a snapshot when saved, as the game keeps it
};

// M14: a member's Friendship (the eboot's member record +0x1C points, +0x10 lives played with her)
struct Bond {
    int points = 0, lives = 0;
};
// M14: a member's birthday event of `year` (record +0xBC year, +0xB8 points, +0xBA flags)
struct BirthdayState {
    int year = 0, points = 0;
    bool received = false;
    bool operator==(const BirthdayState&) const = default;
};

struct SaveData {
    std::string player = "Player";
    int accolade = 0, badge = 0, name_plate = 0;  // the nameplate's choices (0 = not chosen)
    std::set<int> claimed;                        // missions whose reward was taken
    // rewards owned: (mission reward type, id) -> count. Types (M7 plan 202611071847): 1 skill card (count = copies;
    // the level is min(copies, max), M11 S1), 2 item, 3 badge, 4 accolade or badge, 5 profile-card frame, 6 name
    // plate, 7 costume
    std::map<std::pair<int, int>, int> owned;
    std::map<int, std::array<int, 3>> skill_slots;  // member -> the three set skill cards (0 = empty), M11
    std::map<std::pair<int, int>, ChartRecord> records;  // (song id, chart variant)
    long long perfects = 0, greats = 0;                  // judged notes over every finished live
    std::map<int, int> costume_plays;                    // costume id (dNNN) -> finished lives wearing it
    std::map<int, int> wear;                             // member (1-9, 11-19, 21-22) -> the costume id they wear
    std::set<int> special;                               // members whose worn costume is in its special colour (M13)
    int mode = 0;                                        // the L2 mode: 0 µ's, 1 Aqours (M13; the game's save +0x48)
    std::map<int, int> navigator;                        // mode -> the member chosen with R2 (M13); see Navigator()
    std::map<int, Bond> bonds;                           // member -> Friendship (M14)
    int boosters = 0, boosters_used = 0;                 // Wai-Wai! Boosters held (0-9) and used in all (M14)
    int login_date = 0, login_day = 0, logins = 0;       // last login bonus (YYYYMMDD), next day 0-11, bonuses in all
    std::map<int, BirthdayState> birthdays;              // member -> her latest birthday event (M14)
    // M15: the mission counters ("counter KEY N") and id sets ("set KEY id id ..."); see Count/Mark below for the keys
    std::map<std::string, int> counters;
    std::map<std::string, std::set<int>> sets;
    std::map<int, Photo> photos;                         // slot (0-29) -> photo, M12
    bool has_card = false;
    ProfileCard card;                                    // valid when has_card
    std::vector<std::string> unknown;                    // lines of a newer version, written back unchanged
};

// The member chosen for `mode` (0 µ's, 1 Aqours): the save's choice, else Honoka / Chika as a new game.
int Navigator(const SaveData& data, int mode);
// The current member: the chosen member of the current mode.
inline int Current(const SaveData& data) { return Navigator(data, data.mode); }

// M15 mission counters and sets. Keys (G = the group mode 0 µ's / 1 Aqours of the played member, M a member, D a
// costume, S a song):
//   counters: gG.plays gG.greats gG.perfects gG.full_combos gG.best_score gG.best_rank (rank + 1; 0 = none)
//             collected.key collected.thread collected.cloth (materials gained in all, kinds 8-10)
//             views (View Live Show plays) view.S, photos (taken), photo.D (photos of members in D), cards.M (profile
//             cards made of M), camera.M (seconds in the Member camera on M)
//   sets:     keys (key items ever held, kind 11), talk.M (phrases of M heard), menu.M (costumes M wore on the top
//             menu), view_costumes, card_costumes, special (costumes played in a special colour), camera_photo
//             (members photographed in the Member camera)
inline int Count(const SaveData& s, const std::string& key) {
    const auto it = s.counters.find(key);
    return it == s.counters.end() ? 0 : it->second;
}
inline void Add(SaveData& s, const std::string& key, int n = 1) { s.counters[key] += n; }
inline void Best(SaveData& s, const std::string& key, int value) { s.counters[key] = std::max(Count(s, key), value); }
inline void Mark(SaveData& s, const std::string& key, int id) { s.sets[key].insert(id); }
inline bool Marked(const SaveData& s, const std::string& key, int id) {
    const auto it = s.sets.find(key);
    return it != s.sets.end() && it->second.contains(id);
}
inline int SetSize(const SaveData& s, const std::string& key) {
    const auto it = s.sets.find(key);
    return it == s.sets.end() ? 0 : int(it->second.size());
}
// Materials gained (mission kinds 8-11): by item type, 1/2 key items, 3/5/7 threads, 4/6/8 cloths (costume::Item)
inline void CountCollected(SaveData& s, int type, int item, int amount) {
    if (type == 1 || type == 2) Add(s, "collected.key", amount), Mark(s, "keys", item);
    else if (type >= 3 && type % 2) Add(s, "collected.thread", amount);
    else if (type >= 4) Add(s, "collected.cloth", amount);
}
// "gG." for the group mode `mode` (0 µ's, 1 Aqours and Saint Snow)
inline std::string GroupKey(int mode, const char* name) { return std::string(mode == 1 ? "g1." : "g0.") + name; }

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
