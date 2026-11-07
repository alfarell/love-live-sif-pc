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
    unsigned flags = 0;
    std::vector<std::pair<int, int>> needs;  // (item id, amount)
    std::array<std::string, 4> text;         // the description (+0x30, 4 languages)
    int Group() const { return dress >= 200 ? 2 : dress >= 100 ? 1 : 0; }  // µ's, Aqours, Saint Snow by id range
    bool StartsOwned() const { return (flags & 0xFF) == 0; }
    bool Craftable() const;  // flag bit 0 and every amount below 999 (999 = not made from materials)
};
// s_music_datas row +0x15C: one (f32 chance, i32 amount) per item id 1-60 (Hypothesis: the song's "Obtainable
// Materials"; 18 per song, the group's own items plus the common ones)
struct Drop {
    int item = 0;
    float chance = 0;
    int amount = 0;
};
struct Data {
    std::vector<Item> items;
    std::vector<Recipe> recipes;
    std::map<int, std::vector<Drop>> drops;  // by song id
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
// The materials one finished live of `song` gives (each drop: `chance` to get `amount`; Hypothesis, the game's
// rank or costume bonuses are not traced); adds them to the save and returns them.
std::vector<std::pair<int, int>> RollDrops(const Data& data, int song, std::mt19937& random, save::SaveData& save);

}
}
