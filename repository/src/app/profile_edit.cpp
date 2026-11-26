#include "app/profile_edit.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/route.h"
#include "app/skill_screen.h"
#include "engine/core/log.h"
#include "engine/formats/png.h"
#include "engine/platform/os.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/card/card.h"
#include "game/profile/profile.h"
#include "game/save/save.h"
#include "game/text/messages.h"

namespace sif {
namespace {

constexpr const char* kRoot = "uil0050_ProcEditLayout";
const std::string kWin = std::string(kRoot) + "/uil0052_ProcEditWin00";
enum Tab { kFrame, kPhoto, kAccolade, kName, kBadge, kSkill, kFinish, kTabs };
// The list window's groups (uil0052) per tab, with the slot grid from the nodes' positions: columns, the rows that fit
// the window, the first row's y, the row step, and the shift that puts the first row at the window's top (accolade,
// name plate, badge as the M7 screen; frame, photo and skill measured against shadPS4 p04, p06, p14). shortcut: whole
// pages, no smooth scroll
struct Grid {
    const char* group;
    int slots, columns, rows;
    float first_x, first_y, step_x, step_y, shift;
};
constexpr std::array<Grid, 6> kGrids = {{{"uil0060_ProfileFrameGroup00", 20, 4, 3, -402, 478, 260, 238, -236},
                                         {"uil0061_ProfilePhotoGroup00", 28, 4, 5, -402, 432, 260, 144, -147},
                                         {"uil0062_ProfileTitleGroup00", 48, 4, 6, 0, 0, 0, 0, -54},
                                         {"uil0063_ProfileNameGroup00", 30, 3, 5, 0, 0, 0, 0, -70},
                                         {"uil0064_ProfileBadgeGroup00", 48, 4, 6, 0, 0, 0, 0, -54},
                                         {"uil0065_ProfileskillGroup00", 30, 6, 3, -447, 480, 174, 240, -240}}};
// col_pro_select_text00_T: 4 x 2 cells (Card Frame, Photo, Accolade, Name Plate / Badge, Skill Card, Player's Name)
constexpr std::array<std::array<float, 2>, 6> kTabLabel = {
    {{0, 0}, {0.25f, 0}, {0.5f, 0}, {0.75f, 0}, {0, 0.5f}, {0.25f, 0.5f}}};
constexpr float kWinX = 600, kWinY = -620;  // uil0052's place in uil0050

std::string Slot(int tab, int k) {
    char s[128];
    std::snprintf(s, sizeof(s), "%s/%s_%02d", kWin.c_str(), kGrids[size_t(tab)].group, k);
    return s;
}

// One choosable frame: a frame in one orientation (the game lists portrait and landscape as two items, shad2/p04)
struct FrameItem {
    int frame = 0, member = 1;
    bool landscape = false;
};

}

bool RunProfileEdit(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const ProfileEditSetup& setup,
                    long max_frames) {
    route::SetScreen("profile_edit");
    save::SaveData& save = *setup.save;
    UiText text;
    std::string error;
    if (!text.Load(vfs, setup.lang, &error)) LogWarn("profile edit: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/40_collection", setup.lang, &error, &text)) {
        LogError("profile edit: {}", error);
        return true;
    }
    const Messages* m = setup.texts.messages;
    const auto msg = [&](int id, const char* fallback) {
        const std::string s = m ? m->Get(id) : std::string();
        return s.empty() ? std::string(fallback) : s;
    };
    const std::string root = kRoot, preview = root + "/uil0053_ProcPreview00", adjust = root + "/uil0054_PhotoAdjustWin00";
    ui.HideInstance(root + "/uil0031_PlateChangeMenu00", true);
    ui.HideInstance(root + "/uil0033_PlatePreview00", true);
    ui.HideInstance(preview + "/uip0010_ProfileCard00", true);  // the game's card JPEG; the port draws uip0019 instead
    ui.HideNode(preview + "/uip0034_ProcPreviewBG00", "card_bg_horizontal00", true);
    ui.HideNode(preview + "/uip0034_ProcPreviewBG00", "card_bg_vertical00", true);
    ui.SetInstanceText(preview + "/uip0034_ProcPreviewBG00", 0, msg(665, "Completed Image"));
    ui.SetInstanceText(adjust + "/uip0035_PhotoAdjust00", 0, msg(667, "Photo position adjustment"));
    ui.SetMaterialUv(root + "/uip0030_ProcEditTop00", "pop_title00_M", 0.0f, 0.0f);  // "Edit Profile Card" (row 1)
    const bool aqours = setup.member > 9;
    ui.HideInstance(root + "/common_uil0101_ComBGGroup_Member01/common_uip0400_ComTopBG_Ms00", aqours);
    ui.HideInstance(root + "/common_uil0101_ComBGGroup_Member01/common_uip0410_ComTopBG_Aqours00", !aqours);
    ui.HideNode(root + "/common_uip0008_GroupTab00", "select_group00", aqours);
    ui.HideNode(root + "/common_uip0008_GroupTab00", "select_group01", !aqours);

