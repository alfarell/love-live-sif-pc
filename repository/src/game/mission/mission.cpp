#include "game/mission/mission.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <span>
#include <string>
#include <utility>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"
#include "game/card/card.h"
#include "game/costume/costume.h"
#include "game/friendship/friendship.h"
#include "game/member/member.h"
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

bool Claim(const Mission& m, save::SaveData& save, const Context* context) {
    if (save.claimed.contains(m.id) || !ProgressOf(m, save, context).Done()) return false;
    save.claimed.insert(m.id);
    if (m.reward_type == 3) {
        save.boosters = std::min(save.boosters + std::max(1, m.reward_amount), 9);
    } else {
        save.owned[{m.reward_type, m.reward_id}] += std::max(1, m.reward_amount);
    }
    return true;
}

std::vector<int> Owned(const save::SaveData& save, int reward_type) {
    std::vector<int> ids;
    for (const auto& [key, count] : save.owned) {
        if (key.first == reward_type && count > 0) ids.push_back(key.second);
    }
    return ids;
}

Progress ProgressOf(const Mission& m, const save::SaveData& save, const Context* context) {
    const Context none;
    const Context& c = context ? *context : none;
    const auto group_of = [](int mode) { return mode == 1 ? 1 : 0; };  // a group row's p1: 0 µ's, 1 Aqours (Q1)
    const auto key = [&](const char* name) { return save::GroupKey(group_of(m.p1), name); };
    const auto member_key = [&](const char* name) { return std::string(name) + "." + std::to_string(m.p1); };
    // the costumes of a mode (a recipe's group: µ's, else Aqours and Saint Snow)
    const auto costumes_of = [&](int mode, bool owned_only) {
        int n = 0;
        if (!c.costumes) return n;
        for (const costume::Recipe& r : c.costumes->recipes) {
            if ((r.Group() == 0 ? 0 : 1) != mode) continue;
            if (!owned_only || costume::Owns(*c.costumes, save, c.addons, r.dress)) ++n;
        }
        return n;
    };
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
        // the group rows (p1 = the group) count per group (M15 Q2): the played member's group's totals
        case 0: p.have = save::Count(save, key("plays")); break;                          // [p2] Live Shows
        case 1: p.need = 1, p.have = save::Count(save, key("full_combos")) > 0; break;    // a FULL COMBO
        case 2: p.need = 1, p.have = save::Count(save, key("best_rank")) >= 5; break;     // rank S (stored + 1)
        case 3: p.need = 1, p.have = save::Count(save, key("best_rank")) >= 7; break;     // rank SSS
        case 4: p.have = save::Count(save, key("best_score")); break;                     // a score of [p2]
        case 5: p.have = save::Count(save, key("greats")); break;                         // [p2] GREAT notes
        case 6: p.have = save::Count(save, key("perfects")); break;                       // [p2] PERFECT notes
        case 35:  // play "[p1]" [p2] times
            p.have = plays;
            break;
        case 38:  // a FULL COMBO on "[p1]"
            p.need = 1, p.have = full_combos > 0;
            break;
        case 39:  // a score of [p2] on "[p1]"
            p.have = best_score;
            break;
        case 7: p.have = c.costumes ? costumes_of(group_of(m.p1), true) : -1; break;  // [p2] types of costumes
        // [p2] key items / threads / fabrics collected (shortcut: counted for both groups together)
        case 8: p.have = save::Count(save, "collected.key"); break;
        case 9: p.have = save::Count(save, "collected.thread"); break;
        case 10: p.have = save::Count(save, "collected.cloth"); break;
        case 11: {  // every key item of the group collected
            if (!c.costumes) break;
            p.need = p.have = 0;
            for (const costume::Item& i : c.costumes->items) {
                if (i.type != (group_of(m.p1) == 0 ? 1 : 2)) continue;
                ++p.need;
                p.have += save::Marked(save, "keys", i.id);
            }
            break;
        }
        case 12: p.have = save::Count(save, "views"); break;   // [p2] View Live Shows
        case 13: p.have = save::Count(save, "photos"); break;  // [p2] photos
        case 20: {  // select [p1] and play [p2] times
            const auto b = save.bonds.find(m.p1);
            p.have = b == save.bonds.end() ? 0 : b->second.lives;
            break;
        }
        case 21: p.have = save::SetSize(save, member_key("talk")); break;  // [p2] of [p1]'s phrases
        case 22:   // [p2] types of [p1]'s skill cards
        case 23:   // every type of [p1]'s skill cards
        case 31: { // [p2] of [p1]'s normal skill cards at MAX (shortcut: "normal" = a card that levels, max > 1)
            if (!c.cards) break;
            int all = 0, owned = 0, maxed = 0;
            for (const card::SkillCard& k : c.cards->skills) {
                if (k.member != m.p1) continue;
                ++all;
                const int level = card::Level(*c.cards, save, k.id);
                owned += level > 0;
                maxed += k.max_level > 1 && level >= k.max_level;
            }
            p.have = m.Kind() == 31 ? maxed : owned;
            if (m.Kind() == 23) p.need = std::max(1, all);
            break;
        }
        case 24: p.have = save::Count(save, member_key("cards")); break;            // [p2] profile cards of [p1]
        case 26: p.have = save::Count(save, member_key("camera")) / 60; break;      // [p2] minutes on [p1]
        case 27: p.have = save::SetSize(save, member_key("menu")); break;           // [p2] costumes shown
        case 28:                                                                     // every costume shown
            p.have = save::SetSize(save, member_key("menu"));
            p.need = std::max(1, costumes_of(member::Mode(m.p1), false));
            break;
        case 29: {  // [p2] of [p1]'s accolades: the accolades her member missions give
            if (!c.missions) break;
            std::set<int> hers;
            for (const Mission& r : c.missions->rows) {
                if (r.reward_type == 4 && r.p1 == m.p1 && r.Kind() >= 20 && r.Kind() <= 34) hers.insert(r.reward_id);
            }
            p.have = 0;
            for (const int id : hers) p.have += save.owned.contains({4, id});
            break;
        }
        case 33: p.need = 1, p.have = save::Marked(save, "camera_photo", m.p1); break;  // a Member camera photo of her
        case 36: p.have = save::Count(save, "view." + std::to_string(m.p1)); break;   // view "[p1]" [p2] times
        case 40: p.need = 1, p.have = c.costumes && costume::Owns(*c.costumes, save, c.addons, m.p1); break;
        case 42: {  // wear "[p1]" on the Main Menu (any member)
            p.need = 1, p.have = 0;
            for (const auto& [k, ids] : save.sets) p.have |= k.rfind("menu.", 0) == 0 && ids.contains(m.p1);
            break;
        }
        case 43: p.need = 1, p.have = save::Marked(save, "view_costumes", m.p1); break;  // view with "[p1]"
        case 45: p.have = save::Count(save, "photo." + std::to_string(m.p1)); break;     // photos in "[p1]"
        case 46: p.need = 1, p.have = save::Marked(save, "card_costumes", m.p1); break;  // a profile card in it
        case 48: p.need = 1, p.have = save::Marked(save, "special", m.p1); break;        // its special colour
        case 41: {  // play wearing "[p1]" [p2] times
            const auto it = save.costume_plays.find(m.p1);
            p.have = it == save.costume_plays.end() ? 0 : it->second;
            break;
        }
        case 18: p.have = save.logins; break;  // [p2] login bonuses (M14)
        case 30: p.have = save.boosters_used; break;  // use [p2] boosters (shortcut: counted for every member, not [p1]'s lives)
        case 32: {  // [p1]'s Friendship to Lv. [p2] (M14)
            const auto b = save.bonds.find(m.p1);
            p.have = c.levels ? friendship::LevelOf(*c.levels, b == save.bonds.end() ? 0 : b->second.points) : -1;
            break;
        }
        case 34: {  // log in during [p1]'s birthday event and go to the Main Menu (the top menu notes it, M14)
            const auto b = save.birthdays.find(m.p1);
            p.need = 1, p.have = b != save.birthdays.end() && b->second.year > 0;
            break;
        }
        // not counted: 14, 15, 37, 47 (Live Show Medley), 25 (Finale Focus), 16, 19 (online), 44 (profile-card
        // matching), 17 (HR member cards: shortcut, their source is not traced)
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
