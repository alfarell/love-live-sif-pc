#include "game/costume/costume.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"
#include "game/dlc/dlc.h"
#include "game/save/save.h"

namespace sif::costume {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

template <class T> T At(std::span<const uint8_t> d, size_t off) {
    T v{};
    if (off + sizeof(T) <= d.size()) std::memcpy(&v, d.data() + off, sizeof(T));
    return v;
}

std::string Str(const std::vector<uint8_t>& file, uint64_t at) {
    if (at >= file.size()) return {};
    const char* t = reinterpret_cast<const char*>(&file[size_t(at)]);
    return std::string(t, strnlen(t, file.size() - size_t(at)));
}

bool Table(const Vfs& vfs, const char* path, const char* symbol, std::vector<uint8_t>& file, std::span<const uint8_t>& out,
           std::string* error) {
    std::vector<blbn::Symbol> symbols;
    const blbn::Symbol* s = nullptr;
    if (!vfs.Read(path, file, error) || !blbn::Parse(file, symbols, error) || !(s = blbn::Find(symbols, symbol))) {
        return Fail(error, std::string("no ") + symbol + " in " + path);
    }
    out = s->data;
    return true;
}

}

bool Recipe::Craftable() const {
    return (flags & 1) && !needs.empty() &&
           std::none_of(needs.begin(), needs.end(), [](const auto& n) { return n.second >= 999; });
}

bool Load(const Vfs& vfs, Data& out, std::string* error) {
    out = {};
    std::vector<uint8_t> file;
    std::span<const uint8_t> t;
    if (!Table(vfs, "common/common.pac/item.pac/item_data.bin", "ll3::item::s_item_datas", file, t, error)) return false;
    for (size_t r = 0; r + 40 <= t.size(); r += 40) {
        Item item{At<int32_t>(t, r), At<int32_t>(t, r + 4), {}};
        if (item.type <= 0) continue;  // row 0 "none" and the last "max"
        for (size_t k = 0; k < 4; ++k) item.name[k] = Str(file, At<uint64_t>(t, r + 8 + 8 * k));
        out.items.push_back(std::move(item));
    }
    if (!Table(vfs, "common/common.pac/dress.pac/dress_data.bin", "ll3::dress::s_dress_datas", file, t, error)) return false;
    out.base_type = At<int32_t>(t, 4);
    for (size_t r = 192; r + 192 <= t.size(); r += 192) {  // record 0 is empty
        Recipe recipe{At<int32_t>(t, r), At<int32_t>(t, r + 4), At<uint32_t>(t, r + 0x58), {}, {}};
        if (recipe.dress <= 0 || recipe.dress >= 1000) break;
        for (size_t k = 0; k < 4; ++k) recipe.text[k] = Str(file, At<uint64_t>(t, r + 0x30 + 8 * k));
        for (size_t k = 0; k < 12; ++k) {
            const int item = At<int32_t>(t, r + 0x5C + 4 * k), amount = At<int32_t>(t, r + 0x8C + 4 * k);
            if (item > 0 && amount > 0) recipe.needs.push_back({item, amount});
        }
        out.recipes.push_back(std::move(recipe));
    }
    if (!Table(vfs, "common/common.pac/live_common.pac/live_datas.bin", "s_music_datas", file, t, error)) return false;
    for (size_t r = 0; r + 832 <= t.size(); r += 832) {
        out.song_type[At<int32_t>(t, r)] = At<int32_t>(t, r + 0x158);
        std::vector<Drop>& drops = out.drops[At<int32_t>(t, r)];
        for (int item = 1; item <= 60; ++item) {
            const Drop d{item, At<float>(t, r + 0x15C + 8 * size_t(item - 1)), At<int32_t>(t, r + 0x160 + 8 * size_t(item - 1))};
            if (d.chance > 0 && d.amount > 0) drops.push_back(d);
        }
    }
    // the drop's rules (FUN_000679a0's inputs)
    if (!Table(vfs, "common/common.pac/live_common.pac/live_params.bin", "s_CommonParam", file, t, error)) return false;
    for (size_t k = 0; k < 7; ++k) out.rules.count[k] = {At<int32_t>(t, 0x150 + 8 * k), At<int32_t>(t, 0x154 + 8 * k)};
    for (size_t k = 0; k < 9; ++k) out.rules.difficulty[k] = At<float>(t, 0xF4 + 4 * k);
    out.rules.key_bonus = At<float>(t, 0x1A8);
    if (!Table(vfs, "common/common.pac/item.pac/item_combine_data.bin", "ll3::item::s_itemCombineDatas", file, t, error)) {
        return false;
    }
    for (size_t row = 0; row < 5; ++row) {
        for (size_t col = 0; col < 5; ++col) out.rules.combine[row][col] = At<float>(t, 20 * row + 4 * col);
    }
    if (out.items.empty() || out.recipes.empty()) return Fail(error, "no items or costume recipes");
    return true;
}

