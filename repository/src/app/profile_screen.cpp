#include "app/profile_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/formats/png.h"
#include "engine/platform/os.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/profile/profile.h"
#include "game/save/save.h"
#include "game/text/messages.h"

namespace sif {
namespace {

constexpr const char* kRoot = "uil0050_ProcEditLayout";
// The list window's groups (uil0052_ProcEditWin00): instance name and slot count. Tabs 0-2 use the title, name and
// badge groups; the others belong to the profile-card editor (shortcut: not built) and stay hidden.
struct Group {
    const char* name;
    int slots;
};
constexpr std::array<Group, 6> kGroups = {{{"uil0060_ProfileFrameGroup00", 20},
                                           {"uil0061_ProfilePhotoGroup00", 28},
                                           {"uil0062_ProfileTitleGroup00", 48},
                                           {"uil0063_ProfileNameGroup00", 30},
                                           {"uil0064_ProfileBadgeGroup00", 48},
                                           {"uil0065_ProfileskillGroup00", 30}}};
constexpr int kTabs = 3;
constexpr std::array<int, kTabs> kGroupOfTab = {2, 3, 4};  // Accolade, Name Plate, Badge
// col_pro_select_text00_T: 4 x 2 cells (Card Frame, Photo, Accolade, Name Plate / Badge, Skill Card, Player's Name)
constexpr std::array<std::array<float, 2>, 4> kTabLabel = {{{0.5f, 0.0f}, {0.75f, 0.0f}, {0.0f, 0.5f}, {0.5f, 0.5f}}};
// The groups' slot grids (from uil0052's node positions): accolades and badges 4 x 12 (rows 76 apart, first at y 399),
// name plates 3 x 10 (rows 92 apart, first at 415). The game scrolls a group inside the window; the port shows pages of
// the rows that fit, moved down by kShift so the first row sits at the window's top (shortcut: measured on a capture,
// no smooth scroll).
constexpr std::array<int, kTabs> kColumns = {4, 3, 4};
constexpr std::array<int, kTabs> kRows = {6, 5, 6};
constexpr std::array<float, kTabs> kShift = {-54.0f, -70.0f, -54.0f};

std::string Slot(int group, int k) {
    char s[96];
    std::snprintf(s, sizeof(s), "%s/uil0052_ProcEditWin00/%s_%02d", kRoot, kGroups[size_t(group)].name, k);
    return s;
}

}

bool RunProfile(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, Nameplate& plate,
                long max_frames) {
    route::SetScreen("profile");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("profile: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/40_collection", lang, &error, &text)) {
        LogError("profile: {}", error);
        return true;
    }
    const std::string root = kRoot, preview = root + "/uil0033_PlatePreview00";
    for (const char* other : {"uil0051_ProcEditMenu00", "uil0053_ProcPreview00", "uil0054_PhotoAdjustWin00"}) {
        ui.HideInstance(root + "/" + other, true);
    }
    // the page title (col_pro_title_text00_T rows of 1/2: Edit Profile Card, Change Accolade or Name Plate)
    ui.SetMaterialUv(root + "/uip0030_ProcEditTop00", "pop_title00_M", 0.0f, 0.5f);
    ui.SetInstanceText(preview + "/uip0084_PlatePreviewText00", 0, "The chosen Nameplate, Badge and Accolade show in the top menu.");
    ui.SetInstanceText(preview + "/uip0083_PlatePreview00_00", 0, "Nameplate / Badge");
    ui.SetInstanceText(preview + "/uip0083_PlatePreview00_01", 0, "Accolade");
    // shortcut: Player's Name (text entry) is not built; its tab is shown but not selectable
    int tab = 0, cursor = 0;
    const auto list_of = [&](int t) -> const std::vector<NameplateItem>& {
        return t == 0 ? plate.accolades : t == 1 ? plate.name_plates : plate.badges;
    };
    const auto chosen_of = [&](int t) -> size_t& { return t == 0 ? plate.accolade : t == 1 ? plate.name_plate : plate.badge; };
    const auto show = [&] {
        const std::string menu = root + "/uil0031_PlateChangeMenu00";
        for (int k = 0; k < 4; ++k) {
            char icon[96];
            std::snprintf(icon, sizeof(icon), "%s/uip0031_EditMenuIcon00_%02d", menu.c_str(), k);
            ui.SetMaterialUv(icon, "edit_menu_title00_M", kTabLabel[size_t(k)][0], kTabLabel[size_t(k)][1]);
            ui.HideNode(icon, "edit_menu_button00_on", k != tab);
            ui.HideNode(icon, "edit_menu_button00_eff", k != tab);
            ui.HideNode(icon, "edit_menu_button00_off", k == tab);
        }
        ui.HideNode(menu + "/uip0032_EditMenuIcon01", "edit_menu_done00_on", true);
        ui.HideNode(menu + "/uip0032_EditMenuIcon01", "edit_menu_done00_eff", true);
        const std::vector<NameplateItem>& list = list_of(tab);
        const int group = kGroupOfTab[size_t(tab)], slots = kColumns[size_t(tab)] * kRows[size_t(tab)];
        const int page = cursor / slots;
        for (int g = 0; g < int(kGroups.size()); ++g) {
            for (int k = 0; k < kGroups[size_t(g)].slots; ++k) {
                const int i = page * slots + k;
                ui.HideInstance(Slot(g, k), g != group || k >= slots || i >= int(list.size()));
                ui.MoveInstance(Slot(g, k), 0.0f, g == group ? kShift[size_t(tab)] : 0.0f);
            }
        }
        // the window's "○○○がありません。" (none of ○○○) placeholder: only for an empty list
        ui.SetInstanceText(root + "/uil0052_ProcEditWin00/uip0033_ProcEditWinBG00", 0, list.empty() ? "-" : "");
        for (int k = 0; k < slots; ++k) {
            const int i = page * slots + k;
            if (i >= int(list.size())) break;
            const NameplateItem& item = list[size_t(i)];
            const std::string slot = Slot(group, k);
            ui.HideInstance(slot + "/uip0017_IconBack01", i != cursor);                    // focus
            ui.HideInstance(slot + "/uip0004_StarMark00", size_t(i) != chosen_of(tab));    // chosen
            if (tab == 0) {
                const std::string part = slot + "/ctg03_uip0004_Title00";
                ui.SetMaterialTexture(part, "title_plate00_M", TitlePlatePath(item.plate));
                ui.SetInstanceText(part, 0, item.name);
                ui.SetInstanceText(part, 1, "");
            } else if (tab == 1) {
                const std::string part = slot + "/ctg03_uip0005_NamePlate00";
                ui.SetMaterialTexture(part, "name_plate00_M", NamePlatePath(item.id));
                ui.SetInstanceText(part, 0, plate.player);
                ui.SetInstanceText(part, 1, "");
            } else {
                const std::string part = slot + "/ctg03_uip0006_Badge00";
                ui.SetMaterialTexture(part, "badge00_M", BadgePlatePath(item.plate));
                ui.SetInstanceText(part, 0, item.name);
            }
        }
        ShowNameplate(ui, plate, preview + "/ctg03_uip0004_Title00", preview + "/ctg03_uil0005_NameSet00");
    };
    cursor = int(chosen_of(tab));
    show();
    SDL_SetWindowTitle(window, "SIF port - profile card");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const int n = int(list_of(tab).size()), columns = kColumns[size_t(tab)];
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT: cursor = std::max(0, cursor - 1); break;
                case SDL_SCANCODE_RIGHT: cursor = std::min(std::max(0, n - 1), cursor + 1); break;
                case SDL_SCANCODE_UP: cursor = cursor >= columns ? cursor - columns : cursor; break;
                case SDL_SCANCODE_DOWN: cursor = cursor + columns < n ? cursor + columns : cursor; break;
                case SDL_SCANCODE_PAGEUP:
                case SDL_SCANCODE_PAGEDOWN:
                    tab = (tab + (e.key.scancode == SDL_SCANCODE_PAGEUP ? kTabs - 1 : 1)) % kTabs;
                    cursor = int(chosen_of(tab));
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    if (!e.key.repeat && cursor < n) {
                        chosen_of(tab) = size_t(cursor);
                        LogInfo("profile: tab {} item {} ({})", tab, list_of(tab)[size_t(cursor)].id, list_of(tab)[size_t(cursor)].name);
                    }
                    break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    if (!e.key.repeat) fade.Leave(ui);
                    break;
                default: break;
            }
            show();
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, root, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    return open;
}

