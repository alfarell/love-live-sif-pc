#pragma once
// Photos and profile cards (M12, plan/202611161811-m12-profile-cards/): the card frames and the NPC preset cards of
// common/common.pac/card.pac, the photo slot rules and the offline support-card pick. See
// analysis/202611161811-profile-cards/ (P1: eboot, P2: shadPS4) and docs/formats/card.md.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sif {
class Vfs;
namespace save {
struct SaveData;
}

namespace profile {

constexpr int kFrameReward = 5;     // mission reward type 5 = a profile card frame (save.owned key {5, frame id})
constexpr int kPhotoSlots = 30;     // "You can save up to 30 photos"
constexpr int kPhotosPerLive = 5;   // "You can take up to 5 photos of a Live Show"
constexpr int kMaxFavourites = 25;  // Photo Gallery "Maximum Number: 25"

// s_profile_card_frame_datas: 160-byte records.
struct Frame {
    int id = 0;           // 1001-1022 the members' initial frames; others are birthday, group and event frames
    bool initial = false;  // +0x04: owned from the start
    int member = 0;
    std::array<std::string, 4> name;  // JP, EN, CN, TW ("Honoka Kosaka initial\nProfile Card Frame")
};

// ll3::s_preset_profilecard: 96-byte records, the offline "(NPC Profile Card)" support cards. The fields as the eboot
// copies them into a support player (FUN_001f1210).
struct Preset {
    int index = 0;
    int member_card = 0;              // 10031, 111051, 212001
    std::array<int, 3> skill{}, skill_level{};  // all empty in the presets
    int frame = 0, live = 0;          // +0x20, +0x24 (120, 1120, 1900)
    int accolade = 0, badge = 0, name_plate = 0;
    std::string photo;                // "preset_0000.jpg": the whole card, proca_preset/ in the dump
    std::array<std::string, 4> player;  // "Honoka's fan" in JP, EN, CN, TW
    int Member() const { return member_card / 10000; }
};

struct Data {
    std::vector<Frame> frames;    // 183
    std::vector<Preset> presets;  // 20
};

bool Load(const Vfs& vfs, Data& out, std::string* error = nullptr);
const Frame* FindFrame(const Data& data, int id);

// The group a member belongs to for the µ's/Aqours mode: 0 for µ's (1-9), 1 for Aqours and Saint Snow (eboot
// FUN_0001ead0: member > 9).
inline int Group(int member) { return member > 9 ? 1 : 0; }

// The frames a member's card can use: the member's initial frame and the frames of that member the player owns.
std::vector<const Frame*> OwnedFrames(const Data& data, const save::SaveData& save, int member);

// "Create a New Profile Card" opens once a photo is saved (P2: greyed with none, open with photos; not traced).
bool CanCreateCard(const save::SaveData& save);
// The slot a new photo goes to: the lowest free one, else the oldest photo that is not a favourite ("Older photos may
// have been overwritten"); -1 when all 30 are favourites (cannot happen with at most 25).
int NextPhotoSlot(const save::SaveData& save);
// The photo slots, newest first (the Photo Gallery order).
std::vector<int> PhotosNewestFirst(const save::SaveData& save);
// "photos/NN.png" relative to the data folder.
std::string PhotoFile(int slot);

// The eboot's xorshift128 for the NPC pick: one state per game run, from {0, 1, 1812433255, 1900727105} (0x5F55A0).
struct XorShift128 {
    uint32_t s[4] = {0, 1, 1812433255u, 1900727105u};
    uint32_t Next();
    int Below(int n) { return int(float(Next() >> 16) * (1.0f / 65536.0f) * float(n)); }  // 0..n-1, as the eboot
};

// The two "(NPC Profile Card)" supports of a live in `group` (eboot FUN_001d63d0): the presets of that group, two
// different ones drawn with `random`. Preset indexes; -1 when there are fewer candidates.
std::array<int, 2> PickSupport(const Data& data, int group, XorShift128& random);

}
}
