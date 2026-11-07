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

std::string VoiceCue(const Vfs& vfs, int member, int id) {
    char name[32];
    std::snprintf(name, sizeof(name), "/talk_%04d.btlb", id);
    std::vector<uint8_t> f;
    if (!vfs.Read(Folder(member) + name, f) || f.size() < 4 || std::memcmp(f.data(), "BTL", 3) != 0) return {};
    // shortcut: the BTL timeline is not decoded; its events are 16-byte records (u16 tag, u16 kind, u32 0, u32 value,
    // u32 0), and the voice event is tag 0x0514 kind 8 with the cue number as value (1380 of the 1840 talks have one,
    // 1374 of them name an existing cue). Scanning for it is enough until the timeline's tracks and times are decoded.
    for (size_t o = 0; o + 16 <= f.size(); o += 4) {
        uint16_t tag = 0, kind = 0;
        uint32_t zero = 0, value = 0;
        std::memcpy(&tag, &f[o], 2);
        std::memcpy(&kind, &f[o + 2], 2);
        std::memcpy(&zero, &f[o + 4], 4);
        std::memcpy(&value, &f[o + 8], 4);
        if (tag == 0x0514 && kind == 8 && zero == 0) {
            char cue[32];
            // the arcade voice set; Saint Snow's members are numbered with two digits ("vo_ac_21_001")
            std::snprintf(cue, sizeof(cue), member >= 21 ? "vo_ac_%02d_%03u" : "vo_ac_%03d_%03u", member, value);
            return cue;
        }
    }
    return {};
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