    // what can be chosen
    std::vector<FrameItem> frames;
    if (setup.frames) {
        for (const profile::Frame* f : profile::OwnedFrames(*setup.frames, save, setup.member)) {
            frames.push_back({f->id, f->member, false});
            frames.push_back({f->id, f->member, true});
        }
    }
    const std::vector<int> photos = profile::PhotosNewestFirst(save);
    const Nameplate empty_plate;
    const Nameplate& plate = setup.plate ? *setup.plate : empty_plate;
    std::vector<const card::SkillCard*> skills;
    if (setup.texts.cards) skills = card::OwnedSkills(*setup.texts.cards, save, setup.member);

    // the card being edited: the saved one, else defaults (the member's first frame, portrait, the newest photo, the
    // current nameplate choices, her set skill cards)
    save::ProfileCard card = save.card;
    int photo = -1;  // index into `photos`; -1 = the saved card's own photo (profile_card.png)
    if (!save.has_card) {
        card = {};
        card.frame = frames.empty() ? 1000 + setup.member : frames[0].frame;
        if (!photos.empty()) photo = 0;
        const save::PlateChoice& chosen = save.plates[size_t(save.mode == 1 ? 1 : 0)];
        card.accolade = chosen.accolade, card.badge = chosen.badge, card.name_plate = chosen.name_plate;
        if (setup.texts.cards) {
            const auto slots = card::Slots(*setup.texts.cards, save, setup.member);
            for (int k = 0; k < 3; ++k) card.skill[size_t(k)] = slots[size_t(k)];
        }
    }
    const auto photo_record = [&]() -> const save::Photo* {
        return photo >= 0 ? &save.photos.at(photos[size_t(photo)]) : nullptr;
    };
    if (const save::Photo* p = photo_record()) card.live = p->live, card.member_card = p->member_card;

    // pictures: the photo list's thumbnails and the chosen photo at full size
    std::map<int, Texture> thumbs;  // index into photos
    for (size_t i = 0; i < photos.size() && i < size_t(kGrids[kPhoto].slots); ++i) {
        std::vector<uint8_t> rgba;
        uint32_t w = 0, h = 0;
        if (png::Read(setup.data_dir / os::PathFromUtf8(profile::PhotoFile(photos[i])), rgba, w, h, &error) &&
            MakeThumb(renderer, rgba, w, h, thumbs[int(i)])) {
            continue;
        }
        LogWarn("profile edit: {}", error);
    }
    std::vector<uint8_t> full;
    uint32_t full_w = 0, full_h = 0;
    Texture card_photo, adjust_photo;
    const auto load_photo = [&] {
        const std::filesystem::path file = photo >= 0 ? setup.data_dir / os::PathFromUtf8(profile::PhotoFile(photos[size_t(photo)]))
                                                      : setup.data_dir / "profile_card.png";
        full.clear();
        if (!png::Read(file, full, full_w, full_h, &error)) {
            LogWarn("profile edit: {}", error);
            full_w = full_h = 0;
        }
        renderer.Release(adjust_photo);
        if (full_w && renderer.CreateTexture(full_w, full_h, adjust_photo, nullptr)) renderer.UpdateTexture(adjust_photo, full);
    };
    const auto make_card_photo = [&] {
        if (full_w) card.crop = MakeCardPhoto(renderer, full, full_w, full_h, card.landscape, card.crop, card_photo);
    };
    load_photo();
    make_card_photo();