const Item* FindItem(const Data& data, int id) {
    for (const Item& i : data.items) {
        if (i.id == id) return &i;
    }
    return nullptr;
}

const Recipe* FindRecipe(const Data& data, int dress) {
    for (const Recipe& r : data.recipes) {
        if (r.dress == dress) return &r;
    }
    return nullptr;
}

std::string ThumbPath(int dress) {
    char s[80];
    std::snprintf(s, sizeof(s), "ui/ui_dress_thumbnail.pac/texture.pac/dress_thumb_%04d", dress);
    return s;
}

std::string IconPath(const Item& item) {
    static constexpr const char* kKind[] = {"", "key_ms", "key_aq", "yarn_com", "clo_com", "yarn_ms", "clo_ms", "yarn_aq", "clo_aq"};
    char s[96];
    std::snprintf(s, sizeof(s), "ui/ui_material_icon.pac/texture.pac/material_icon_%s_%03d",
                  kKind[std::clamp(item.type, 0, 8)], item.id);
    return s;
}

int ItemCount(const save::SaveData& save, int item) {
    const auto it = save.owned.find({kItemReward, item});
    return it == save.owned.end() ? 0 : it->second;
}

bool Owns(const Data& data, const save::SaveData& save, const dlc::Ownership* addons, int dress) {
    if (dress == 0) return true;
    const Recipe* r = FindRecipe(data, dress);
    if (r && r->StartsOwned()) return true;
    if (save.owned.contains({kCostumeReward, dress})) return true;
    return addons && addons->addon_costumes.contains(dress);
}

bool CanCraft(const Recipe& recipe, const save::SaveData& save) {
    return recipe.Craftable() && std::all_of(recipe.needs.begin(), recipe.needs.end(),
                                             [&](const auto& n) { return ItemCount(save, n.first) >= n.second; });
}

bool Craft(const Recipe& recipe, save::SaveData& save) {
    if (!CanCraft(recipe, save) || save.owned.contains({kCostumeReward, recipe.dress})) return false;
    for (const auto& [item, amount] : recipe.needs) {
        if ((save.owned[{kItemReward, item}] -= amount) <= 0) save.owned.erase({kItemReward, item});
    }
    save.owned[{kCostumeReward, recipe.dress}] = 1;
    return true;
}

