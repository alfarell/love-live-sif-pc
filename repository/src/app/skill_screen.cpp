#include "app/skill_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "engine/ui/ui_text.h"
#include "game/text/messages.h"

namespace sif {
namespace {

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

std::string Two(const char* format, int k) {
    char s[96];
    std::snprintf(s, sizeof(s), format, k);
    return s;
}

std::string Message(const Messages& messages, int id, const char* fallback) {
    const std::string& m = messages.Get(id);
    return m.empty() ? std::string(fallback) : m;
}

// Series (the id's thousands digit): 1 live skills, 2 materials and boosters, 3 friendship, camera and shoutouts.
// skill_name_plate00 rows: purple (v 0), yellow (0.25), cyan (0.5); proc_skill_icon01 cells: mic, face, dress.
int Series(int card) { return card / 1000 % 10; }
float IconU(int card) { return Series(card) == 1 ? 0.0f : Series(card) == 3 ? 0.25f : 0.5f; }

// The 20 member tabs of col_type_tab_plate00_00 (5 x 5 cells of 0.2): µ's 1-9, Aqours 11-19, Saint Snow 21-22.
int TabCell(int member) { return member <= 9 ? member - 1 : member <= 19 ? member - 2 : member - 3; }

void LoadMessages(const Vfs& vfs, int language, Messages& messages) {
    std::string error;
    if (!messages.Load(vfs, language, &error)) LogWarn("skill cards: messages: {}", error);
}

// uip0041_CardInfoWin00 (inside uil0005_SelectInfo00) for one skill card, or empty for none
void ApplyInfo(UiScreen& ui, const std::string& info, const SkillSetup& setup, const Messages& messages, int language,
               const std::string& lang, const card::SkillCard* c, bool owned) {
    const std::string win = info + "/uip0041_CardInfoWin00";
    ui.HideNode(win, "prof_info_plate00", true);
    ui.HideInstance(info + "/uip0010_ProfileCard00_00", true);
    ui.HideInstance(info + "/ctg80_uip0002_SkillList01", true);
    ui.HideInstance(info + "/uip0020_SkillCard00_00", !c);
    ui.HideNode(win, "skill_info_plate00", !c);
    // boxes by place (labelled in a test run): 6 the green heading, 5 top right, 8 name, 9 description, 3 bottom
    // left, 4 bottom right (large); 0-2, 7, 10 and 11 belong to the profile plate or stay empty
    for (int b = 0; b < 12; ++b) ui.SetInstanceText(win, b, "");
    if (!c) return;
    ui.SetInstanceText(win, 6, Message(messages, 612, "Skill Info"));
    const int level = std::max(1, card::Level(*setup.cards, *setup.save, c->id));
    if (!ui.SetMaterialTexture(info + "/uip0020_SkillCard00_00", "skill_card00_M", SkillThumbPath(c->id, lang, false))) {
        LogWarn("skill cards: no picture for {}", c->id);
    }
    ui.SetMaterialUv(win, "skill_icon00_M", IconU(c->id), 0);
    ui.SetInstanceText(win, 5, "S-" + std::to_string(c->skill));
    ui.SetInstanceText(win, 8, c->name[size_t(language)]);
    ui.SetInstanceText(win, 9, c->text[size_t(language)]);
    ui.SetInstanceText(win, 3, c->Timed() ? "" : Message(messages, 615, "[Automatic activation]"));
    ui.SetInstanceText(win, 4, owned ? "Lv." + std::to_string(level) : "");
}

// One card cell (uil0004_CardPoint00): the skill card face (owned) or the card back
void ApplyCell(UiScreen& ui, const std::string& cell, int id, bool face_up, bool in_use, const std::string& lang) {
    for (const char* other : {"/uip0010_ProfileCard01", "/uip0022_MemberCard01", "/uip0025_ArtWork01", "/common_uip0020_CheckMark00",
                              "/uip0060_CollSelectMark00_00"}) {
        ui.HideInstance(cell + other, true);
    }
    ui.HideInstance(cell, id == 0);
    ui.HideInstance(cell + "/uip0018_CardBack01", face_up);
    ui.HideInstance(cell + "/uip0020_SkillCard01", !face_up);
    ui.HideInstance(cell + "/uip0060_CollSelectMark00_01", !in_use);  // the star of a card in use
    if (face_up) ui.SetMaterialTexture(cell + "/uip0020_SkillCard01", "skill_card00_M", SkillThumbPath(id, lang, false));
}

}

float SkillPlateV(int card) { return Series(card) == 1 ? 0.0f : Series(card) == 3 ? 0.25f : 0.5f; }

std::vector<live::Skill> LiveSkills(const SkillSetup& setup) {
    std::vector<live::Skill> out;
    if (!setup.cards || !setup.save) return out;
    const std::array<int, card::kSlots> slots = card::Slots(*setup.cards, *setup.save, setup.member);
    for (int k = 0; k < card::kSlots; ++k) {
        const card::SkillCard* c = card::FindSkill(*setup.cards, slots[size_t(k)]);
        if (!c) continue;
        const card::LevelRow& row = c->At(card::Level(*setup.cards, *setup.save, c->id));
        // the level's seconds x 1000 (eboot 0x5E89EC); the played member's cards count in full (no support share)
        out.push_back({k, c->id, c->kind, row.seconds > 0 ? double(row.seconds) * 1000.0 : 0.0, row.amount});
    }
    return out;
}

std::string SkillThumbPath(int card, const std::string& lang, bool small) {
    std::string path = small ? "ui/ui_skill_card_thumbnail_s" : "ui/ui_skill_card_thumbnail";
    if (!lang.empty()) path += "_" + lang;
    return path + ".pac/texture.pac/" + (small ? "skc_s_" : "skc_") + std::to_string(card);
}

void ApplySkillRows(UiScreen& ui, const std::string& instance, const SkillSetup& setup, const Messages& messages, int language) {
    // boxes (labelled in a test run): 6 the heading; row k: 2k the name, 2k + 1 the level
    ui.SetInstanceText(instance, 6, Message(messages, 605, "Skill Card in Use"));
    const auto slots = card::Slots(*setup.cards, *setup.save, setup.member);
    for (int k = 0; k < card::kSlots; ++k) {
        const card::SkillCard* c = slots[size_t(k)] ? card::FindSkill(*setup.cards, slots[size_t(k)]) : nullptr;
        const std::string plate = Two("skill_effect%02d", k);
        ui.HideNode(instance, plate, !c);
        if (c) ui.SetMaterialUv(instance, plate + "_M", 0, SkillPlateV(c->id));
        ui.SetInstanceText(instance, 2 * k, c ? c->name[size_t(language)] : Message(messages, 684, "Skill not set."));
        ui.SetInstanceText(instance, 2 * k + 1, c ? "Lv." + std::to_string(card::Level(*setup.cards, *setup.save, c->id)) : "");
    }
}

bool RunSkillSelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const SkillSetup& setup,
                    long max_frames) {
    route::SetScreen("skill_select");
    const int language = Language(lang);
    Messages messages;
    LoadMessages(vfs, language, messages);
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("skill select: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/40_collection", lang, &error, &text)) {
        LogError("skill select: {}", error);
        return true;
    }
    const std::string root = "uil0002_PopColl00", pop = root + "/uil0003_PopCollLayout00";
    for (const char* hidden : {"/uip0003_ItemSort00", "/common_uip0052_ButtonSquare00", "/uip0050_PopItemList00",
                               "/uil0024_PopItemList01"}) {
        ui.HideInstance(pop + hidden, true);
    }
    ui.SetMaterialUv(pop + "/uip0002_PopCollTop00", "pop_title00_M", 0, 0.5f);  // "Skill Card Selection" (row 2)
    const std::string list = pop + "/uil0021_SelectList00", info = pop + "/uil0005_SelectInfo00";
    ui.HideNode(list + "/uip0052_PopSelectListWin00", "profile_set00", true);
    // boxes (labelled): 0 the heading, 3 left of the R1 mark, 2 right of it
    ui.SetInstanceText(list + "/uip0052_PopSelectListWin00", 0, Message(messages, 605, "Skill Card in Use"));
    ui.SetInstanceText(list + "/uip0052_PopSelectListWin00", 2, Message(messages, 622, "Remove"));
    ui.SetInstanceText(list + "/uip0052_PopSelectListWin00", 3, "");
    ui.SetInstanceText(list + "/uip0052_PopSelectListWin00", 1, "");
    ui.SetMaterialUv(list + "/common_uip0005_SelectButton00", "button_text00_M", 0, 0.375f);  // "OK" (row 4)
    for (int k = 0; k < 2; ++k) ui.HideInstance(list + Two("/uip0010_ProfileCard00_%02d", k), true);

    std::vector<const card::SkillCard*> cards = card::OwnedSkills(*setup.cards, *setup.save, setup.member);
    constexpr int kCells = 30, kColumns = 6;
    std::array<int, card::kSlots>& slots = setup.save->skill_slots[setup.member];
    slots = card::Slots(*setup.cards, *setup.save, setup.member);  // drop cards no longer valid
    bool in_list = false;  // focus: the slot row (slot 0-2, 3 = OK) or the card list
    int slot = 0, pick = 0, top = 0;
    const auto show = [&] {
        // shortcut: the game scrolls the 30 cells through the window; the port fills the two rows inside it (cells
        // 06-17, as in shadPS4 s14) and pages by whole rows
        top = std::clamp(top, std::max(0, (pick / kColumns - 1) * kColumns), pick / kColumns * kColumns);
        for (int k = 0; k < kCells; ++k) {
            const std::string cell = pop + Two("/uil0020_PopItemList00/uil0004_CardPoint00_%02d", k);
            const int i = top + k - kColumns;
            const bool inside = k >= kColumns && k < 3 * kColumns;
            const int id = inside && i < int(cards.size()) ? cards[size_t(i)]->id : 0;
            // the in-use star is uip0060_CollSelectMark00_00 (seen in a test run; _01 is another mark)
            ApplyCell(ui, cell, id, true, false, lang);
            ui.HideInstance(cell + "/uip0060_CollSelectMark00_00", !(id && std::find(slots.begin(), slots.end(), id) != slots.end()));
        }
        for (int k = 0; k < card::kSlots; ++k) {
            const std::string s = list + Two("/uip0020_SkillCard00_%02d", k);
            ui.HideInstance(s, slots[size_t(k)] == 0);
            if (slots[size_t(k)]) ui.SetMaterialTexture(s, "skill_card00_M", SkillThumbPath(slots[size_t(k)], lang, false));
        }
        const card::SkillCard* focused = in_list ? (pick < int(cards.size()) ? cards[size_t(pick)] : nullptr)
                                         : slot < card::kSlots && slots[size_t(slot)] ? card::FindSkill(*setup.cards, slots[size_t(slot)])
                                                                                       : nullptr;
        ApplyInfo(ui, info, setup, messages, language, lang, focused, true);
        const bool ok = !in_list && slot == card::kSlots;
        ui.HideNode(list + "/common_uip0005_SelectButton00", "button_bg00_on", !ok);
        ui.HideNode(list + "/common_uip0005_SelectButton00", "button_effect00", !ok);
        ui.HideNode(list + "/common_uip0005_SelectButton00", "button_bg00_off", ok);
    };
    show();
    SDL_SetWindowTitle(window, "SIF port - skill card selection");
    LogInfo("skill select: member {}, {} owned cards", setup.member, cards.size());
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int n = int(cards.size());
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT:
                    if (in_list) pick = std::max(0, pick - 1);
                    else slot = std::max(0, slot - 1);
                    break;
                case SDL_SCANCODE_RIGHT:
                    if (in_list) pick = std::min(std::max(0, n - 1), pick + 1);
                    else slot = std::min(card::kSlots, slot + 1);
                    break;
                case SDL_SCANCODE_UP:
                    if (in_list) pick = std::max(0, pick - kColumns);
                    break;
                case SDL_SCANCODE_DOWN:
                    if (in_list) pick = std::min(std::max(0, n - 1), pick + kColumns);
                    break;
                case SDL_SCANCODE_SPACE:
                case SDL_SCANCODE_RETURN:
                    if (!in_list && slot == card::kSlots) {
                        fade.Leave(ui);
                    } else if (!in_list) {
                        in_list = n > 0;
                    } else if (pick < n) {
                        const int id = cards[size_t(pick)]->id;
                        for (int& s : slots) {
                            if (s == id) s = 0;  // a card sits in one slot at a time
                        }
                        slots[size_t(slot)] = id;
                        LogInfo("skill select: slot {} = {} {}", slot + 1, id, cards[size_t(pick)]->name[1]);
                        in_list = false;
                    }
                    break;
                case SDL_SCANCODE_PAGEDOWN:  // R1: remove
                    if (!in_list && slot < card::kSlots && slots[size_t(slot)]) {
                        LogInfo("skill select: slot {} removed", slot + 1);
                        slots[size_t(slot)] = 0;
                    }
                    break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    if (in_list) in_list = false;
                    else fade.Leave(ui);
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
        // the focus frame (uip0061_CollCursor00): on the list, uil0020 cells 196 apart from (204, -8), rows 241 apart
        // (PopColl00 (960, -505) + PopCollLayout (-266, 311) + cell); on a slot, uil0021 (704, -832) + (-164 + 164 k, -10)
        if (in_list) {
            const int k = pick - top + kColumns;
            ui.DrawPart(renderer, "uip0061_CollCursor00", 204.0f + 196.0f * float(k % kColumns), -8.0f - 241.0f * float(k / kColumns), 1.0f);
        } else if (slot < card::kSlots) {
            ui.DrawPart(renderer, "uip0061_CollCursor00", 540.0f + 164.0f * float(slot), -842.0f, 1.0f);
        }
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    LogInfo("skill select: set {} {} {}", slots[0], slots[1], slots[2]);
    return open;
}

