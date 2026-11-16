#pragma once
// Costume materials and crafting (M7 finish plan step 2, plan/202611021940-m7-systems-breadth/202611071847-plan.md):
// the item table, each costume's recipe, the materials each song drops, and which costumes the player owns.
// See docs/formats/costume.md.

#include <array>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace sif {
class Vfs;
namespace dlc {
struct Ownership;
}
namespace save {
struct SaveData;
}

namespace costume {

constexpr int kItemReward = 2, kCostumeReward = 7;  // mission reward types; save.owned keys
std::string ThumbPath(int dress);  // ui/ui_dress_thumbnail.pac dress_thumb_NNNN (448 x 448; none for d000)

// common/common.pac/item.pac/item_data.bin ll3::item::s_item_datas: 40-byte rows, +0 id, +4 type, +8 4 x name
struct Item {
    int id = 0;
    int type = 0;  // 1 µ's key item, 2 Aqours key item, 3/4 common thread/cloth, 5/6 µ's, 7/8 Aqours thread/cloth
    std::array<std::string, 4> name;  // Japanese, English, simplified, traditional Chinese
};
// s_dress_datas row +0x58: u32 flags, 12 item ids, 12 amounts (Hypothesis on the flag bits, from all 70 rows: bit 0
// craftable, bit 1 a reward costume, bit 16 special colours; low byte 0 = owned from the start: the two School
// Unity and Believe again, as on a new game's Create/Change Costumes screen)
struct Recipe {
    int dress = 0;
    int type = 0;  // +0x04: costume type 0-4, the key-material pair it favours (messages 803-812; s_itemCombineDatas row)
    unsigned flags = 0;
    std::vector<std::pair<int, int>> needs;  // (item id, amount)
    std::array<std::string, 4> text;         // the description (+0x30, 4 languages)
    int Group() const { return dress >= 200 ? 2 : dress >= 100 ? 1 : 0; }  // µ's, Aqours, Saint Snow by id range
    bool StartsOwned() const { return (flags & 0xFF) == 0; }
    bool Craftable() const;  // flag bit 0 and every amount below 999 (999 = not made from materials)
};
// s_music_datas row +0x15C: one (f32 chance, i32 cap) per item id 1-60, the song's "Obtainable Materials" (18 per
// song, the group's own items plus the common ones). The cap is how many times the item is tried in the first pass
// of the drop (eboot FUN_000679a0, low 16 bits).
struct Drop {
    int item = 0;
    float chance = 0;
    int amount = 0;  // the cap
};
// What the material drop reads besides the song's list (M11 S6b; analysis 202611151530-skills-and-cards/202611161517):
struct DropRules {
    std::array<std::pair<int, int>, 7> count{};         // s_CommonParam +0x150: (min, max) units by rank D .. SSS
    std::array<float, 9> difficulty{};                  // +0xF4: chance factor by difficulty index 0-8 (0.8 .. 1.0)
    float key_bonus = 1.2f;                             // +0x1A8: key chances when the costume matches the song
    std::array<std::array<float, 5>, 5> combine{};      // item_combine_data.bin s_itemCombineDatas [costume][song type]
};
struct Data {
    std::vector<Item> items;
    std::vector<Recipe> recipes;
    std::map<int, std::vector<Drop>> drops;  // by song id
    std::map<int, int> song_type;            // s_music_datas +0x158: song type 0-4 (s_itemCombineDatas column)
    int base_type = 0;                       // the type of s_dress_datas record 0 (costumes without a record, d000)
    DropRules rules;
};

bool Load(const Vfs& vfs, Data& out, std::string* error = nullptr);
const Item* FindItem(const Data& data, int id);
const Recipe* FindRecipe(const Data& data, int dress);
// ui/ui_material_icon.pac/texture.pac/material_icon_<key|yarn|clo>_<ms|aq|com>_NNN
std::string IconPath(const Item& item);
int ItemCount(const save::SaveData& save, int item);

// d000 (the uniform), a costume owned from the start, one rewarded or crafted (save.owned type 7), or one an
// installed add-on gives (`addons` may be null).
bool Owns(const Data& data, const save::SaveData& save, const dlc::Ownership* addons, int dress);
bool CanCraft(const Recipe& recipe, const save::SaveData& save);
// Spends the materials and adds the costume; false (nothing changes) if it cannot be made.
bool Craft(const Recipe& recipe, save::SaveData& save);
// One finished live, for the material drop.
struct DropInput {
    int song = 0;
    int rank = 0;        // 0 D .. 6 SSS (Hypothesis: the game's index at save +0xF0914 is this rank)
    int difficulty = 0;  // 0-8 (variants 00-04, then 10-13)
    int dress = 0;       // the costume worn (0 = the uniform / base model)
    std::map<int, int> skills;  // material skill sums by card::Kind (27-35): the played member's set cards
};
// The materials one finished live gives (eboot FUN_000679a0): a count from the rank, the song's materials tried in
// order (keys first, then by weight) up to their caps, the rest drawn among the five most likely; then the material
// skills' conversions. Adds them to the save and returns (item, amount) in the order first gained.
std::vector<std::pair<int, int>> RollDrops(const Data& data, const DropInput& in, std::mt19937& random, save::SaveData& save);

}
}
