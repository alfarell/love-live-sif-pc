#include "app/costume_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/member_popup.h"
#include "app/navigator.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/fs/vfs.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/member/member.h"
#include "game/text/messages.h"

namespace sif {
namespace {

// The 34 slots of uil0030_MaterialListLayout00, as the game fills them (seen on its own screen in shadPS4, M7):
// 6 key items, 16 threads (4 common, 12 of the group), 4 common cloths, 8 of the group's cloths.
std::vector<int> MaterialSlots(bool aqours) {
    std::vector<int> ids;
    const auto range = [&](int first, int last) {
        for (int i = first; i <= last; ++i) ids.push_back(i);
    };
    if (aqours) {
        range(55, 60), range(1, 4), range(35, 46), range(5, 8), range(47, 54);
    } else {
        range(29, 34), range(1, 4), range(9, 20), range(5, 8), range(21, 28);
    }
    return ids;
}

}

bool RunMaterials(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const costume::Data& data,
                  const save::SaveData& save, long max_frames) {
    route::SetScreen("materials");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("materials: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/30_costume", lang, &error, &text)) {
        LogError("materials: {}", error);
        return true;
    }
    const std::string root = "uil0030_MaterialListLayout00";
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    bool aqours = false;
    const auto show = [&] {
        ui.HideInstance(root + "/common_uil0100_ComBGGroup_Ms00", aqours);
        ui.HideInstance(root + "/common_uil0110_ComBGGroup_Aqours00", !aqours);
        const std::vector<int> ids = MaterialSlots(aqours);
        for (size_t k = 0; k < ids.size(); ++k) {
            char set[64];
            std::snprintf(set, sizeof(set), "/uil0031_MaterialIconSet00_%02zu", k);
            const std::string icon = root + set + "/common_uip0021_ItemIcon00", info = root + set + "/uip0080_CosMaterialInfo00";
            const costume::Item* item = costume::FindItem(data, ids[k]);
            ui.HideInstance(root + set, !item);
            if (!item) continue;
            if (!ui.SetMaterialTexture(icon, "item_icon00_M", costume::IconPath(*item))) LogWarn("materials: no {}", costume::IconPath(*item));
            ui.HideNode(icon, "icon_select00_eff", true);
            ui.SetInstanceText(info, 0, item->name[size_t(language)]);
            ui.SetInstanceText(info, 1, "\xC3\x97" + std::to_string(costume::ItemCount(save, item->id)));  // "×N"
        }
    };
    show();
    SDL_SetWindowTitle(window, "SIF port - costume materials");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT:
                case SDL_SCANCODE_RIGHT: aqours = !aqours, show(); break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE: fade.Leave(ui); break;
                default: break;
            }
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

}

