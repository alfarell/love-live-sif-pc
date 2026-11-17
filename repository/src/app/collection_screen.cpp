#include "app/collection_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/profile_card.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/formats/png.h"
#include "engine/platform/os.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/card/card.h"
#include "game/costume/costume.h"
#include "game/profile/profile.h"
#include "game/save/save.h"
#include "game/text/messages.h"

namespace sif {
namespace {

constexpr const char* kRoot = "uil0001_CollLayout00";
const std::string kWin = std::string(kRoot) + "/uil0011_GalleryComWin00";
const std::string kList = kWin + "/uil0022_CollSelectList00";
constexpr int kColumns = 7;  // uil0022_CollSelectList00: card00 6 rows of 7 (240 apart), photo00 8 rows of 7 (144 apart)

std::string Two(const char* format, int k) {
    char s[128];
    std::snprintf(s, sizeof(s), format, k);
    return s;
}

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

std::string Message(const Messages& m, int id, const char* fallback) {
    const std::string& s = m.Get(id);
    return s.empty() ? std::string(fallback) : s;
}

// The parts both screens share: the collection frame without the skill window and the preview, the title row of
// col_title_text00_T (rows of 1/8: Member Card, Skill Card, Photo Gallery, AC Archive), the group background
bool LoadFrame(UiScreen& ui, UiText& text, Renderer& renderer, const Vfs& vfs, const std::string& lang, float title_v,
               const char* who) {
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("{}: text: {}", who, error);
    if (!ui.Load(renderer, vfs, "ui2/40_collection", lang, &error, &text)) {
        LogError("{}: {}", who, error);
        return false;
    }
    ui.HideInstance(std::string(kRoot) + "/uil0012_GallerySkillWin00", true);
    ui.HideInstance(std::string(kRoot) + "/uil0010_ImgPreview00", true);
    ui.SetMaterialUv(std::string(kRoot) + "/uip0001_CollTop00", "title_mode00", 0, title_v);
    ui.SetInstanceText(kWin + "/uip0007_SelectWin00", 0, "");  // its own text is "no photos" (Japanese placeholder)
    return true;
}

void ShowGroup(UiScreen& ui, bool aqours) {
    ui.HideInstance(std::string(kRoot) + "/common_uil0101_ComBGGroup_Member01/common_uip0400_ComTopBG_Ms00", aqours);
    ui.HideInstance(std::string(kRoot) + "/common_uil0101_ComBGGroup_Member01/common_uip0410_ComTopBG_Aqours00", !aqours);
    ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab00", "select_group00", aqours);
    ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab00", "select_group01", !aqours);
}

// A photo shown at a quarter size (every 4th pixel; shortcut: no filtering)
bool LoadThumb(Renderer& renderer, const std::filesystem::path& file, Texture& out) {
    std::vector<uint8_t> rgba;
    uint32_t w = 0, h = 0;
    std::string error;
    if (!png::Read(file, rgba, w, h, &error)) {
        LogWarn("photo gallery: {}", error);
        return false;
    }
    return MakeThumb(renderer, rgba, w, h, out);
}

}

bool RunPhotoGallery(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                     const std::filesystem::path& data_dir, save::SaveData& save, long max_frames) {
    route::SetScreen("photo_gallery");
    UiText text;
    UiScreen ui;
    if (!LoadFrame(ui, text, renderer, vfs, lang, 0.25f, "photo gallery")) return true;
    Messages messages;
    std::string error;
    if (!messages.Load(vfs, Language(lang), &error)) LogWarn("photo gallery: messages: {}", error);
    ShowGroup(ui, false);
    // no member tabs, no card grid; the favourite counter top right (uil0014: mark, "Favorites:", n, "(Maximum ...)")
    ui.HideInstance(kWin + "/uil0006_SelectTab00", true);
    for (int k = 0; k < 42; ++k) ui.HideInstance(kList + Two("/uil0004_CardPoint00_%02d", k), true);
    const std::string fav = kWin + "/uil0014_FavoriteSelect00";
    ui.SetMaterialUv(fav + "/common_uip0020_CheckMark00", "check00_M", 0.5f, 0);
    ui.SetInstanceText(fav + "/uip0009_FavoriteNumber00", 0, Message(messages, 796, "Favorites: "));
    char most[64];
    std::snprintf(most, sizeof(most), Message(messages, 798, "(Maximum Number: %2d)").c_str(), profile::kMaxFavourites);
    ui.SetInstanceText(fav + "/uip0009_FavoriteNumber00", 2, most);
    // the game shows the grid scrolled so its first row sits 184 px lower (shad2/c01); all 30 photos fit rows 0-4
    constexpr float kScroll = -184;
    ui.MoveInstance(kList, 0, kScroll);
    for (const char* bar : {"/common_uip0906_CollectionScrollBar00", "/common_uip0907_CollectionScrollBar_slider00"}) {
        ui.HideInstance(kWin + bar, true);  // shortcut: nothing to scroll
    }

    std::vector<int> slots = profile::PhotosNewestFirst(save);
    std::map<int, Texture> thumbs;  // slot -> texture
    for (const int slot : slots) {
        Texture t;
        if (LoadThumb(renderer, data_dir / os::PathFromUtf8(profile::PhotoFile(slot)), t)) thumbs[slot] = t;
    }
    int pick = 0;
    Texture full;  // the photo shown full screen (Space)
    bool full_screen = false;
    const auto favourites = [&] {
        int n = 0;
        for (const auto& [slot, p] : save.photos) n += p.favourite ? 1 : 0;
        return n;
    };
    const auto show = [&] {
        for (int k = 0; k < 56; ++k) {
            const std::string cell = kList + Two("/uil0013_PhotoPoint00_%02d", k);
            const bool on = k < int(slots.size());
            ui.HideInstance(cell, !on);
            if (!on) continue;
            const int slot = slots[size_t(k)];
            if (const auto t = thumbs.find(slot); t != thumbs.end()) {
                ui.SetMaterialTexture(cell + "/uip0012_ProfilePhoto00", "photo01_M", &t->second);
            }
            ui.HideNode(cell + "/uip0012_ProfilePhoto00", "copyright00", true);
            const bool f = save.photos.at(slot).favourite;
            ui.HideInstance(cell + "/common_uip0020_CheckMark00", !f);  // the game marks favourites only (shad2/c01)
            ui.SetMaterialUv(cell + "/common_uip0020_CheckMark00", "check00_M", 0.5f, 0);
        }
        ui.SetInstanceText(fav + "/uip0009_FavoriteNumber00", 1, std::to_string(favourites()));
        ui.SetInstanceText(kWin + "/uip0007_SelectWin00", 0, slots.empty() ? "No photos." : "");
    };
    show();
    LogInfo("photo gallery: {} photos, {} favourites", slots.size(), favourites());
    SDL_SetWindowTitle(window, "SIF port - photo gallery");
    bool open = true, changed = false;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int n = int(slots.size());
            if (full_screen) {  // any key closes the full-screen view
                full_screen = false;
                renderer.Release(full);
                continue;
            }
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT: pick = std::max(0, pick - 1); break;
                case SDL_SCANCODE_RIGHT: pick = std::min(std::max(0, n - 1), pick + 1); break;
                case SDL_SCANCODE_UP: pick = std::max(0, pick - kColumns); break;
                case SDL_SCANCODE_DOWN: pick = std::min(std::max(0, n - 1), pick + kColumns); break;
                case SDL_SCANCODE_Q:
                    if (pick < n) {
                        save::Photo& p = save.photos.at(slots[size_t(pick)]);
                        if (p.favourite || favourites() < profile::kMaxFavourites) {
                            p.favourite = !p.favourite, changed = true;
                            LogInfo("photo gallery: photo {} favourite {}", slots[size_t(pick)], p.favourite ? "on" : "off");
                        } else {
                            LogInfo("photo gallery: {} favourites already", profile::kMaxFavourites);
                        }
                    }
                    break;
                case SDL_SCANCODE_DELETE:
                    if (pick < n) {
                        const int slot = slots[size_t(pick)];
                        if (save.photos.at(slot).favourite) {  // message 800: favourites cannot be deleted
                            LogInfo("photo gallery: photo {} is a favourite, not deleted", slot);
                            break;
                        }
                        // shortcut: the game asks first (message 799); the port deletes at once
                        std::error_code ec;
                        std::filesystem::remove(data_dir / os::PathFromUtf8(profile::PhotoFile(slot)), ec);
                        save.photos.erase(slot);
                        if (auto t = thumbs.find(slot); t != thumbs.end()) renderer.Release(t->second), thumbs.erase(t);
                        slots = profile::PhotosNewestFirst(save);
                        pick = std::min(pick, std::max(0, int(slots.size()) - 1));
                        changed = true;
                        LogInfo("photo gallery: photo {} deleted", slot);
                    }
                    break;
                case SDL_SCANCODE_SPACE:
                case SDL_SCANCODE_RETURN:
                    if (pick < n) {
                        std::vector<uint8_t> rgba;
                        uint32_t w = 0, h = 0;
                        if (png::Read(data_dir / os::PathFromUtf8(profile::PhotoFile(slots[size_t(pick)])), rgba, w, h, &error) &&
                            renderer.CreateTexture(w, h, full, &error) && renderer.UpdateTexture(full, rgba)) {
                            full_screen = true;
                            LogInfo("photo gallery: photo {} full screen", slots[size_t(pick)]);
                        } else {
                            LogWarn("photo gallery: {}", error);
                        }
                    }
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
        if (full_screen) {
            renderer.DrawTextureFitted(full);
        } else {
            ui.DrawLayout(renderer, kRoot, 1.0f);
            if (!slots.empty()) {  // the focus frame: cells 244 apart, rows 144 apart from (-732, 432) of the list
                ui.DrawPart(renderer, "uip0065_CollPhotoCursor00", 946.0f - 732.0f + 244.0f * float(pick % kColumns),
                            -520.0f + 432.0f - 144.0f * float(pick / kColumns) + kScroll, 1.0f);
            }
        }
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    renderer.Release(full);
    for (auto& [slot, t] : thumbs) renderer.Release(t);
    ui.Release(renderer);
    if (changed) LogInfo("photo gallery: {} photos, {} favourites", slots.size(), favourites());
    return open;
}

bool RunMemberCards(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                    const MemberCardSetup& setup, long max_frames) {
    route::SetScreen("member_card");
    UiText text;
    UiScreen ui;
    if (!LoadFrame(ui, text, renderer, vfs, lang, 0.0f, "member cards")) return true;
    Messages messages;
    std::string error;
    if (!messages.Load(vfs, Language(lang), &error)) LogWarn("member cards: messages: {}", error);
    ui.HideInstance(kWin + "/uil0014_FavoriteSelect00", true);
    for (int k = 0; k < 56; ++k) ui.HideInstance(kList + Two("/uil0013_PhotoPoint00_%02d", k), true);
    bool aqours = setup.member >= 11;
    int tab = 0, pick = 0, top = 0;
    const auto members = [&] {
        std::vector<int> m;
        if (aqours) {
            for (int i = 11; i <= 19; ++i) m.push_back(i);
            m.push_back(21), m.push_back(22);
        } else {
            for (int i = 1; i <= 9; ++i) m.push_back(i);
        }
        return m;
    };
    tab = std::clamp(setup.member - (aqours ? 11 : 1), 0, aqours ? 10 : 8);
    std::vector<const card::MemberCard*> cards;  // the member's R pictures, one per costume, in table order
    const auto owns = [&](const card::MemberCard& c) {
        return setup.costumes && setup.save && costume::Owns(*setup.costumes, *setup.save, setup.addons, c.costume);
    };
    const auto load_tab = [&] {
        const int member = members()[size_t(tab)];
        cards.clear();
        for (const card::MemberCard& c : setup.cards->members) {
            if (c.member == member && c.id % 10 == 0) cards.push_back(&c);
        }
        pick = top = 0;
        int owned = 0;
        for (const card::MemberCard* c : cards) owned += owns(*c) ? 1 : 0;
        LogInfo("member cards: member {}, {} cards, {} owned", member, cards.size(), owned);
    };
    const auto name_of = [&](int dress) {
        for (const auto& [id, name] : setup.names) {
            if (id == dress) return name;
        }
        return std::string();
    };
    load_tab();
    const auto show = [&] {
        const std::vector<int> m = members();
        ShowGroup(ui, aqours);
        for (int k = 0; k < 11; ++k) {
            const std::string t = kWin + Two("/uil0006_SelectTab00/uip0006_SelectTab00_%02d", k);
            ui.HideInstance(t, k >= int(m.size()));
            if (k >= int(m.size())) continue;
            const int mem = m[size_t(k)], cell = mem <= 9 ? mem - 1 : mem <= 19 ? mem - 2 : mem - 3;
            for (const char* mat : {"coll_page_tab00_M", "coll_page_tab01_M"}) {
                ui.SetMaterialUv(t, mat, float(cell % 5) * 0.2f, float(cell / 5) * 0.2f);
            }
            ui.HideNode(t, "coll_page_tab00_on", k != tab);
            ui.HideNode(t, "coll_page_tab00_off", k == tab);
        }
        // shortcut: as Collection -> Skill Card, the rows inside the window (1-3, cells 07-27) show; whole-row paging
        top = std::clamp(top, std::max(0, (pick / kColumns - 2) * kColumns), pick / kColumns * kColumns);
        for (int k = 0; k < 42; ++k) {
            const std::string cell = kList + Two("/uil0004_CardPoint00_%02d", k);
            const int i = top + k - kColumns;
            const bool inside = k >= kColumns && k < 4 * kColumns && i < int(cards.size());
            ui.HideInstance(cell, !inside);
            if (!inside) continue;
            const card::MemberCard& c = *cards[size_t(i)];
            const bool face_up = owns(c);
            for (const char* other : {"/uip0010_ProfileCard01", "/uip0020_SkillCard01", "/uip0025_ArtWork01",
                                      "/common_uip0020_CheckMark00", "/uip0060_CollSelectMark00_00",
                                      "/uip0060_CollSelectMark00_01"}) {
                ui.HideInstance(cell + other, true);
            }
            ui.HideInstance(cell + "/uip0018_CardBack01", face_up);
            ui.HideInstance(cell + "/uip0022_MemberCard01", !face_up);
            if (face_up) {
                ui.SetMaterialTexture(cell + "/uip0022_MemberCard01", "member_card00_M",
                                      "ui/ui_member_card_thumbnail.pac/texture.pac/mbc_" + std::to_string(c.id));
            }
        }
        if (pick < int(cards.size())) {
            const card::MemberCard& c = *cards[size_t(pick)];
            const std::string name = name_of(c.costume);
            char line[256];
            std::snprintf(line, sizeof(line), owns(c) ? "%s" : Message(messages, 490, "(Not possessed)%s").c_str(),
                          name.c_str());
            SDL_SetWindowTitle(window, ("SIF port - member card: " + std::string(line)).c_str());
        }
    };
    show();
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int n = int(cards.size()), tabs = int(members().size());
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT: pick = std::max(0, pick - 1); break;
                case SDL_SCANCODE_RIGHT: pick = std::min(std::max(0, n - 1), pick + 1); break;
                case SDL_SCANCODE_UP: pick = std::max(0, pick - kColumns); break;
                case SDL_SCANCODE_DOWN: pick = std::min(std::max(0, n - 1), pick + kColumns); break;
                case SDL_SCANCODE_PAGEUP: tab = (tab + tabs - 1) % tabs, load_tab(); break;  // L1
                case SDL_SCANCODE_PAGEDOWN: tab = (tab + 1) % tabs, load_tab(); break;       // R1
                case SDL_SCANCODE_TAB: aqours = !aqours, tab = 0, load_tab(); break;        // L2 (shortcut: Tab key)
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
        ui.DrawLayout(renderer, kRoot, 1.0f);
        if (!cards.empty()) {  // the focus frame, as Collection -> Skill Card's (cells 244 apart, rows 240 apart)
            const int k = pick - top + kColumns;
            ui.DrawPart(renderer, "uip0061_CollCursor00", 946.0f - 732.0f + 244.0f * float(k % kColumns),
                        -520.0f + 480.0f - 240.0f * float(k / kColumns), 1.0f);
        }
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    return open;
}

}
