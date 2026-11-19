#include "app/top_menu.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/member_popup.h"
#include "app/navigator.h"
#include "app/route.h"
#include "engine/audio/audio_output.h"
#include "engine/audio/sound_bank.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/event/event.h"
#include "game/member/member.h"
#include "game/save/save.h"
#include "game/talk/talk.h"

namespace sif {
namespace {

constexpr int kModes = 6;   // the wheel's modes (0x1BD2D0: indices taken mod 6)
constexpr int kIcons = 8;   // placed wheel icons (TopMenu +0x388 ... +0x3C0)
constexpr int kSwitch = 20; // ui_animation.bin: clip 20 "switch", posed at the label id on layer 1
constexpr int kActive = 13, kUnfocus = 5;  // ui_animation.bin: clips 13 "active", 5 "unfocus"
constexpr int kCurrentSlot = 3;            // the wheel icon showing the current mode
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
constexpr const char* kRoomPart = "uip0001_RoomBG00";  // her room, and a quad for the game's off-screen member picture

// The navigator's voice line for talk `id`; logs and returns false when it has none.
bool Say(const Vfs& vfs, const cri::CueSheet& voice, AudioOutput& audio, int member, int id) {
    const std::string cue = talk::VoiceCue(vfs, member, id);
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
// `focus_changed`: replay the banners' focus clips (only then, so a held focus does not restart its animation)
// `mode_changed`: replay the wheel icons' clips
void Apply(UiScreen& ui, int mode, int entry, bool focus_changed = true, bool mode_changed = true) {
    for (int i = 0; i < kIcons; ++i) {  // 0x1B94C0: icon i shows label ((position + 3) mod 6) + i
        const std::string icon = Instance("uil0002_MenuList00", "uip0004_MenuIcon00", i);
        ui.Play(icon, UiScreen::ClipName(kSwitch), false, 1);
        ui.SetClipTime(icon, 1, float((mode + 3) % kModes + i));
        // the current mode's icon (slot 3: label (mode + 3) mod 6 + 3 = mode) plays clip 13 "active" (the plain circle
        // and the large name banner); the others clip 5 "unfocus" (the icon with its small tilted plate). Without a clip
        // every node of the part draws at once (M9 F1)
        if (mode_changed) ui.Play(icon, UiScreen::ClipName(i == kCurrentSlot ? kActive : kUnfocus), false, 0);
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
            // the focused banner plays clip 13 "active" (shows the green menu_select01_on layer, with a small bounce);
            // the others show their white menu_select01_off layer. Clip 14 "inactive" is not used: it fades the green
            // layer's colour to 0, which this renderer draws as black, not as a fade (M9 F1)
            if (focus_changed) ui.Play(banner, i == entry ? UiScreen::ClipName(kActive) : std::string(), false, 0);
            ui.HideNode(banner, "menu_select01_on", i != entry);
        }
    }
}

void ApplyNameplate(UiScreen& ui, const Nameplate& p, const save::SaveData* save) {
    const std::string top = std::string(kRoot) + "/uil0009_PlayerDataTop00";
    ShowNameplate(ui, p, top + "/ctg03_uip0004_Title00", top + "/ctg03_uil0005_NameSet00");
    // the header's date (uip0008_PlayerData00 box 0) and the Wai-Wai! Boosters held (box 1: "Wai-Wai! Booster 5", at 9
    // "Wai-Wai! Booster 9 MAX"; messages 698/699, M14 F5. shortcut: in English, as the date; the menu loads no messages)
    const std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &t);
    char date[64];
    std::strftime(date, sizeof(date), "%Y/%m/%d  %H:%M", &local);
    ui.SetInstanceText(top + "/uip0008_PlayerData00", 0, date);
    std::string boost;
    if (save) {
        char s[16];
        std::snprintf(s, sizeof(s), "%d", save->boosters);
        boost = save->boosters >= event::kMaxBoosters ? "Wai-Wai! Booster 9 MAX" : "Wai-Wai! Booster " + std::string(s);
    }
    ui.SetInstanceText(top + "/uip0008_PlayerData00", 1, boost);
}

// The day's first visit gives the login bonus (eboot FUN_001b94c0; M14 F1 §3): the next of 12 days, a Wai-Wai!
// Booster (cap 9) or 10 of a key item by the group mode. shortcut: its pop-up is not shown, only logged
void LoginBonus(save::SaveData& s) {
    const event::Date d = event::Today();
    const int today = d.year * 10000 + d.month * 100 + d.day;
    if (s.login_date == today) return;
    const int day = std::clamp(s.login_day, 0, 11);
    const int item = event::kLoginBonus[size_t(day)][member::Group(save::Current(s)) == 0 ? 0 : 1];
    if (item == 0) {
        s.boosters = std::min(s.boosters + 1, event::kMaxBoosters);
    } else {
        int& have = s.owned[{2, item}];  // a costume material (mission reward type 2)
        have = std::min(have + event::kLoginItemAmount, 9999);
    }
    s.login_date = today, s.login_day = (day + 1) % 12, ++s.logins;
    LogInfo("login bonus: day {}: {}, {} boosters held", day + 1,
            item ? "10 of item " + std::to_string(item) : std::string("a Wai-Wai! Booster"), s.boosters);
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
               int* chosen_mode, AudioOutput* audio, const Nameplate* plate, save::SaveData* save,
               const event::Data* events) {
    route::SetScreen("menu");
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
    int shown_mode = mode, shown_entry = entry;
    if (save) LoginBonus(*save);
    if (plate) ApplyNameplate(ui, *plate, save);
    SDL_SetWindowTitle(window, "SIF port - top menu");
    // the room (behind her) and the rest of the menu (in front) are drawn in two passes around the 3D member. The game
    // renders her off-screen and shows the picture on the room part's model_character00 quad; the port hides that quad
    std::map<std::string, float> room_only, without_room;
    std::string room_instance;
    for (const auto& [node, target] : ui.Placements(kRoot)) {
        const bool room = target.find(kRoomPart) != std::string::npos;
        room_only[node] = room ? 1.0f : 0.0f;
        without_room[node] = room ? 0.0f : 1.0f;
        if (!room) continue;
        room_instance = std::string(kRoot) + "/" + node;
        ui.HideNode(room_instance, "model_character00", true);
        ui.HideNode(room_instance, "room_bg001", true);  // shortcut: when the game shows the second room is not traced
    }
    if (room_instance.empty()) LogWarn("top menu: no {} in {}", kRoomPart, kRoot);
    cri::CueSheet voice;
    std::vector<talk::Rule> rules;
    if (audio && cri::LoadCueSheet(vfs, "voice", voice, &error)) {
        rules = talk::LoadRules(vfs);
    } else if (audio) {
        LogWarn("top menu: voice: {}", error);
        audio = nullptr;
    }
    save::SaveData none;
    save::SaveData& s = save ? *save : none;
    Navigator nav;
    std::vector<int> plain;  // her type-0 talks (any time), for T
    // the current member (save: mode and the mode's chosen member) in her room and costume, with the header of her
    // group, greeting by the hour
    const auto show = [&] {
        const int who = save::Current(s);
        nav.Release(renderer);
        const auto w = s.wear.find(who);
        if (!nav.Load(renderer, vfs, who, w == s.wear.end() ? -1 : w->second, s.special.contains(who), &error)) {
            LogWarn("top menu: navigator {}: {}", who, error);
        }
        const std::string folder = std::string("ui2/10_top_menu.pac/") + kRoomPart + ".pac/texture.pac/";
        if (!room_instance.empty() &&
            !ui.SetMaterialTexture(room_instance, "_charaTex_bg_room00", folder + member::RoomTexture(who) + ".btx")) {
            LogWarn("top menu: no room for member {}", who);
        }
        // "Now Open!": a running birthday event of her mode shows its banner (eboot FUN_0000f3d0: top_event_bd00_00_MM;
        // M14 F6). shortcut: otherwise the panel keeps the layout's own picture (the game shows the monthly ranking)
        for (const int m : member::kAll) {
            const event::Birthday* b = events ? event::Find(*events, m) : nullptr;
            if (!b || member::Mode(m) != member::Mode(who) || !event::Active(*b, event::Today())) continue;
            char path[96];
            std::snprintf(path, sizeof(path), "ui/ui_bd_banner_top.pac/texture.pac/top_event_bd00_00_%02d", m);
            ui.SetMaterialTexture(std::string(kRoot) + "/uip0002_MenuBG00", "event_info00_M1", path);
            ui.SetMaterialUv(std::string(kRoot) + "/uip0002_MenuBG00", "event_info00_M1", 0, 0);
            LogInfo("top menu: birthday event of member {} running", m);
            break;
        }
        const int group = member::Group(who);  // the header's logo and plate (eboot 0x1BD2D0: logo_*, player_data_bg00_*)
        for (int g = 0; g < 3; ++g) {
            static constexpr const char* kLogo[] = {"logo_ms00", "logo_aq00", "logo_ss00"};
            static constexpr const char* kPlate[] = {"player_data_bg00_ms", "player_data_bg00_aq", "player_data_bg00_ss"};
            ui.HideNode(kLogo[g], g != group);
            ui.HideNode(kPlate[g], g != group);
        }
        plain.clear();
        for (const talk::Rule& r : rules) {
            if (r.type == 0 && !talk::VoiceCue(vfs, who, r.id).empty()) plain.push_back(r.id);
        }
        if (!audio) return;
        const std::time_t t = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &t);
        const int greeting = talk::GreetingFor(rules, local.tm_hour);
        if (greeting >= 0 && Say(vfs, voice, *audio, who, greeting)) nav.Talk();
    };
    show();
    MemberPopup popup;
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
            if (popup.IsOpen()) {  // R2's member popup takes the keys while open
                if (popup.Key(e.key.scancode) == MemberPopup::Result::Chosen) {
                    const int m = popup.Chosen();
                    s.mode = member::Mode(m);
                    s.navigator[s.mode] = m;
                    show();  // shortcut: the game's loading screen between members is not shown
                }
                continue;
            }
            const std::vector<int>& ids = kEntriesOfMode[size_t(mode)];
            switch (e.key.scancode) {
                case SDL_SCANCODE_ESCAPE: fade.Leave(ui); break;
                case SDL_SCANCODE_HOME:  // L2 "Toggle Mode [µ's/Aqours]": the other mode's member at once
                    s.mode = 1 - s.mode;
                    LogInfo("top menu: mode {} (L2), member {}", s.mode == 0 ? "µ's" : "Aqours", save::Current(s));
                    show();
                    continue;
                case SDL_SCANCODE_END:  // R2 "Switch Members"
                    popup.Open(renderer, vfs, lang, s.mode, save::Current(s));
                    continue;
                case SDL_SCANCODE_T:
                    LogInfo("top menu: another line (T)");
                    if (audio && !plain.empty() && Say(vfs, voice, *audio, nav.Member(), plain[random() % plain.size()])) nav.Talk();
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
            Apply(ui, mode, entry, mode != shown_mode || entry != shown_entry, mode != shown_mode);
            shown_mode = mode, shown_entry = entry;
            LogInfo("top menu: mode {}, entry {}", mode, ids.empty() ? -1 : ids[size_t(entry % std::max<size_t>(1, ids.size()))]);
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        nav.Update(double(now - last) / 1e9);
        if (popup.IsOpen()) popup.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        if (!room_only.empty()) {
            ui.DrawLayout(renderer, kRoot, 1.0f, room_only);
            renderer.Use3D();
            nav.Draw(renderer, Navigator::MenuCamera(float(renderer.Width()) / float(std::max(1, renderer.Height()))));
            ui.DrawLayout(renderer, kRoot, 1.0f, without_room);
        } else {
            ui.DrawLayout(renderer, kRoot, 1.0f);
        }
        popup.Draw(renderer);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    popup.Release(renderer);
    nav.Release(renderer);
    ui.Release(renderer);
    LogInfo("top menu: {}", chosen >= 0 ? "entry " + std::to_string(chosen) : std::string("closed"));
    if (chosen_mode) *chosen_mode = mode;
    return chosen;
}

}
