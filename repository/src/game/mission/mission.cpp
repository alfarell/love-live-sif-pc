#include "game/mission/mission.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"
#include "game/save/save.h"

namespace sif::mission {
namespace {

constexpr const char* kFolder = "common/common.pac/mission.pac/";

bool Table(const Vfs& vfs, const char* file, const char* symbol, std::vector<uint8_t>& bytes,
           std::span<const uint8_t>& data, std::string* error) {
    std::vector<blbn::Symbol> symbols;
    if (!vfs.Read(std::string(kFolder) + file, bytes, error) || !blbn::Parse(bytes, symbols, error)) return false;
    const blbn::Symbol* s = blbn::Find(symbols, symbol);
    if (!s) {
        if (error) *error = std::string(file) + ": no " + symbol;
        return false;
    }
    data = s->data;
    return true;
}

std::string Str(const std::vector<uint8_t>& f, uint64_t at) {
    if (at >= f.size()) return {};
    const char* p = reinterpret_cast<const char*>(&f[size_t(at)]);
    return std::string(p, strnlen(p, f.size() - size_t(at)));
}

}

bool Load(const Vfs& vfs, Missions& out, std::string* error) {
    out = {};
    static constexpr std::array<std::array<const char*, 2>, 4> kTables = {{
        {"mission_common.bin", "ll3::mission::s_mission_common_datas"},
        {"mission_music.bin", "ll3::mission::s_mission_music_datas"},
        {"mission_dress.bin", "ll3::mission::s_mission_dress_datas"},
        {"mission_recommended.bin", "ll3::mission::s_mission_recommended_datas"},
    }};
    std::vector<uint8_t> f;
    std::span<const uint8_t> d;
    for (const auto& t : kTables) {
        if (!Table(vfs, t[0], t[1], f, d, error)) return false;
        for (size_t o = 0; o + 28 <= d.size(); o += 28) {
            int32_t v[7];
            std::memcpy(v, &d[o], 28);
            out.rows.push_back({v[0], v[1], v[2], v[3], v[4], v[5], v[6]});
        }
    }
    // 9 u64 per kind: 4 titles, 4 descriptions (file offsets of UTF-8 strings), then a flag word (not decoded)
    if (!Table(vfs, "mission_condition.bin", "ll3::mission::s_mission_condition", f, d, error)) return false;
    for (size_t o = 0; o + 72 <= d.size(); o += 72) {
        uint64_t q[9];
        std::memcpy(q, &d[o], 72);
        Condition c;
        for (int l = 0; l < 4; ++l) c.title[size_t(l)] = Str(f, q[l]), c.text[size_t(l)] = Str(f, q[4 + l]);
        out.conditions.push_back(std::move(c));
    }
    return true;
}

bool LoadAwards(const Vfs& vfs, std::vector<Award>& accolades, std::vector<Award>& badges, std::string* error) {
    accolades.clear(), badges.clear();
    std::vector<uint8_t> f;
    std::vector<blbn::Symbol> symbols;
    const blbn::Symbol* s = nullptr;
    // +0 id, +4 u32 1, +8 plate (Hypothesis), +0x10 u64 name offset, +0x18 four u32 not decoded
    if (!vfs.Read("common/common.pac/honorific_title.pac/honorific_title_datas.bin", f, error) ||
        !blbn::Parse(f, symbols, error) || !(s = blbn::Find(symbols, "s_honorific_title_datas"))) {
        if (error && error->empty()) *error = "no s_honorific_title_datas";
        return false;
    }
    for (size_t o = 0; o + 40 <= s->data.size(); o += 40) {
        uint32_t v[4];
        uint64_t name = 0;
        std::memcpy(v, &s->data[o], 16);
        std::memcpy(&name, &s->data[o + 16], 8);
        accolades.push_back({int(v[0]), Str(f, name), int(v[2]), 0});
    }
    // +0 id, +4 category, +8 u64 name offset, +0x10 ARGB, +0x14 group mask (Hypothesis: 1 mu's, 2 Aqours, 4 Saint Snow)
    std::vector<uint8_t> g;
    if (!vfs.Read("common/common.pac/badge.pac/badge_datas.bin", g, error) || !blbn::Parse(g, symbols, error) ||
        !(s = blbn::Find(symbols, "s_badge_datas"))) {
        if (error && error->empty()) *error = "no s_badge_datas";
        return false;
    }
    for (size_t o = 0; o + 32 <= s->data.size(); o += 32) {
        uint32_t v[2], color = 0;
        uint64_t name = 0;
        std::memcpy(v, &s->data[o], 8);
        std::memcpy(&name, &s->data[o + 8], 8);
        std::memcpy(&color, &s->data[o + 16], 4);
        badges.push_back({int(v[0]), Str(g, name), int(v[1]), color});
    }
    return true;
}

const Award* FindAward(const std::vector<Award>& awards, int id) {
    for (const Award& a : awards) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

Progress ProgressOf(const Mission& m, const save::SaveData& save) {
    Progress p;
    p.need = std::max(1, m.p2);
    const bool per_song = m.Kind() >= 35 && m.Kind() <= 39;
    int plays = 0, best_score = 0, best_rank = -1, full_combos = 0;
    for (const auto& [chart, r] : save.records) {
        if (per_song && chart.first != m.p1) continue;
        plays += r.plays, full_combos += r.full_combos;
        best_score = std::max(best_score, r.best_score), best_rank = std::max(best_rank, r.best_rank);
    }
    switch (m.Kind()) {
        case 0:   // play Live Shows [p2] times
        case 35:  // play "[p1]" [p2] times
            p.have = plays;
            break;
        case 1:   // a FULL COMBO on any song
        case 38:  // a FULL COMBO on "[p1]"
            p.need = 1, p.have = full_combos > 0;
            break;
        case 2: p.need = 1, p.have = best_rank >= 4; break;  // rank S or better (0 D .. 4 S .. 6 SSS)
        case 3: p.need = 1, p.have = best_rank >= 6; break;  // rank SSS
        case 4:   // a score of [p2] on any song
        case 39:  // a score of [p2] on "[p1]"
            p.have = best_score;
            break;
        // shortcut: GREAT/PERFECT totals, costumes, items, viewing mode, medleys, cards, likes and member counters
        // are not in the save yet; add them with the screens that count them
        default: break;
    }
    return p;
}

std::string Fill(const std::string& pattern, const Mission& m, const std::string& p1_name) {
    std::string s = pattern;
    for (const auto& [key, value] : {std::pair<std::string, std::string>{"[p1]", p1_name}, {"[p2]", std::to_string(m.p2)}}) {
        for (size_t at; (at = s.find(key)) != std::string::npos;) s.replace(at, key.size(), value);
    }
    return s;
}

}