    int tab = 0, cursor = 0;
    bool in_list = false, adjusting = false, saved = false;
    const auto count = [&](int t) -> int {
        switch (t) {
            case kFrame: return int(frames.size());
            case kPhoto: return int(photos.size());
            case kAccolade: return int(plate.accolades.size());
            case kName: return int(plate.name_plates.size());
            case kBadge: return int(plate.badges.size());
            case kSkill: return int(skills.size());
            default: return 0;
        }
    };
    // the item a tab shows as chosen (the star); skill cards show their set order instead
    const auto chosen = [&](int t) -> int {
        for (int i = 0; i < count(t); ++i) {
            if (t == kFrame && frames[size_t(i)].frame == card.frame && frames[size_t(i)].landscape == card.landscape) return i;
            if (t == kPhoto && i == photo) return i;
            if (t == kAccolade && plate.accolades[size_t(i)].id == card.accolade) return i;
            if (t == kName && plate.name_plates[size_t(i)].id == card.name_plate) return i;
            if (t == kBadge && plate.badges[size_t(i)].id == card.badge) return i;
        }
        return -1;
    };
    const auto show = [&] {
        // the tab bar (uil0051): six tabs and Finish Editing
        const std::string menu = root + "/uil0051_ProcEditMenu00";
        for (int k = 0; k < 6; ++k) {
            char icon[128];
            std::snprintf(icon, sizeof(icon), "%s/uip0031_EditMenuIcon00_%02d", menu.c_str(), k);
            ui.SetMaterialUv(icon, "edit_menu_title00_M", kTabLabel[size_t(k)][0], kTabLabel[size_t(k)][1]);
            ui.HideNode(icon, "edit_menu_button00_on", k != tab);
            ui.HideNode(icon, "edit_menu_button00_eff", k != tab || in_list);
            ui.HideNode(icon, "edit_menu_button00_off", k == tab);
        }
        ui.HideNode(menu + "/uip0032_EditMenuIcon01", "edit_menu_done00_on", tab != kFinish);
        ui.HideNode(menu + "/uip0032_EditMenuIcon01", "edit_menu_done00_eff", tab != kFinish);
        ui.HideNode(menu + "/uip0032_EditMenuIcon01", "edit_menu_done00_off", tab == kFinish);
        // the list window: the current tab's group, one page of it
        ui.HideInstance(kWin, adjusting);
        ui.HideInstance(adjust, !adjusting);
        const int list_tab = tab == kFinish ? -1 : tab;
        for (int g = 0; g < int(kGrids.size()); ++g) {
            const Grid& grid = kGrids[size_t(g)];
            const int per_page = grid.columns * grid.rows, page = g == list_tab ? cursor / per_page : 0;
            for (int k = 0; k < grid.slots; ++k) {
                const int i = page * per_page + k;
                ui.HideInstance(Slot(g, k), g != list_tab || k >= per_page || i >= count(g));
                ui.MoveInstance(Slot(g, k), 0.0f, g == list_tab ? grid.shift : 0.0f);
            }
        }
        ui.SetInstanceText(kWin + "/uip0033_ProcEditWinBG00", 0, list_tab >= 0 && count(list_tab) == 0 ? "-" : "");
        if (list_tab >= 0) {
            const Grid& grid = kGrids[size_t(list_tab)];
            const int per_page = grid.columns * grid.rows, page = cursor / per_page, star = chosen(list_tab);
            for (int k = 0; k < per_page; ++k) {
                const int i = page * per_page + k;
                if (i >= count(list_tab)) break;
                const std::string slot = Slot(list_tab, k);
                ui.HideInstance(slot + "/uip0004_StarMark00", i != star);
                ui.HideInstance(slot + "/uip0017_IconBack01", !(in_list && i == cursor));  // the focus (M7 groups)
                switch (list_tab) {
                    case kFrame: {
                        const FrameItem& f = frames[size_t(i)];
                        const std::string part = slot + "/uip0011_ProfileFrame00";
                        char tex[96];
                        std::snprintf(tex, sizeof(tex), "ui/ui_profile_card.pac/texture.pac/pfc_frame%s_000_%03d",
                                      f.landscape ? "_h" : "", f.member);
                        ui.SetMaterialTexture(part, f.landscape ? "profile_frame01_M" : "profile_frame00_M", tex);
                        ui.HideNode(part, "profile_frame00", f.landscape);
                        ui.HideNode(part, "profile_frame01", !f.landscape);
                        // the group's selection glow (col_select_card_ef00) is portrait-shaped; the cursor part marks the focus
                        ui.HideInstance(slot + "/uip0016_IconBack00_00", true);
                        break;
                    }
                    case kPhoto:
                        if (const auto t = thumbs.find(i); t != thumbs.end()) {
                            ui.SetMaterialTexture(slot + "/uip0012_ProfilePhoto00", "photo01_M", &t->second);
                        }
                        ui.HideNode(slot + "/uip0012_ProfilePhoto00", "copyright00", true);
                        break;
                    case kAccolade: {
                        const NameplateItem& item = plate.accolades[size_t(i)];
                        ui.SetMaterialTexture(slot + "/ctg03_uip0004_Title00", "title_plate00_M", TitlePlatePath(item.plate));
                        ui.SetInstanceText(slot + "/ctg03_uip0004_Title00", 0, item.name);
                        ui.SetInstanceText(slot + "/ctg03_uip0004_Title00", 1, "");
                        break;
                    }
                    case kName: {
                        const NameplateItem& item = plate.name_plates[size_t(i)];
                        ui.SetMaterialTexture(slot + "/ctg03_uip0005_NamePlate00", "name_plate00_M", NamePlatePath(item.id));
                        ui.SetInstanceText(slot + "/ctg03_uip0005_NamePlate00", 0, plate.player);
                        ui.SetInstanceText(slot + "/ctg03_uip0005_NamePlate00", 1, "");
                        break;
                    }
                    case kBadge: {
                        const NameplateItem& item = plate.badges[size_t(i)];
                        ui.SetMaterialTexture(slot + "/ctg03_uip0006_Badge00", "badge00_M", BadgePlatePath(item.plate));
                        ui.SetInstanceText(slot + "/ctg03_uip0006_Badge00", 0, item.name);
                        break;
                    }
                    case kSkill: {
                        const int id = skills[size_t(i)]->id;
                        ui.SetMaterialTexture(slot + "/uip0020_SkillCard00", "skill_card00_M", SkillThumbPath(id, setup.lang, false));
                        int order = -1;
                        for (int s = 0; s < 3; ++s) {
                            if (card.skill[size_t(s)] == id) order = s;
                        }
                        ui.HideInstance(slot + "/uip0005_SelectNumber00", order < 0);
                        if (order >= 0) ui.SetMaterialUv(slot + "/uip0005_SelectNumber00", "select_number00_M", 0.25f * float(order), 0);
                        break;
                    }
                    default: break;
                }
            }
        }
        // the Completed Image: the card as it is now (skill levels from what the player owns)
        if (setup.texts.cards) {
            for (int s = 0; s < 3; ++s) {
                card.skill_level[size_t(s)] = card.skill[size_t(s)] ? card::Level(*setup.texts.cards, save, card.skill[size_t(s)]) : 0;
            }
        }
        ApplyProfileCard(ui, "uip0019_ProfileCardCustom00", MakeProfileCardArt(card, setup.texts),
                         card_photo.gpu ? &card_photo : nullptr);
        if (adjusting && adjust_photo.gpu) {
            ui.SetMaterialTexture(adjust + "/uip0035_PhotoAdjust00", "photo_preview00", &adjust_photo);
            ui.HideNode(adjust + "/uip0035_PhotoAdjust00", "adjuster_vertical00", card.landscape);
            ui.HideNode(adjust + "/uip0035_PhotoAdjust00", "adjuster_horizontal00", !card.landscape);
            ui.HideNode(adjust + "/uip0035_PhotoAdjust00", "photo_black00", true);
        }
    };
    // choosing an item (Enter in a list)
    const auto choose = [&] {
        if (cursor >= count(tab)) return;
        switch (tab) {
            case kFrame:
                card.frame = frames[size_t(cursor)].frame;
                if (card.landscape != frames[size_t(cursor)].landscape) card.landscape = !card.landscape, make_card_photo();
                break;
            case kPhoto:
                photo = cursor;
                if (const save::Photo* p = photo_record()) card.live = p->live, card.member_card = p->member_card;
                card.crop = 0;
                load_photo();
                make_card_photo();
                break;
            case kAccolade: card.accolade = plate.accolades[size_t(cursor)].id; break;
            case kName: card.name_plate = plate.name_plates[size_t(cursor)].id; break;
            case kBadge: card.badge = plate.badges[size_t(cursor)].id; break;
            case kSkill: {
                const int id = skills[size_t(cursor)]->id;
                auto it = std::find(card.skill.begin(), card.skill.end(), id);
                if (it != card.skill.end()) {  // set: remove it, the later ones move up
                    std::rotate(it, it + 1, card.skill.end());
                    card.skill.back() = 0;
                } else if (auto free = std::find(card.skill.begin(), card.skill.end(), 0); free != card.skill.end()) {
                    *free = id;
                }
                break;
            }
            default: break;
        }
        LogInfo("profile edit: tab {} item {}", tab, cursor);
    };
    const auto finish = [&] {
        // the photo becomes the card's own (profile_card.png), so later photos or deletions do not change it
        if (photo >= 0) {
            std::error_code ec;
            std::filesystem::copy_file(setup.data_dir / os::PathFromUtf8(profile::PhotoFile(photos[size_t(photo)])),
                                       setup.data_dir / "profile_card.png", std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) LogWarn("profile edit: cannot copy the photo: {}", ec.message());
        }
        save.card = card;
        save.has_card = true;
        // mission kinds 24 (profile cards of the member) and 46 (one with a member in that costume), M15
        save::Add(save, "cards." + std::to_string(card.member_card / 10000));
        save::Mark(save, "card_costumes", card.member_card / 10 % 1000);
        saved = true;
        // shortcut: the game asks "Save changes and upload to the server?" and uploads (online); the port saves only
        LogInfo("profile edit: saved: frame {} {}, crop {}, live {}, member card {}, accolade {}, badge {}, name plate {}, "
                "skills {} {} {}",
                card.frame, card.landscape ? "landscape" : "portrait", card.crop, card.live, card.member_card, card.accolade,
                card.badge, card.name_plate, card.skill[0], card.skill[1], card.skill[2]);
    };
    show();
    LogInfo("profile edit: {} frames, {} photos, {} skill cards; {}", frames.size(), photos.size(), skills.size(),
            save.has_card ? "editing the card" : "a new card");
    SDL_SetWindowTitle(window, "SIF port - edit profile card");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const SDL_Scancode key = e.key.scancode;
            if (adjusting) {  // Left/Right move the photo inside the card's window
                if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT || key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN) {
                    const int step = std::max(1, int(full_w) / 64) * (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_UP ? -1 : 1);
                    card.crop += step;
                    make_card_photo();
                } else if (!e.key.repeat && (key == SDL_SCANCODE_Q || key == SDL_SCANCODE_ESCAPE || key == SDL_SCANCODE_RETURN ||
                                             key == SDL_SCANCODE_SPACE)) {
                    adjusting = false;
                    LogInfo("profile edit: photo position {}", card.crop);
                }
                show();
                continue;
            }
            const int n = count(tab), columns = tab < kFinish ? kGrids[size_t(tab)].columns : 1;
            if (!in_list) {  // the tab bar
                switch (key) {
                    case SDL_SCANCODE_LEFT:
                    case SDL_SCANCODE_PAGEUP: tab = (tab + kTabs - 1) % kTabs, cursor = 0; break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_PAGEDOWN: tab = (tab + 1) % kTabs, cursor = 0; break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_SPACE:
                        if (e.key.repeat) break;
                        if (tab == kFinish) {
                            finish();
                            fade.Leave(ui);
                        } else if (n > 0) {
                            in_list = true, cursor = std::max(0, chosen(tab));
                        }
                        break;
                    case SDL_SCANCODE_Q:
                        if (tab == kPhoto && full_w && !e.key.repeat) adjusting = true;
                        break;
                    case SDL_SCANCODE_ESCAPE:
                    case SDL_SCANCODE_BACKSPACE:
                        if (!e.key.repeat) fade.Leave(ui);
                        break;
                    default: break;
                }
            } else {  // a list
                switch (key) {
                    case SDL_SCANCODE_LEFT: cursor = std::max(0, cursor - 1); break;
                    case SDL_SCANCODE_RIGHT: cursor = std::min(std::max(0, n - 1), cursor + 1); break;
                    case SDL_SCANCODE_UP:
                        if (cursor >= columns) cursor -= columns;
                        else in_list = false;
                        break;
                    case SDL_SCANCODE_DOWN: cursor = cursor + columns < n ? cursor + columns : cursor; break;
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_SPACE:
                        if (!e.key.repeat) choose();
                        break;
                    case SDL_SCANCODE_Q:
                        if (tab == kPhoto && full_w && !e.key.repeat) adjusting = true;
                        break;
                    case SDL_SCANCODE_ESCAPE:
                    case SDL_SCANCODE_BACKSPACE: in_list = false; break;
                    default: break;
                }
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
        // the Completed Image (uil0053 at (1532, -592), the card at (0, -12)), about 0.74 of its size as in shad/p14
        ui.DrawPart(renderer, "uip0019_ProfileCardCustom00", 1532.0f, -604.0f, 1.0f, card.landscape ? 0.74f : 0.72f);
        // the focus on the frame, photo and skill lists (their groups have no focus part of their own)
        if (in_list && !adjusting && (tab == kFrame || tab == kPhoto || tab == kSkill)) {
            const Grid& g = kGrids[size_t(tab)];
            const int k = cursor % (g.columns * g.rows);
            ui.DrawPart(renderer, tab == kPhoto ? "uip0065_CollPhotoCursor00" : "uip0061_CollCursor00",
                        kWinX + g.first_x + g.step_x * float(k % g.columns),
                        kWinY + g.first_y - g.step_y * float(k / g.columns) + g.shift, 1.0f, tab == kSkill ? 0.7f : 1.0f);
        }
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    for (auto& [i, t] : thumbs) renderer.Release(t);
    renderer.Release(card_photo);
    renderer.Release(adjust_photo);
    ui.Release(renderer);
    if (!saved) LogInfo("profile edit: left without saving");
    return open;
}

}
