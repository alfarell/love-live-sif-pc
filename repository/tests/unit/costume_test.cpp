// Costume materials and crafting (src/game/costume/costume.cpp) against the user's own dump.
#include <cstdio>
#include <random>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/costume/costume.h"
#include "game/dlc/dlc.h"
#include "game/save/save.h"

using namespace sif;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sif_costume_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    costume::Data data;
    std::string error;
    Check(costume::Load(vfs, data, &error), "load: " + error);
    Check(data.items.size() == 60, "60 items (rows 0 and 'max' left out): " + std::to_string(data.items.size()));
    const costume::Item* white = costume::FindItem(data, 1);
    Check(white && white->name[1] == "White thread" && white->type == 3, "item 1 White thread, a common thread");
    Check(white && costume::IconPath(*white) == "ui/ui_material_icon.pac/texture.pac/material_icon_yarn_com_001", "item 1 icon");
    std::vector<uint8_t> icon;
    Check(white && vfs.Read(costume::IconPath(*white), icon), "item 1 icon is in the dump");

    const costume::Recipe* live = costume::FindRecipe(data, 31);  // Bokura no LIVE Kimi to no LIFE
    Check(live && live->Craftable() && !live->StartsOwned() && live->needs.size() == 11 && live->needs[0] == std::pair{1, 30},
          "costume 31: craftable, 11 materials, 30 White thread first");
    const costume::Recipe* unity = costume::FindRecipe(data, 3);
    Check(unity && unity->StartsOwned() && !unity->Craftable(), "costume 3 School Unity: owned from the start");
    Check(data.drops[10].size() == 18 && data.drops[10][0].item == 1, "song 0010 drops 18 materials, White thread first");

    save::SaveData save;
    Check(costume::Owns(data, save, nullptr, 0) && costume::Owns(data, save, nullptr, 3), "uniform and School Unity owned");
    Check(!costume::Owns(data, save, nullptr, 31), "costume 31 not owned on a new save");
    dlc::Ownership addons;
    addons.addon_costumes.insert(31);
    Check(costume::Owns(data, save, &addons, 31), "costume 31 owned through its add-on");
    Check(live && !costume::Craft(*live, save), "no materials: no craft");
    if (live) {
        for (const auto& [item, amount] : live->needs) save.owned[{costume::kItemReward, item}] = amount + 1;
        Check(costume::Craft(*live, save) && costume::Owns(data, save, nullptr, 31), "craft costume 31");
        Check(costume::ItemCount(save, 1) == 1 && !costume::Craft(*live, save), "materials spent; made once");
    }
    std::mt19937 random(1);
    int got = 0;
    for (int i = 0; i < 100; ++i) got += int(costume::RollDrops(data, 10, random, save).size());
    Check(got > 100 && got < 1500, "100 lives of 0010 drop some but not all materials: " + std::to_string(got));
    std::printf("%s\n", failures ? "costume_test FAILED" : "costume_test ok");
    return failures ? 1 : 0;
}
