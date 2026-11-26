#include "game/card/card.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <random>
#include <span>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"
#include "game/save/save.h"

namespace sif::card {
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

const LevelRow& SkillCard::At(int level) const {
    static const LevelRow none;
    if (levels.empty()) return none;
    return levels[size_t(std::clamp(level, 1, std::min(max_level, int(levels.size()))) - 1)];
}

bool Load(const Vfs& vfs, Data& out, std::string* error) {
    out = {};
    std::vector<uint8_t> f;
    std::span<const uint8_t> d;
    if (!Table(vfs, "common/common.pac/card.pac/skill_card_datas.bin", "s_skill_card_datas", f, d, error)) return false;
    for (size_t o = 0; o + 128 <= d.size(); o += 128) {
        SkillCard c;
        c.id = At<int32_t>(d, o), c.skill = At<int32_t>(d, o + 4), c.member = At<int32_t>(d, o + 8);
        c.kind = At<int32_t>(d, o + 0x10);
        for (int l = 0; l < 4; ++l) {
            c.name[size_t(l)] = Str(f, At<uint64_t>(d, o + 0x18 + 8 * size_t(l)));
            c.text[size_t(l)] = Str(f, At<uint64_t>(d, o + 0x38 + 8 * size_t(l)));
        }
        c.picture = Str(f, At<uint64_t>(d, o + 0x58));
        c.max_level = std::max(1, At<int32_t>(d, o + 0x6C));
        c.no_drop = At<uint8_t>(d, o + 0x7C) != 0;
        // +0x70: file offset of the level table, 11 rows of 48 bytes; row 0 repeats row 1 (levels are 1-10)
        const uint64_t table = At<uint64_t>(d, o + 0x70);
        const std::span<const uint8_t> file(f);
        for (int row = 1; row <= 10 && table + 48 * uint64_t(row + 1) <= f.size(); ++row) {
            const size_t r = size_t(table) + 48 * size_t(row);
            LevelRow lv;
            lv.seconds = At<float>(file, r), lv.amount = At<int32_t>(file, r + 4), lv.share = At<int32_t>(file, r + 8);
            lv.amount_alt = At<int32_t>(file, r + 12);
            for (int l = 0; l < 4; ++l) lv.text[size_t(l)] = Str(f, At<uint64_t>(file, r + 16 + 8 * size_t(l)));
            c.levels.push_back(std::move(lv));
        }
        out.skills.push_back(std::move(c));
    }
    if (!Table(vfs, "common/common.pac/card.pac/member_card_datas.bin", "s_member_card_datas", f, d, error)) return false;
    for (size_t o = 0; o + 28 <= d.size(); o += 28) {
        out.members.push_back({At<int32_t>(d, o), At<int32_t>(d, o + 8), At<int32_t>(d, o + 0x10), At<int32_t>(d, o + 0x14)});
    }
    // 20-byte rows: +0 id, +4 pool (0 skill cards, 1 boosters, 2 and 3 not decoded), +0x10 the item (eboot 0x1F2960)
    if (!Table(vfs, "common/common.pac/reward.pac/reward_data.bin", "ll3::reward::s_reward_datas", f, d, error)) return false;
    for (size_t o = 0; o + 20 <= d.size(); o += 20) {
        if (At<int32_t>(d, o + 4) == 0) out.drop_pool.push_back(At<int32_t>(d, o + 0x10));
    }
    if (out.skills.empty() || out.members.empty() || out.drop_pool.empty()) {
        if (error) *error = "card tables are empty";
        return false;
    }
    return true;
}

const SkillCard* FindSkill(const Data& data, int id) {
    for (const SkillCard& c : data.skills) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

std::string ThumbPath(const Data* cards, int member, int dress) {
    if (!cards) return {};
    for (const MemberCard& c : cards->members) {
        if (c.member == member && c.costume == dress && c.id % 10 == 0) {
            return "ui/ui_member_card_thumbnail.pac/texture.pac/mbc_" + std::to_string(c.id);
        }
    }
    return {};
}

int Level(const Data& data, const save::SaveData& save, int id) {
    const auto it = save.owned.find({kSkillCardReward, id});
    if (it == save.owned.end() || it->second <= 0) return 0;
    const SkillCard* c = FindSkill(data, id);
    return c ? std::min(it->second, c->max_level) : 0;
}

int AddSkill(const Data& data, save::SaveData& save, int id) {
    ++save.owned[{kSkillCardReward, id}];
    return Level(data, save, id);
}

std::vector<const SkillCard*> OwnedSkills(const Data& data, const save::SaveData& save, int member) {
    std::vector<const SkillCard*> cards;
    for (const SkillCard& c : data.skills) {
        if (c.member == member && Level(data, save, c.id) > 0) cards.push_back(&c);
    }
    return cards;
}

std::array<int, kSlots> Slots(const Data& data, const save::SaveData& save, int member) {
    std::array<int, kSlots> slots{};
    const auto it = save.skill_slots.find(member);
    if (it == save.skill_slots.end()) return slots;
    for (int s = 0; s < kSlots; ++s) {
        const int id = it->second[size_t(s)];
        const SkillCard* c = FindSkill(data, id);
        if (c && c->member == member && Level(data, save, id) > 0) slots[size_t(s)] = id;
    }
    return slots;
}

int SkillSum(const Data& data, const save::SaveData& save, int member, int kind) {
    int sum = 0;
    for (int id : Slots(data, save, member)) {
        const SkillCard* c = FindSkill(data, id);
        if (c && c->kind == kind) sum += c->At(Level(data, save, id)).amount;
    }
    return sum;
}

int DrawSkillCard(const Data& data, int member, int boost, std::mt19937& random) {
    // eboot FUN_001f2ab0 pool 0: member index 0-8 (µ's 1-9) weigh 11 each, 9-19 (11-19, 21-22) weigh 9 each; the
    // played member's weight gains the Skill Drop Boost sum (FUN_000a68d0)
    static constexpr int kMembers[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22};
    const bool mus = member >= 1 && member <= 9;
    const int first = mus ? 0 : 9, last = mus ? 9 : 20;
    const double base = mus ? 11 : 9;
    double total = 0;
    for (int i = first; i < last; ++i) total += base + (kMembers[i] == member ? boost : 0);
    double r = std::uniform_real_distribution<double>(0, total)(random);
    int picked = kMembers[last - 1];
    for (int i = first; i < last; ++i) {
        r -= base + (kMembers[i] == member ? boost : 0);
        if (r < 0) {
            picked = kMembers[i];
            break;
        }
    }
    // then one card uniformly among that member's cards in the pool
    std::vector<int> cards;
    for (int id : data.drop_pool) {
        const SkillCard* c = FindSkill(data, id);
        if (c && c->member == picked) cards.push_back(id);
    }
    if (cards.empty()) return 0;
    return cards[std::uniform_int_distribution<size_t>(0, cards.size() - 1)(random)];
}

std::vector<int> RollSkillDrops(const Data& data, const save::SaveData& save, int member, int rank, std::mt19937& random) {
    std::vector<int> got;
    const int boost = SkillSum(data, save, member, kSkillDropBoost);
    if (const int id = DrawSkillCard(data, member, boost, random)) got.push_back(id);
    // a second draw at rank S or better (result +0xC87 > 3) with Getting Skills Charge %: roll x 100 <= the sum
    const int charge = SkillSum(data, save, member, kGettingSkillsCharge);
    if (rank > 3 && charge > 0 && std::uniform_real_distribution<double>(0, 100)(random) <= charge) {
        if (const int id = DrawSkillCard(data, member, boost, random)) got.push_back(id);
    }
    return got;
}

}