bool RunSkillCollection(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                        const SkillSetup& setup, long max_frames) {
    route::SetScreen("skill_collection");
    const int language = Language(lang);
    Messages messages;
    LoadMessages(vfs, language, messages);
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("skill collection: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/40_collection", lang, &error, &text)) {
        LogError("skill collection: {}", error);
        return true;
    }
    const std::string root = "uil0001_CollLayout00", win = root + "/uil0012_GallerySkillWin00";
    ui.HideInstance(root + "/uil0011_GalleryComWin00", true);
    ui.HideInstance(root + "/uil0010_ImgPreview00", true);
    ui.HideInstance(win + "/uil0014_FavoriteSelect00", true);
    ui.SetMaterialUv(root + "/uip0001_CollTop00", "title_mode00", 0, 0.125f);  // "Collection: Skill Card" (row 2)
    ui.SetInstanceText(win + "/uip0008_SkillWin00", 0, "");
    bool aqours = setup.member >= 11;
    int tab = 0, pick = 0, top = 0;
    {
        const int first = aqours ? 11 : 1;
        tab = std::clamp(setup.member - first, 0, aqours ? 10 : 8);
    }
    constexpr int kCells = 30, kColumns = 5;  // uil0023_CollSelectList01: 6 rows of 5
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
    std::vector<const card::SkillCard*> cards;
    const auto load_tab = [&] {
        const int member = members()[size_t(tab)];
        cards.clear();
        for (const card::SkillCard& c : setup.cards->skills) {
            if (c.member == member) cards.push_back(&c);
        }
        pick = top = 0;
    };
    load_tab();
    const auto show = [&] {
        const std::vector<int> m = members();
        ui.HideInstance(root + "/common_uil0101_ComBGGroup_Member01/common_uip0400_ComTopBG_Ms00", aqours);
        ui.HideInstance(root + "/common_uil0101_ComBGGroup_Member01/common_uip0410_ComTopBG_Aqours00", !aqours);
        ui.HideNode(root + "/common_uip0008_GroupTab00", "select_group00", aqours);
        ui.HideNode(root + "/common_uip0008_GroupTab00", "select_group01", !aqours);
        for (int k = 0; k < 11; ++k) {
            const std::string t = win + Two("/uil0006_SelectTab00/uip0006_SelectTab00_%02d", k);
            ui.HideInstance(t, k >= int(m.size()));
            if (k >= int(m.size())) continue;
            const int cell = TabCell(m[size_t(k)]);
            for (const char* mat : {"coll_page_tab00_M", "coll_page_tab01_M"}) {
                ui.SetMaterialUv(t, mat, float(cell % 5) * 0.2f, float(cell / 5) * 0.2f);
            }
            ui.HideNode(t, "coll_page_tab00_on", k != tab);
            ui.HideNode(t, "coll_page_tab00_off", k == tab);
        }
        // shortcut: the game scrolls the 30 cells through the window's stencil; the port shows the three rows that sit
        // inside it (cells 05-19) and pages by whole rows; rows 00-04 (above) and 20-29 (below) stay hidden
        top = std::clamp(top, std::max(0, (pick / kColumns - 2) * kColumns), pick / kColumns * kColumns);
        for (int k = 0; k < kCells; ++k) {
            const std::string cell = win + Two("/uil0023_CollSelectList01/uil0004_CardPoint00_%02d", k);
            const int i = top + k - kColumns;
            const bool inside = k >= kColumns && k < 4 * kColumns;
            const int id = inside && i < int(cards.size()) ? cards[size_t(i)]->id : 0;
            ApplyCell(ui, cell, id, id && card::Level(*setup.cards, *setup.save, id) > 0, false, lang);
        }
        const card::SkillCard* c = pick < int(cards.size()) ? cards[size_t(pick)] : nullptr;
        const bool owned = c && card::Level(*setup.cards, *setup.save, c->id) > 0;
        ApplyInfo(ui, win + "/uil0005_SelectInfo00", setup, messages, language, lang, owned ? c : nullptr, owned);
        if (c) {
            char line[256];
            std::snprintf(line, sizeof(line), owned ? "%s" : Message(messages, 490, "(Not possessed)%s").c_str(),
                          c->name[size_t(language)].c_str());
            SDL_SetWindowTitle(window, ("SIF port - skill card: " + std::string(line)).c_str());
        }
    };
    show();
    LogInfo("skill collection: {} cards owned of {}", [&] {
        int n = 0;
        for (const card::SkillCard& c : setup.cards->skills) n += card::Level(*setup.cards, *setup.save, c.id) > 0;
        return n;
    }(), setup.cards->skills.size());
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
                case SDL_SCANCODE_PAGEUP: tab = (tab + tabs - 1) % tabs, load_tab(); break;    // L1
                case SDL_SCANCODE_PAGEDOWN: tab = (tab + 1) % tabs, load_tab(); break;         // R1
                case SDL_SCANCODE_TAB: aqours = !aqours, tab = 0, load_tab(); break;          // L2 (shortcut: Tab key)
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE: fade.Leave(ui); break;
                default: break;
            }
            show();
            if (e.key.scancode == SDL_SCANCODE_PAGEUP || e.key.scancode == SDL_SCANCODE_PAGEDOWN || e.key.scancode == SDL_SCANCODE_TAB) {
                LogInfo("skill collection: member {}, {} cards", members()[size_t(tab)], cards.size());
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
        {  // the focus frame (uip0061_CollCursor00, placed by the game, not by the layout): uil0023 at (946, -520), cells
           // 244 apart, rows 240 apart from (-732, 480); the visible rows are 1-3
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
