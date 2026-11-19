#include "game/friendship/friendship.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"

namespace sif::friendship {
namespace {

int32_t I32(std::span<const uint8_t> d, size_t off) {
    int32_t v = 0;
    if (off + 4 <= d.size()) std::memcpy(&v, d.data() + off, 4);
    return v;
}

}

bool Load(const Vfs& vfs, Tables& out, std::string* error) {
    out = {};
    constexpr const char* kPath = "common/common.pac/game.pac/level_datas.bin";
    std::vector<uint8_t> file;
    std::vector<blbn::Symbol> symbols;
    if (!vfs.Read(kPath, file, error) || !blbn::Parse(file, symbols, error)) return false;
    const blbn::Symbol* levels = blbn::Find(symbols, "s_level_datas");
    const blbn::Symbol* gain = blbn::Find(symbols, "s_experience_datas");
    const blbn::Symbol* emblems = blbn::Find(symbols, "s_emblemrank_datas");
    if (!levels || !gain || !emblems || gain->data.size() < 9 * 28) {
        if (error) *error = std::string(kPath) + ": tables missing";
        return false;
    }
    for (size_t o = 0; o + 8 <= levels->data.size(); o += 8) out.levels.push_back({I32(levels->data, o), I32(levels->data, o + 4)});
    for (size_t d = 0; d < 9; ++d) {
        for (size_t r = 0; r < 7; ++r) out.gain[d][r] = I32(gain->data, d * 28 + r * 4);
    }
    for (size_t o = 0; o + 8 <= emblems->data.size(); o += 8) out.emblems.push_back({I32(emblems->data, o), I32(emblems->data, o + 4)});
    return !out.levels.empty();
}

int LevelOf(const Tables& t, int points) {
    for (auto it = t.levels.rbegin(); it != t.levels.rend(); ++it) {
        if (it->second <= points) return it->first;
    }
    return 1;
}

int PointsFor(const Tables& t, int level) {
    if (t.levels.empty()) return 0;
    const int i = std::clamp(level, 1, int(t.levels.size())) - 1;
    return t.levels[size_t(i)].second;
}

int EmblemRank(const Tables& t, int level) {
    for (auto it = t.emblems.rbegin(); it != t.emblems.rend(); ++it) {
        if (it->second <= level) return it->first;
    }
    return 0;
}

std::string EmblemPath(int member, int rank) {
    char s[64];
    std::snprintf(s, sizeof(s), "ui/ui_yellrank.pac/texture.pac/emblem_%d_%d", member, rank);
    return s;
}

int Gain(const Tables& t, int difficulty, int rank, const GainSkills& skills, int perfects) {
    if (difficulty < 0 || difficulty > 8 || rank < 0 || rank > 6) return 0;
    const int base = t.gain[size_t(difficulty)][size_t(rank)];
    int total = base;
    for (const int sum : {skills.more, skills.more_plus, skills.expanded}) {
        if (sum > 0 && base * sum >= 100) total += base * sum / 100;  // 0xA6EDC-0xA6F73
    }
    if (skills.perfect > 0 && perfects > 0) {  // 0xA6F8D: rounded up (vroundss mode 2)
        total += int(std::ceil(float(skills.perfect) / 100.0f * float(perfects)));
    }
    return total;
}

}