namespace sif {
namespace {

// The costume's state: 0 obtained, 1 ready to make, 2 not enough materials, 3 add-on only (no recipe). Its label in
// the info panel and the pop-up (status_label00-03, com_select_purchased_text00-03: Ready to Create, Obtained,
// Insufficient Materials, "These costumes are download only.") is kLabelOf[state].
constexpr int kLabelOf[4] = {1, 0, 2, 3};
int StateOf(const costume::Data& data, const dlc::Ownership* addons, const save::SaveData& save, int dress) {
    if (costume::Owns(data, save, addons, dress)) return 0;
    const costume::Recipe* r = costume::FindRecipe(data, dress);
    if (!r || !r->Craftable()) return 3;
    return costume::CanCraft(*r, save) ? 1 : 2;
}

std::string Two(const char* pattern, int k) {
    char s[96];
    std::snprintf(s, sizeof(s), pattern, k);
    return s;
}

}

bool RunCostumes(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const costume::Data& data,
                 const std::vector<std::pair<int, std::string>>& names, const dlc::Ownership* addons, save::SaveData& save,
                 long max_frames) {
    route::SetScreen("costumes");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("costumes: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/30_costume", lang, &error, &text)) {
        LogError("costumes: {}", error);
        return true;
    }
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    const std::string root = "uil0001_CosLayout00", create = root + "/uil0002_CosCreate00";
    const std::string pop = create + "/uil0010_PopCreate00", pop_part = pop + "/uip0030_PopCreate00";
    static constexpr const char* kGrids[3] = {"/uil0003_CosMenu00_Ms", "/uil0004_CosMenu00_Aq", "/uil0005_CosMenu00_Ss"};
    constexpr int kSlots = 33, kColumns = 6;
    // shortcut: the member-card pictures (the grid and preview show the costume thumbnail instead), the material
    // gauges, the button hints and the song list are not built
    ui.HideInstance(root + "/uil0020_CosComplete00", true);
    // the member (M13 N6): the game draws her off-screen onto uip0002_CosChara00's model_character00 quad; the port
    // draws her in 3D instead (Navigator) and keeps the part's "Trying It On" plate (fitting_mark00) in front of her
    const std::string chara = create + "/uip0002_CosChara00", hint = create + "/common_uip0014_OpeButton00";
    ui.HideNode(chara, "model_character00", true);
    // shortcut: the curtain is not on the game's screen while a member stands there (N2); when it closes (likely
    // between members) is not traced, so it is not drawn
    ui.HideInstance(create + "/uip0018_CosCurtain00", true);
    Messages messages;
    if (!messages.Load(vfs, language, &error)) LogWarn("costumes: messages: {}", error);
    // its box 0 is _text00 by name (in mesh order the untextured model_character00 quad would be box 0)
    ui.TextBoxesByName("uip0002_CosChara00");
    ui.SetInstanceText(chara, 0, messages.Get(534));  // "Trying It On"
    ui.HideInstance(pop + "/uil0007_MusicList00", true);
    ui.HideInstance(pop + "/common_uip0052_ButtonSquare00", true);
    ui.HideNode(root + "/uip0001_CosTop00", "page_title01", true);  // "Costume Complete"; page_title00 is this screen
    for (const char* n : {"preview00_card00_R", "preview00_card00_R_line", "preview00_card00_HR", "preview00_card00_HR_line"}) {
        ui.HideNode(create + "/uip0007_CosPreview00", n, true);
    }
    ui.HideNode(create + "/uip0006_CosInfo00", "cos_caution00", true);
    ui.HideNode(pop_part, "cos_caution01", true);
    ui.HideNode(pop_part, "card_preview_chara00", true);
    ui.HideNode(pop_part, "card_preview_bg00", true);

