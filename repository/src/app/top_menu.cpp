#include "app/top_menu.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"

namespace sif {
namespace {

constexpr int kModes = 6;   // the wheel's modes (0x1BD2D0: indices taken mod 6)
constexpr int kIcons = 8;   // placed wheel icons (TopMenu +0x388 ... +0x3C0)
constexpr int kSwitch = 20; // ui_animation.bin: clip 20 "switch", posed at the label id on layer 1
// DAT_005F42F0: the submenu list shown for each mode; DAT_005F3F54: entries per submenu list
constexpr std::array<int, kModes> kSubmenuOfMode = {1, 3, 3, 1, 0, 2};
constexpr std::array<int, 4> kEntriesOfSubmenu = {5, 4, 3, 2};
// The entry ids (label cells) per mode, from the pointer table at 0xC085A0 (resolved through its relocations).
// Mode 0 (Play) is filled at runtime (its slot points at code in the file); Hypothesis: the 4 ids at 0x5F4410
// (0 1 2 3 = Play Live Show, Play Live Show (AC), View Live Show, Live Show Medley, as seen in shadPS4)
const std::array<std::vector<int>, kModes> kEntriesOfMode = {{
    {0, 1, 2, 3}, {0, 1}, {5, 7}, {8, 9, 10, 12}, {10, 12, 13, 14, 15}, {15, 16, 17}}};
// The submenu layouts per list index, matched by their banner counts (5, 4, 3, 2): idle and active ("_act") forms
constexpr std::array<const char*, 4> kIdleList = {"uil0003_SubMenuList00", "uil0010_SubMenuList03",
                                                  "uil0004_SubMenuList01", "uil0005_SubMenuList02"};
constexpr std::array<const char*, 4> kActiveList = {"uil0006_SubMenuList00_act", "uil0011_SubMenuList03_act",
                                                    "uil0007_SubMenuList01_act", "uil0008_SubMenuList02_act"};
constexpr const char* kRoot = "uil0001_TopMenu00";

std::string Instance(const std::string& layout, const char* part, int index) {
    char node[64];
    std::snprintf(node, sizeof(node), "%s_%02d", part, index);
    return std::string(kRoot) + "/" + layout + "/" + node;
}

// shortcut: the wheel turns in whole steps (the game eases between them with clips 11/12 "left"/"right")
void Apply(UiScreen& ui, int mode, int entry) {
    for (int i = 0; i < kIcons; ++i) {  // 0x1B94C0: icon i shows label ((position + 3) mod 6) + i
        const std::string icon = Instance("uil0002_MenuList00", "uip0004_MenuIcon00", i);
        ui.Play(icon, UiScreen::ClipName(kSwitch), false, 1);
        ui.SetClipTime(icon, 1, float((mode + 3) % kModes + i));
    }
    const int list = kSubmenuOfMode[size_t(mode)];
    for (int l = 0; l < 4; ++l) {
        ui.HideInstance(std::string(kRoot) + "/" + kIdleList[size_t(l)], true);  // shortcut: the focused (active) form only
        ui.HideInstance(std::string(kRoot) + "/" + kActiveList[size_t(l)], l != list);
    }
    const std::vector<int>& ids = kEntriesOfMode[size_t(mode)];
    for (int i = 0; i < kEntriesOfSubmenu[size_t(list)]; ++i) {
        const std::string banner = Instance(kActiveList[size_t(list)], "uip0006_SubMenuList01", i);
        ui.HideInstance(banner, i >= int(ids.size()));
        if (i < int(ids.size())) {
            ui.Play(banner, UiScreen::ClipName(kSwitch), false, 1);
            ui.SetClipTime(banner, 1, float(ids[size_t(i)]));
        }
    }
    (void)entry;  // shortcut: the focused entry's highlight (clips 4/5 "focus"/"unfocus") is not played yet
}

}

int RunTopMenu(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames) {
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("top menu: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/10_top_menu", lang, &error, &text)) {
        LogError("top menu: {}", error);
        return -1;
    }
    ui.HideNode("new00", true);  // nothing is new or complete without a save (0x1BD2D0 shows them per save flags)
    ui.HideNode("complete00", true);
    int mode = 0, entry = 0;  // shortcut: start mode not traced; 0 = Play
    Apply(ui, mode, entry);
    SDL_SetWindowTitle(window, "SIF port - top menu");
    int chosen = -1;
    bool running = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; running && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const std::vector<int>& ids = kEntriesOfMode[size_t(mode)];
            switch (e.key.scancode) {
                case SDL_SCANCODE_ESCAPE: fade.Leave(ui); break;
                case SDL_SCANCODE_LEFT: mode = (mode + kModes - 1) % kModes, entry = 0; break;
                case SDL_SCANCODE_RIGHT: mode = (mode + 1) % kModes, entry = 0; break;
                case SDL_SCANCODE_UP: entry = ids.empty() ? 0 : (entry + int(ids.size()) - 1) % int(ids.size()); break;
                case SDL_SCANCODE_DOWN: entry = ids.empty() ? 0 : (entry + 1) % int(ids.size()); break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    if (!ids.empty()) chosen = ids[size_t(entry)], fade.Leave(ui);
                    break;
                default: break;
            }
            Apply(ui, mode, entry);
            LogInfo("top menu: mode {}, entry {}", mode, ids.empty() ? -1 : ids[size_t(entry % std::max<size_t>(1, ids.size()))]);
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, kRoot, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    LogInfo("top menu: {}", chosen >= 0 ? "entry " + std::to_string(chosen) : std::string("closed"));
    return chosen;
}

}
