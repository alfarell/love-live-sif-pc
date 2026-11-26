#include "game/talk/talk.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "engine/fs/pac.h"
#include "engine/fs/vfs.h"

namespace sif::talk {
namespace {

std::string Folder(int member) {
    char s[48];
    std::snprintf(s, sizeof(s), "talk/talk.pac/c%02d.pac", member);
    return s;
}

}

std::vector<int> TalkIds(const Vfs& vfs, int member) {
    std::vector<pac::ArcEntry> entries;
    std::vector<int> ids;
    if (!vfs.List(Folder(member), entries)) return ids;
    for (const pac::ArcEntry& e : entries) {
        int id = 0;
        if (std::sscanf(e.name.c_str(), "talk_%d", &id) == 1) ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

namespace {

// Talk and voice timelines are BTL files whose events are (u16 tag, u16 payload size, u32 0, payload). The tracks and
// key times around them are not decoded (shortcut: scanning for the events is enough for the talks and mouth keys).
uint32_t U32(const std::vector<uint8_t>& f, size_t o) {
    uint32_t v = 0;
    std::memcpy(&v, &f[o], 4);
    return v;
}

// The first payload u32 of the first event `tag` with payload size `size`; -1 if none.
int64_t FindEvent(const std::vector<uint8_t>& f, uint16_t tag, uint16_t size) {
    for (size_t o = 0; o + 8 + size <= f.size(); o += 4) {
        if (U32(f, o) == (uint32_t(size) << 16 | tag) && U32(f, o + 4) == 0) return U32(f, o + 8);
    }
    return -1;
}

bool ReadTalk(const Vfs& vfs, int member, int id, std::vector<uint8_t>& f) {
    char name[32];
    std::snprintf(name, sizeof(name), "/talk_%04d.btlb", id);
    return vfs.Read(Folder(member) + name, f) && f.size() >= 4 && std::memcmp(f.data(), "BTL", 3) == 0;
}

}

int64_t VoiceBase(int member) {
    if (member >= 21) return 50000 + 1000 * (member - 21);
    if (member >= 11) return 40000 + 1000 * (member - 11);
    return 10000 + 1000 * (member - 1);
}

int64_t VoiceId(const Vfs& vfs, int member, int id) {
    std::vector<uint8_t> f;
    if (!ReadTalk(vfs, member, id, f)) return -1;
    const int64_t value = FindEvent(f, 0x0028, 4);  // the voice (1780 of the 1840 talks; 1744 name a cue)
    return value < 0 ? -1 : VoiceBase(member) + value;
}

int Motion(const Vfs& vfs, int member, int id) {
    std::vector<uint8_t> f;
    if (!ReadTalk(vfs, member, id, f)) return -1;
    return int(FindEvent(f, 0x0514, 8));  // payload (motion, 0); M7 took this for the voice
}

std::vector<MouthKey> MouthKeys(const Vfs& vfs, int member, int64_t voice_id) {
    std::vector<MouthKey> keys;
    char folder[48], prefix[16], suffix[16];
    std::snprintf(folder, sizeof(folder), "chr/c%02d/c%02d.pac/timeline.pac", member, member);
    std::snprintf(prefix, sizeof(prefix), "voice_%02d_", member);
    std::snprintf(suffix, sizeof(suffix), "_%05lld.btlb", static_cast<long long>(voice_id));
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List(folder, entries)) return keys;
    std::vector<uint8_t> f;
    for (const pac::ArcEntry& e : entries) {
        if (e.name.rfind(prefix, 0) == 0 && e.name.ends_with(suffix) && vfs.Read(std::string(folder) + "/" + e.name, f)) break;
    }
    // each key: u32 frame, u32 1, then tag 0x0b (shape, blend, f32 weight) or tag 0x0c (0, blend: close)
    for (size_t o = 8; o + 20 <= f.size(); o += 4) {
        if (U32(f, o - 4) != 1 || U32(f, o + 4) != 0) continue;
        MouthKey k;
        k.frame = int(U32(f, o - 8));
        if (U32(f, o) == (0x0cu << 16 | 0x0b)) {
            k.shape = int(U32(f, o + 8)), k.blend = int(U32(f, o + 12));
            std::memcpy(&k.weight, &f[o + 16], 4);
        } else if (U32(f, o) == (0x08u << 16 | 0x0c) && U32(f, o + 8) == 0) {
            k.blend = int(U32(f, o + 12));
        } else {
            continue;
        }
        if (k.shape < 0 || k.shape > 7) continue;
        keys.push_back(k);
    }
    std::stable_sort(keys.begin(), keys.end(), [](const MouthKey& a, const MouthKey& b) { return a.frame < b.frame; });
    return keys;
}

std::array<float, 8> MouthAt(const std::vector<MouthKey>& keys, float frames) {
    std::array<float, 8> w{};
    for (size_t i = 0; i < keys.size() && keys[i].frame <= frames; ++i) {
        const float until = i + 1 < keys.size() ? std::min(frames, float(keys[i + 1].frame)) : frames;
        const float r = keys[i].blend > 0 ? std::clamp((until - keys[i].frame) / float(keys[i].blend), 0.0f, 1.0f) : 1.0f;
        for (size_t s = 1; s < w.size(); ++s) w[s] = w[s] * (1 - r) + (int(s) == keys[i].shape ? keys[i].weight * r : 0.0f);
    }
    return w;
}

std::vector<Rule> LoadRules(const Vfs& vfs) {
    std::vector<uint8_t> f;
    std::vector<Rule> rules;
    if (!vfs.Read("talk/talk.pac/talk_data.bin", f) || f.size() < 0x40 || std::memcmp(f.data(), "BLBN", 4) != 0) return rules;
    // shortcut: rows from 0x40 until the padding (the last id repeated); the BLBN chunk header's count is not read
    for (size_t o = 0x40; o + 16 <= f.size(); o += 16) {
        uint32_t v[4];
        std::memcpy(v, &f[o], 16);
        if (!rules.empty() && int(v[0]) == rules.back().id) break;
        rules.push_back({int(v[0]), int(v[1]), v[2], int(v[3] >> 16)});
    }
    return rules;
}

int GreetingFor(const std::vector<Rule>& rules, int hour) {
    for (const Rule& r : rules) {
        if (r.type != 1) continue;
        const int start = int(r.param & 0xFFFF), end = int(r.param >> 16);
        if (start <= end ? (hour >= start && hour < end) : (hour >= start || hour < end)) return r.id;
    }
    return -1;
}

}