    std::array<std::vector<int>, 3> lists;  // the costumes of each group, in the table's order
    for (const costume::Recipe& r : data.recipes) lists[size_t(r.Group())].push_back(r.dress);
    const auto name_of = [&](int dress) {
        for (const auto& [id, n] : names) {
            if (id == dress) return n;
        }
        return std::to_string(dress);
    };
    // the member being dressed (the game's "Switch Members"): µ's 1-9, Aqours 11-19, Saint Snow 21-22; the grid shows
    // the member's group
    // it starts on the save's current member (R2 / L2, M13 N5) and her costume
    int m = save::Current(save), group = member::Group(m), cursor = 0;
    bool special = false;  // the L1 colour shown on the focused costume
    const auto wears = [&](int who) {
        const auto it = save.wear.find(who);
        return it == save.wear.end() ? 0 : it->second;
    };
    const auto on_member = [&] {  // the grid of her group, focused on what she wears
        group = member::Group(m);
        const std::vector<int>& now = lists[size_t(group)];
        const auto at = std::find(now.begin(), now.end(), wears(m));
        cursor = at == now.end() ? 0 : int(at - now.begin());
        special = save.special.contains(m);
    };
    on_member();
    Navigator nav;
    int dressed_member = -1, dressed = -1;
    bool dressed_special = false;
    MemberPopup popup;
    // "Check Costume" / "Change Costume?" (M17 T6; 00_common uil0062_CosPop00, shadPS4 work/m13/shad/cs5.png): Back
    // (focused) / OK before Change Clothes. shortcut: the costume's thumbnail where the game shows the member card
    UiText ask_text;
    UiScreen ask;
    int asking = -1;  // -1 closed, else the focused button (0 Back, 1 OK)
    constexpr const char* kAsk = "uil0062_CosPop00";
    if (!ask_text.Load(vfs, lang, &error)) LogWarn("costumes: text: {}", error);
    const bool can_ask = ask.Load(renderer, vfs, "ui2/00_common", lang, &error, &ask_text);
    if (!can_ask) LogWarn("costumes: {}", error);
    const auto show_ask = [&] {
        const std::string at = std::string(kAsk) + "/";
        for (int b = 0; b < 2; ++b) {
            const std::string button = at + "uip0005_SelectButton00_0" + std::to_string(b);
            ask.SetMaterialUv(button, "button_text00_M", 0, b == 0 ? 0.25f : 0.375f);  // Back, OK
            ask.HideNode(button, "button_bg00_on", b != asking);
            ask.HideNode(button, "button_bg00_off", b == asking);
            ask.HideNode(button, "button_effect00", b != asking);
        }
    };
    const auto has_model = [&](int m, int dress) {
        char path[64];
        std::snprintf(path, sizeof(path), "chr/c%02d/d%03d/d%03d.pac", m, dress, dress);
        return vfs.Exists(path);
    };
    bool popped = false;
    const auto show = [&] {
        const std::vector<int>& list = lists[size_t(group)];
        cursor = std::clamp(cursor, 0, std::max(0, int(list.size()) - 1));
        for (int g = 0; g < 3; ++g) ui.HideInstance(create + kGrids[g], g != group);
        for (int k = 0; k < kSlots; ++k) {
            const std::string icon = create + kGrids[group] + Two("/uip0004_CosIcon00_%02d", k);
            ui.HideInstance(icon, k >= int(list.size()));
            if (k >= int(list.size())) continue;
            const int dress = list[size_t(k)];
            ui.SetMaterialTexture(icon, "chara_img00_M", costume::ThumbPath(dress));
            ui.HideNode(icon, "costume_icon00_on", k != cursor);
            ui.HideNode(icon, "costume_icon00_on_eff", k != cursor);
            ui.HideNode(icon, "costume_icon00_off", k == cursor);
            ui.HideNode(icon, "costume_icon00_complet", StateOf(data, addons, save, dress) != 0);
            for (const char* n : {"gage_key00", "gage_normal00", "gage_key00_stencil", "gage_normal00_stencil"}) {
                ui.HideNode(icon, n, true);
            }
        }
        if (list.empty()) return;
        const int dress = list[size_t(cursor)], state = StateOf(data, addons, save, dress);
        const costume::Recipe* r = costume::FindRecipe(data, dress);
        ui.SetInstanceText(create + "/uip0003_CosName00", 0, name_of(dress));
        ui.SetMaterialTexture(create + "/uip0007_CosPreview00", "costume_preview00_M", costume::ThumbPath(dress));
        for (int l = 0; l < 4; ++l) {
            ui.HideNode(create + "/uip0006_CosInfo00", Two("status_label%02d", l), l != kLabelOf[state]);
            ui.HideNode(pop_part, Two("status_label_pop%02d", l), l != kLabelOf[state]);
        }
        const int worn = wears(m);
        const bool has_special = vfs.Exists(member::CostumePack(m, dress, true));
        if (!has_special) special = false;
        // L1 "Change to special color" / "Default" above the grid, only for costumes that have one (N2 cs2.png, cs4.png)
        ui.HideInstance(hint, !has_special);
        ui.SetInstanceText(hint, 0, messages.Get(special ? 604 : 603));
        // "Trying It On" while the focused costume is not the one she wears (not for the colour alone: N2 cs4.png)
        ui.HideNode(chara, "fitting_mark00", dress == worn);
        if (m != dressed_member || dress != dressed || special != dressed_special) {
            const bool same_member = m == dressed_member;
            if (nav.Load(renderer, vfs, m, dress, special, &error)) {
                if (same_member) nav.Play(518);  // dresschk: she looks at the new costume
            } else {
                LogWarn("costumes: member {} in {}: {}", m, dress, error);
            }
            dressed_member = m, dressed = dress, dressed_special = special;
        }
        const std::string wearing = messages.Get(member::ProfileOf(m).short_name) + " wears: " +
                                    (worn ? name_of(worn) : std::string("own default")) +
                                    (state == 0 && dress != worn ? (has_model(m, dress) ? "  (Enter: wear this)" : "  (no model)") : "");
        ui.SetInstanceText(create + "/common_uip0009_SelectInfoWin00", 0,
                           (r ? r->text[size_t(language)] : std::string()) + "\n\n" + wearing);
        ui.SetInstanceText(create + "/common_uip0009_SelectInfoWin00", 1, "");
        ui.HideInstance(pop, !popped);
        if (!popped) return;
        // Confirm Costume Creation: text boxes 0 the question, 1 the name (Hypothesis, checked by capture)
        ui.SetInstanceText(pop_part, 0, state == 1 ? "Make this costume?" : "Not enough materials to make this costume.");
        ui.SetInstanceText(pop_part, 1, name_of(dress));
        ui.SetInstanceText(pop_part, 2, "");
        ui.SetInstanceText(pop_part, 3, "");
        ui.SetMaterialTexture(pop_part, "banner_img00_M", costume::ThumbPath(dress));
        for (int k = 0; k < 12; ++k) {
            const std::string row = pop + "/uil0006_CosMaterialList00" + Two("/uip0011_MaterialParts02_%02d", k);
            const bool used = r && k < int(r->needs.size());
            ui.HideInstance(row, !used);
            if (!used) continue;
            const auto [item, amount] = r->needs[size_t(k)];
            const costume::Item* i = costume::FindItem(data, item);
            if (i) ui.SetMaterialTexture(row, "icon_materiai00_M", costume::IconPath(*i));
            ui.SetInstanceNumber(row, 0, std::to_string(amount));
            ui.SetInstanceNumber(row, 1, std::to_string(costume::ItemCount(save, item)));
        }
        // the button labels are cells of com_popup_button_text00_T (2 columns, rows of 1/8): Back (0, 2), Create (1, 2)
        ui.SetMaterialUv(pop + "/common_uip0005_SelectButton00_00", "button_text00_M", 0.0f, 0.25f);
        ui.SetMaterialUv(pop + "/common_uip0005_SelectButton00_01", "button_text00_M", 0.5f, 0.25f);
    };
    show();
    SDL_SetWindowTitle(window, "SIF port - costumes");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const std::vector<int>& list = lists[size_t(group)];
            const int n = int(list.size());
            if (popup.IsOpen()) {  // R2: the member popup, as on the top menu
                if (!e.key.repeat && popup.Key(e.key.scancode) == MemberPopup::Result::Chosen) {
                    m = popup.Chosen();
                    save.mode = member::Mode(m), save.navigator[save.mode] = m;
                    on_member();
                    show();
                }
                continue;
            }
            if (asking >= 0) {
                if (e.key.repeat) continue;
                const SDL_Scancode k = e.key.scancode;
                if (k == SDL_SCANCODE_LEFT || k == SDL_SCANCODE_RIGHT) {
                    asking = 1 - asking;
                    show_ask();
                } else if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_SPACE || k == SDL_SCANCODE_ESCAPE ||
                           k == SDL_SCANCODE_BACKSPACE) {
                    const bool ok = asking == 1 && (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_SPACE);
                    asking = -1;
                    LogInfo("costumes: change costume? {}", ok ? "OK" : "Back");
                    if (ok) {
                        const int dress = list[size_t(cursor)];
                        save.wear[m] = dress;
                        if (special) save.special.insert(m); else save.special.erase(m);
                        nav.Play(519);  // dressfix
                        LogInfo("costumes: member {} wears {} {}{}", m, dress, name_of(dress), special ? " (special colour)" : "");
                    }
                    show();
                }
                continue;
            }
            if (popped) {
                if (e.key.repeat) continue;
                if (e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_SPACE) {
                    const costume::Recipe* r = n ? costume::FindRecipe(data, list[size_t(cursor)]) : nullptr;
                    if (r && costume::Craft(*r, save)) LogInfo("costumes: made {} {}", r->dress, name_of(r->dress));
                    popped = false;
                } else if (e.key.scancode == SDL_SCANCODE_ESCAPE || e.key.scancode == SDL_SCANCODE_BACKSPACE) {
                    popped = false;
                }
                show();
                continue;
            }
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT: cursor = std::max(0, cursor - 1); break;
                case SDL_SCANCODE_RIGHT: cursor = std::min(n - 1, cursor + 1); break;
                case SDL_SCANCODE_UP: cursor = cursor >= kColumns ? cursor - kColumns : cursor; break;
                case SDL_SCANCODE_DOWN: cursor = cursor + kColumns < n ? cursor + kColumns : cursor; break;
                case SDL_SCANCODE_PAGEUP:  // L1: the special colour, where the costume has one
                    if (!e.key.repeat && n && vfs.Exists(member::CostumePack(m, list[size_t(cursor)], true))) {
                        special = !special;
                        LogInfo("costumes: member {} {} in {} colours", m, list[size_t(cursor)], special ? "special" : "default");
                    }
                    break;
                case SDL_SCANCODE_HOME:  // L2: the other mode's member
                    if (e.key.repeat) break;
                    save.mode = 1 - save.mode;
                    m = save::Current(save);
                    on_member();
                    LogInfo("costumes: mode {}, member {}", save.mode, m);
                    break;
                case SDL_SCANCODE_END:  // R2: Switch Members
                    if (!e.key.repeat) popup.Open(renderer, vfs, lang, save.mode, m);
                    continue;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE: {
                    if (e.key.repeat || !n) break;
                    const int dress = list[size_t(cursor)], state = StateOf(data, addons, save, dress);
                    popped = state == 1 || state == 2;  // add-on only: no recipe
                    if (state == 0 && has_model(m, dress) && can_ask) {  // owned: "Change Clothes" asks first
                        asking = 0;
                        ask.SetMaterialUv(std::string(kAsk) + "/uip1002_ComPopBG01", "title_text00_M", 0, 0.125f);
                        ask.SetInstanceText(std::string(kAsk) + "/uip1014_ComPopType04", 0, messages.Get(535));
                        ask.SetMaterialTexture(std::string(kAsk) + "/uip1014_ComPopType04", "banner_img00_M",
                                               costume::ThumbPath(dress));
                        show_ask();
                        ask.Play("*", UiScreen::ClipName(1), false);
                        LogInfo("costumes: change costume? {} {}", dress, name_of(dress));
                    }
                    break;
                }
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
        if (asking >= 0) ask.Update(double(now - last) / 1e9);
        nav.Update(double(now - last) / 1e9);
        if (popup.IsOpen()) popup.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        // the screen, her in 3D over it, then the "Trying It On" plate over her (the chara part drawn again alone)
        const bool plate = !ui.NodeHidden(chara, "fitting_mark00");
        ui.HideNode(chara, "fitting_mark00", true);
        ui.DrawLayout(renderer, root, 1.0f);
        if (!popped) {  // shortcut: she is hidden while the craft question is up, instead of drawn under it
            renderer.Use3D();
            nav.Draw(renderer, Navigator::CostumeCamera(float(renderer.Width()) / float(std::max(1, renderer.Height()))));
            if (plate) {
                ui.HideNode(chara, "fitting_mark00", false);
                ui.DrawInstance(renderer, chara, 1.0f);
            }
        }
        ui.HideNode(chara, "fitting_mark00", !plate);
        popup.Draw(renderer);
        if (asking >= 0) ask.DrawLayout(renderer, kAsk, 1.0f, 960, 540);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    popup.Release(renderer);
    if (can_ask) ask.Release(renderer);
    nav.Release(renderer);
    ui.Release(renderer);
    return open;
}

}
