#include "game/costume/costume.h"

#include <algorithm>
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
    for (size_t r = 192; r + 192 <= t.size(); r += 192) {  // record 0 is empty
        Recipe recipe{At<int32_t>(t, r), At<uint32_t>(t, r + 0x58), {}, {}};
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
        std::vector<Drop>& drops = out.drops[At<int32_t>(t, r)];
        for (int item = 1; item <= 60; ++item) {
            const Drop d{item, At<float>(t, r + 0x15C + 8 * size_t(item - 1)), At<int32_t>(t, r + 0x160 + 8 * size_t(item - 1))};
            if (d.chance > 0 && d.amount > 0) drops.push_back(d);
        }
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

std::vector<std::pair<int, int>> RollDrops(const Data& data, int song, std::mt19937& random, save::SaveData& save) {
    std::vector<std::pair<int, int>> got;
    const auto it = data.drops.find(song);
    if (it == data.drops.end()) return got;
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    for (const Drop& d : it->second) {
        if (roll(random) < d.chance) {
            save.owned[{kItemReward, d.item}] += d.amount;
            got.push_back({d.item, d.amount});
        }
    }
    return got;
}

}