std::vector<std::pair<int, int>> RollDrops(const Data& data, const DropInput& in, std::mt19937& random, save::SaveData& save) {
    std::vector<std::pair<int, int>> got;  // (item, amount) in the order first gained
    const auto list = data.drops.find(in.song);
    if (list == data.drops.end()) return got;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const auto skill = [&](int kind) {
        const auto it = in.skills.find(kind);
        return it == in.skills.end() ? 0.0f : float(it->second);
    };
    const auto chance = [&](float pct) { return pct > 0 && unit(random) * 100.0f <= pct; };  // xorshift x 100 <= v
    const auto boost = [](float pct) { return pct > 0 ? (pct + 100.0f) / 100.0f : 0.0f; };   // (v + 100) / 100
    // the costume: its type against the song's type (s_itemCombineDatas, 1.0 .. 1.5 on the diagonal); a match also
    // favours the key materials (Hypothesis: the flag at save +0xF0A40 bit 1 is this match)
    const Recipe* worn = FindRecipe(data, in.dress);
    const int costume_type = std::clamp(worn ? worn->type : data.base_type, 0, 4);
    const auto st = data.song_type.find(in.song);
    const int song_type = std::clamp(st == data.song_type.end() ? 0 : st->second, 0, 4);
    const float combine = data.rules.combine[size_t(costume_type)][size_t(song_type)];
    const bool key_match = costume_type == song_type;
    const float difficulty = data.rules.difficulty[size_t(std::clamp(in.difficulty, 0, 8))];
    // the material skills (kinds 27-35; FUN_000caaa0 sums), each read once per live as in the eboot
    const float all = boost(skill(27)), common = boost(skill(30)), keys = boost(skill(33));
    const float costume_keys = key_match ? boost(skill(35)) : 0.0f;
    const float yarn_cap = chance(skill(31)) ? 1.5f : 0.0f, cloth_cap = chance(skill(32)) ? 1.5f : 0.0f;
    const bool double_keys = chance(skill(34));
    struct Entry {
        const Item* item;
        float weight;
        int cap;
    };
    std::vector<Entry> entries;
    for (const Drop& d : list->second) {
        const Item* item = FindItem(data, d.item);
        const int cap = d.amount & 0xFFFF;
        if (!item || cap == 0 || d.chance <= 0) continue;
        Entry e{item, d.chance * difficulty * combine, cap};
        switch (item->type) {
        case 1:
        case 2:  // keys
            if (key_match) e.weight *= data.rules.key_bonus * (costume_keys > 0 ? costume_keys : 1.0f);
            if (keys > 0) e.weight *= keys;
            break;
        case 3:
        case 4:  // common yarn / cloth
            if (common > 0) e.weight *= common;
            [[fallthrough]];
        default: {  // yarn types 3, 5, 7, 9, 11 and cloth types 4, 6, 8, 10, 12: the cap skills
            const float f = item->type % 2 ? yarn_cap : cloth_cap;
            if (f > 0) e.cap = int(int16_t(f * float(e.cap)));
        }
        }
        if (all > 0) e.weight *= all;
        entries.push_back(e);
    }
    // keys first, then by weight, highest first (both sorts stable: FUN_00069CA0, FUN_0006B0D0)
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.weight > b.weight; });
    std::stable_partition(entries.begin(), entries.end(), [](const Entry& e) { return e.item->type == 1 || e.item->type == 2; });
    std::vector<int> common_yarn, common_cloth, group_yarn, group_cloth;  // indexes into `got`, by item type
    const auto give = [&](const Item* item) {
        const auto add = [&] {
            for (size_t k = 0; k < got.size(); ++k) {
                if (got[k].first == item->id) {
                    ++got[k].second;
                    return int(k);
                }
            }
            got.push_back({item->id, 1});
            return int(got.size() - 1);
        };
        const int at = add();
        if (double_keys && (item->type == 1 || item->type == 2)) add();
        std::vector<int>* by_type = item->type == 3 ? &common_yarn : item->type == 4 ? &common_cloth
                                  : item->type >= 5 && item->type % 2 ? &group_yarn : item->type >= 5 ? &group_cloth : nullptr;
        if (by_type && std::find(by_type->begin(), by_type->end(), at) == by_type->end()) by_type->push_back(at);
    };
    // how many units: uniform in the rank's range (shortcut: the Wai-Wai! Booster's +10 is left out, no boosters)
    const auto [lo, hi] = data.rules.count[size_t(std::clamp(in.rank, 0, 6))];
    int left = lo + int(unit(random) * float(hi - lo + 1));
    // pass 1: rounds over the list; each try uses one of the item's cap and gives one unit when the roll is within its
    // weight
    for (bool more = true; left > 0 && more;) {
        more = false;
        for (Entry& e : entries) {
            if (left == 0) break;
            if (e.cap == 0) continue;
            --e.cap;
            if (unit(random) <= e.weight) {
                give(e.item);
                --left;
            }
            if (e.cap != 0) more = true;
        }
    }
    // pass 2: the units still owed, by weight among the five most likely (FUN_0006C160 sorts again, no keys first)
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.weight > b.weight; });
    const size_t top = std::min<size_t>(5, entries.size());
    while (left > 0 && top > 0) {
        float total = 0;
        for (size_t k = 0; k < top; ++k) total += entries[k].weight;
        const float r = total * unit(random);
        size_t pick = top;
        float sum = 0;
        for (size_t k = 0; k < top && pick == top; ++k) {
            sum += entries[k].weight;
            if (r <= sum) pick = k;
        }
        if (pick == top) break;  // no weight at all
        give(entries[pick].item);
        --left;
    }
    // Converting Threads / Fabric (kinds 28, 29): ceil(v% of each common unit) move to the group's own yarns / cloths
    // that dropped, spread evenly, the rest one each to random ones
    const auto convert = [&](const std::vector<int>& from, const std::vector<int>& to, float pct) {
        if (to.empty() || pct <= 0) return;
        int moved = 0;
        for (int k : from) {
            const int n = int(std::ceil(pct * float(got[size_t(k)].second) / 100.0f));
            moved += n;
            got[size_t(k)].second -= n;
        }
        for (int k : to) got[size_t(k)].second += moved / int(to.size());
        for (int r = 0; r < moved % int(to.size()); ++r) {
            got[size_t(to[size_t(unit(random) * float(to.size())) % to.size()])].second += 1;
        }
    };
    convert(common_yarn, group_yarn, skill(28));
    convert(common_cloth, group_cloth, skill(29));
    std::erase_if(got, [](const auto& g) { return g.second <= 0; });
    for (const auto& [item, amount] : got) save.owned[{kItemReward, item}] += amount;
    return got;
}

}