int RunProfileMenu(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                   const ProfileMenuSetup& setup, long max_frames) {
    route::SetScreen("profile_menu");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("profile menu: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/40_collection", lang, &error, &text)) {
        LogError("profile menu: {}", error);
        return 0;
    }
    const std::string root = "uil0090_ProcMenuLayout00", buttons = root + "/uil0091_MenuButGroup00";
    const Messages* m = setup.texts.messages;
    const auto msg = [&](int id, const char* fallback) {
        const std::string s = m ? m->Get(id) : std::string();
        return s.empty() ? std::string(fallback) : s;
    };
    ui.SetInstanceText(root + "/uip0039_col_pro_top_title_plate00_00", 0, msg(680, "Current Profile Card"));
    ui.SetInstanceText(root + "/uip0039_col_pro_top_title_plate00_01", 0, msg(681, "Current Accolade / Nameplate"));
    ui.SetInstanceText(root + "/uip0036_ProcMenuPlate00", 0, msg(706, "Menu"));
    ui.SetInstanceText(buttons + "/uip0037_ProcNowMemIcon00", 0, msg(683, "Card-setting member"));
    if (setup.plate) ShowNameplate(ui, *setup.plate, root + "/ctg03_uip0004_Title00", root + "/ctg03_uil0005_NameSet00");

    // the current card: drawn by the port from its parts, in place of the game's saved JPEG (uip0038's card nodes)
    const bool has_card = setup.save && setup.save->has_card;
    Texture photo;
    ProfileCardArt art;
    if (has_card) {
        const save::ProfileCard& card = setup.save->card;
        art = MakeProfileCardArt(card, setup.texts);
        std::vector<uint8_t> rgba;
        uint32_t w = 0, h = 0;
        if (png::Read(setup.data_dir / "profile_card.png", rgba, w, h, &error)) {
            MakeCardPhoto(renderer, rgba, w, h, card.landscape, card.crop, photo);
        } else {
            LogWarn("profile menu: {}", error);
        }
        ApplyProfileCard(ui, "uip0019_ProfileCardCustom00", art, photo.gpu ? &photo : nullptr);
    }
    const std::string bg = root + "/uip0038_ProcPreviewBG01";
    ui.HideNode(bg, "card_bg_Noimg", has_card);
    ui.HideNode(bg, "card_vertical00", true);
    ui.HideNode(bg, "card_horizontal00", true);
    // shortcut: the dissolve backgrounds behind the game's card JPEG are not drawn (they need the JPEG)
    ui.HideNode(bg, "card_bg_vertical00", true);
    ui.HideNode(bg, "card_bg_horizontal00", true);
    // the button group's parts carry their own screen positions, and uil0090 places the group at (1336, -673) too:
    // drawn as placed they are off screen, so the group is moved back (the game's placement rule is not traced)
    ui.MoveInstance(buttons, -1336.0f, 673.0f);
    // the group background and tab follow the card-setting member (µ's or Aqours / Saint Snow)
    const bool aqours = setup.member > 9;
    ui.HideInstance(root + "/common_uil0101_ComBGGroup_Member01/common_uip0400_ComTopBG_Ms00", aqours);
    ui.HideInstance(root + "/common_uil0101_ComBGGroup_Member01/common_uip0410_ComTopBG_Aqours00", !aqours);
    ui.HideNode(root + "/common_uip0008_GroupTab00", "select_group00", aqours);
    ui.HideNode(root + "/common_uip0008_GroupTab00", "select_group01", !aqours);

    // the buttons: each part holds all three kinds (plate_change, edit, user); the focused one shows its "on" form.
    // shortcut: a button that cannot be chosen (Create without a photo, Everyone's Profile Cards) shows its "off" form;
    // the game greys it
    const bool can_create = setup.save && profile::CanCreateCard(*setup.save);
    const std::array<bool, 3> enabled = {true, can_create, false};
    static constexpr const char* kKinds[3] = {"plate_change00", "edit00", "user00"};
    int focus = 0;
    const auto show = [&] {
        for (int k = 0; k < 3; ++k) {
            char b[160];
            std::snprintf(b, sizeof(b), "%s/uip0090_ProcMenuBut00_%02d", buttons.c_str(), k);
            ui.HideNode(b, "select00_on", k != focus);
            ui.HideNode(b, "select00_off", k == focus);
            for (int kind = 0; kind < 3; ++kind) {
                ui.HideNode(b, std::string(kKinds[kind]) + "_on", kind != k);
                ui.HideNode(b, std::string(kKinds[kind]) + "_off", kind != k);
            }
        }
    };
    show();
    LogInfo("profile menu: {}, create {}", has_card ? "a card" : "no card", can_create ? "open" : "greyed");
    SDL_SetWindowTitle(window, "SIF port - profile card");
    int result = 0;
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false, result = -1;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            switch (e.key.scancode) {
                case SDL_SCANCODE_UP:
                    for (int k = focus - 1; k >= 0; --k) {
                        if (enabled[size_t(k)]) {
                            focus = k;
                            break;
                        }
                    }
                    break;
                case SDL_SCANCODE_DOWN:
                    for (int k = focus + 1; k < 3; ++k) {
                        if (enabled[size_t(k)]) {
                            focus = k;
                            break;
                        }
                    }
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    result = focus + 1;
                    LogInfo("profile menu: button {}", focus);
                    fade.Leave(ui);
                    break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE: fade.Leave(ui); break;
                default: break;
            }
            show();
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, root, 1.0f);
        // the card in the preview box (uip0038 at (375, -577), about 0.76 of the card's own size, as in shad2/p09)
        if (has_card) ui.DrawPart(renderer, "uip0019_ProfileCardCustom00", 375.0f, -577.0f, 1.0f, 0.76f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    renderer.Release(photo);
    ui.Release(renderer);
    return open ? result : -1;
}

}
