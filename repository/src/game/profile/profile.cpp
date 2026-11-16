#include "game/profile/profile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <span>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"
#include "game/save/save.h"

namespace sif::profile {
namespace {

template <class T>
T At(std::span<const uint8_t> d, size_t off) {
    T v{};
    if (off + sizeof(T) <= d.size()) std::memcpy(&v, d.data() + off, sizeof(T));
    return v;
}

std::string Str(const std::vector<uint8_t>& f, uint64_t at) {
    if (at == 0 || at >= f.size()) return {};
    const char* p = reinterpret_cast<const char*>(&f[size_t(at)]);
    return std::string(p, strnlen(p, f.size() - size_t(at)));
}

bool Table(const Vfs& vfs, const char* path, const char* symbol, std::vector<uint8_t>& file,
           std::span<const uint8_t>& data, std::string* error) {
    std::vector<blbn::Symbol> symbols;
    const blbn::Symbol* s = nullptr;
    if (!vfs.Read(path, file, error) || !blbn::Parse(file, symbols, error) || !(s = blbn::Find(symbols, symbol))) {
        if (error && error->empty()) *error = std::string(path) + ": no " + symbol;
        return false;
    }
    data = s->data;
    return true;
}

}

bool Load(const Vfs& vfs, Data& out, std::string* error) {
    out = {};
    std::vector<uint8_t> f;
    std::span<const uint8_t> d;
    if (!Table(vfs, "common/common.pac/card.pac/profile_card_datas.bin", "s_profile_card_frame_datas", f, d, error)) {
        return false;
    }
    for (size_t o = 0; o + 160 <= d.size(); o += 160) {
        Frame fr;
        fr.id = At<int32_t>(d, o), fr.initial = At<int32_t>(d, o + 4) != 0, fr.member = At<int32_t>(d, o + 8);
        for (int l = 0; l < 4; ++l) fr.name[size_t(l)] = Str(f, At<uint64_t>(d, o + 0x10 + 8 * size_t(l)));
        out.frames.push_back(std::move(fr));
    }
    if (!Table(vfs, "common/common.pac/card.pac/preset_profilecard.bin", "ll3::s_preset_profilecard", f, d, error)) {
        return false;
    }
    for (size_t o = 0; o + 96 <= d.size(); o += 96) {
        Preset p;
        p.index = At<int32_t>(d, o), p.member_card = At<int32_t>(d, o + 4);
        for (size_t i = 0; i < 3; ++i) {
            p.skill[i] = At<int32_t>(d, o + 8 + 4 * i), p.skill_level[i] = At<int32_t>(d, o + 0x14 + 4 * i);
        }
        p.frame = At<int32_t>(d, o + 0x20), p.live = At<int32_t>(d, o + 0x24);
        p.accolade = At<int32_t>(d, o + 0x2C), p.badge = At<int32_t>(d, o + 0x30), p.name_plate = At<int32_t>(d, o + 0x34);
        p.photo = Str(f, At<uint64_t>(d, o + 0x38));
        for (int l = 0; l < 4; ++l) p.player[size_t(l)] = Str(f, At<uint64_t>(d, o + 0x40 + 8 * size_t(l)));
        out.presets.push_back(std::move(p));
    }
    if (out.frames.empty() || out.presets.empty()) {
        if (error) *error = "profile card tables are empty";
        return false;
    }
    return true;
}

const Frame* FindFrame(const Data& data, int id) {
    for (const Frame& f : data.frames) {
        if (f.id == id) return &f;
    }
    return nullptr;
}

std::vector<const Frame*> OwnedFrames(const Data& data, const save::SaveData& save, int member) {
    std::vector<const Frame*> out;
    for (const Frame& f : data.frames) {
        if (f.member != member) continue;
        const auto it = save.owned.find({kFrameReward, f.id});
        if (f.initial || (it != save.owned.end() && it->second > 0)) out.push_back(&f);
    }
    return out;
}

bool CanCreateCard(const save::SaveData& save) { return !save.photos.empty(); }

int NextPhotoSlot(const save::SaveData& save) {
    for (int slot = 0; slot < kPhotoSlots; ++slot) {
        if (!save.photos.contains(slot)) return slot;
    }
    int oldest = -1;
    for (const auto& [slot, p] : save.photos) {
        if (!p.favourite && (oldest < 0 || p.time < save.photos.at(oldest).time)) oldest = slot;
    }
    return oldest;
}

std::vector<int> PhotosNewestFirst(const save::SaveData& save) {
    std::vector<int> slots;
    for (const auto& [slot, p] : save.photos) slots.push_back(slot);
    // the same second (five photos in one live): the higher slot was written later
    std::stable_sort(slots.begin(), slots.end(), [&](int a, int b) {
        const long long ta = save.photos.at(a).time, tb = save.photos.at(b).time;
        return ta != tb ? ta > tb : a > b;
    });
    return slots;
}

std::string PhotoFile(int slot) {
    char name[32];
    std::snprintf(name, sizeof name, "photos/%02d.png", slot);
    return name;
}

uint32_t XorShift128::Next() {
    uint32_t t = s[0] ^ (s[0] << 11);
    s[0] = s[1], s[1] = s[2], s[2] = s[3];
    s[3] = s[3] ^ (s[3] >> 19) ^ t ^ (t >> 8);
    return s[3];
}

std::array<int, 2> PickSupport(const Data& data, int group, XorShift128& random) {
    std::vector<int> candidates;
    for (const Preset& p : data.presets) {
        if (Group(p.Member()) == group) candidates.push_back(p.index);
    }
    std::array<int, 2> picks{-1, -1};
    if (candidates.size() < 2) return picks;  // the eboot picks none then
    for (int& pick : picks) {
        const int i = random.Below(int(candidates.size()));
        pick = candidates[size_t(i)];
        candidates.erase(candidates.begin() + i);
    }
    return picks;
}

}
