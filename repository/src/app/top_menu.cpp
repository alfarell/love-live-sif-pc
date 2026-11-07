#include "app/top_menu.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <random>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "engine/audio/audio_output.h"
#include "engine/audio/sound_bank.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/talk/talk.h"

namespace sif {
namespace {

constexpr int kModes = 6;   // the wheel's modes (0x1BD2D0: indices taken mod 6)
constexpr int kIcons = 8;   // placed wheel icons (TopMenu +0x388 ... +0x3C0)
constexpr int kSwitch = 20; // ui_animation.bin: clip 20 "switch", posed at the label id on layer 1
// DAT_005F42F0: the submenu list shown for each mode; DAT_005F3F54: entries per submenu list
constexpr std::array<int, kModes> kSubmenuOfMode = {1, 3, 3, 1, 0, 2};
constexpr std::array<int, 4> kEntriesOfSubmenu = {5, 4, 3, 2};
// The entry ids (label cells of top_mode_sub00_on_T, 4 per row: 0-3 Play items, 5 Create/Change Costumes, 7 Costume
// Materials, 8 Songs, 9 Costumes, 10 Game Settings, 12 Control Settings, 13 Sound, 14 Credits, 15 Profile Card,
// 16 Member Card, 17 Skill Card, 18 Photo Gallery, 19 AC Archive, 20 Task List, 21 Ongoing Events, 22 Rankings).
// From the pointer table at 0xC085A0, which the first reading took one slot late (it gave each mode the previous
// mode's ids); corrected by the label atlas and the submenu sizes (2, 2, 4, 5, 3), M7.
const std::array<std::vector<int>, kModes> kEntriesOfMode = {{
    {0, 1, 2, 3}, {5, 7}, {8, 9}, {10, 12, 13, 14}, {15, 16, 17, 18, 19}, {20, 21, 22}}};
// The submenu layouts per list index, matched by their banner counts (5, 4, 3, 2): idle and active ("_act") forms
constexpr std::array<const char*, 4> kIdleList = {"uil0003_SubMenuList00", "uil0010_SubMenuList03",
                                                  "uil0004_SubMenuList01", "uil0005_SubMenuList02"};
constexpr std::array<const char*, 4> kActiveList = {"uil0006_SubMenuList00_act", "uil0011_SubMenuList03_act",
                                                    "uil0007_SubMenuList01_act", "uil0008_SubMenuList02_act"};
constexpr const char* kRoot = "uil0001_TopMenu00";
// shortcut: the navigator is always Honoka (member 1); the game's navigator choice (save / Members screen) is not built
constexpr int kNavigator = 1;

// The navigator's voice line for talk `id`; logs and returns false when it has none.
bool Say(const Vfs& vfs, const cri::CueSheet& voice, AudioOutput& audio, int id) {
    const std::string cue = talk::VoiceCue(vfs, kNavigator, id);
    cri::Pcm pcm;
    std::string error;
    if (cue.empty() || !cri::LoadCue(vfs, voice, cue, pcm, &error)) {
        LogWarn("talk {}: no voice ({}{})", id, cue, error.empty() ? "" : ": " + error);
        return false;
    }
    LogInfo("talk {}: {}", id, cue);
    audio.Play(std::move(pcm));
    return true;
}

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

void ApplyNameplate(UiScreen& ui, const Nameplate& p) {
    const std::string top = std::string(kRoot) + "/uil0009_PlayerDataTop00";
    ShowNameplate(ui, p, top + "/ctg03_uip0004_Title00", top + "/ctg03_uil0005_NameSet00");
    // the header's date (uip0008_PlayerData00 box 0) and boost items (box 1; shortcut: not built, left empty)
    const std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &t);
    char date[64];
    std::strftime(date, sizeof(date), "%Y/%m/%d  %H:%M", &local);
    ui.SetInstanceText(top + "/uip0008_PlayerData00", 0, date);
    ui.SetInstanceText(top + "/uip0008_PlayerData00", 1, "");
}

}

std::string TitlePlatePath(int plate) {
    char s[96];
    std::snprintf(s, sizeof(s), "ui/ui_honorific_title.pac/texture.pac/title_plate_%04d", plate);
    return s;
}

std::string NamePlatePath(int id) {  // id PPPSSSS -> name_plate_PPP_SSSS
    char s[96];
    std::snprintf(s, sizeof(s), "ui/ui_name_plate.pac/texture.pac/name_plate_%03d_%04d", id / 10000, id % 10000);
    return s;
}

std::string BadgePlatePath(int category) {
    char s[96];
    std::snprintf(s, sizeof(s), "ui/ui_badge.pac/texture.pac/badge_plate_%03d_0", category);
    return s;
}

void ShowNameplate(UiScreen& ui, const Nameplate& p, const std::string& title, const std::string& set) {
    const std::string name = set + "/uip0005_NamePlate00", badge = set + "/uip0006_Badge00";
    const NameplateItem none;
    const NameplateItem& a = p.accolade < p.accolades.size() ? p.accolades[p.accolade] : none;
    const NameplateItem& b = p.badge < p.badges.size() ? p.badges[p.badge] : none;
    if (a.plate && !ui.SetMaterialTexture(title, "title_plate00_M", TitlePlatePath(a.plate))) LogWarn("nameplate: no {}", a.plate);
    if (p.name_plate < p.name_plates.size()) {
        const int id = p.name_plates[p.name_plate].id;
        if (!ui.SetMaterialTexture(name, "name_plate00_M", NamePlatePath(id))) LogWarn("nameplate: no name plate {}", id);
    }
    if (b.plate && !ui.SetMaterialTexture(badge, "badge00_M", BadgePlatePath(b.plate))) LogWarn("nameplate: no badge {}", b.plate);
    // text boxes (probed): title 0 accolade name, name plate 0 player name, badge 0 badge name; box 1 of the title and
    // name plates sits over box 0's left part (shortcut: its use is not known, left empty)
    ui.SetInstanceText(title, 0, a.name);
    ui.SetInstanceText(title, 1, "");
    ui.SetInstanceText(name, 0, p.player);
    ui.SetInstanceText(name, 1, "");
    ui.SetInstanceText(badge, 0, b.name);
}

int RunTopMenu(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
               int* chosen_mode, AudioOutput* audio, const Nameplate* plate) {
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
    if (plate) ApplyNameplate(ui, *plate);
    SDL_SetWindowTitle(window, "SIF port - top menu");
    cri::CueSheet voice;
    std::vector<int> plain;  // type-0 talks (any time), for T
    if (audio && cri::LoadCueSheet(vfs, "voice", voice, &error)) {
        const auto rules = talk::LoadRules(vfs);
        for (const talk::Rule& r : rules) {
            if (r.type == 0 && !talk::VoiceCue(vfs, kNavigator, r.id).empty()) plain.push_back(r.id);
        }
        const std::time_t t = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &t);
        const int greeting = talk::GreetingFor(rules, local.tm_hour);
        if (greeting >= 0) Say(vfs, voice, *audio, greeting);
    } else if (audio) {
        LogWarn("top menu: voice: {}", error);
        audio = nullptr;
    }
    std::mt19937 random{std::random_device{}()};
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
                case SDL_SCANCODE_T:
                    if (audio && !plain.empty()) Say(vfs, voice, *audio, plain[random() % plain.size()]);
                    continue;
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
    if (chosen_mode) *chosen_mode = mode;
    return chosen;
}

}
