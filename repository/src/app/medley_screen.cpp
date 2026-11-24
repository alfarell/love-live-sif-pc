#include "app/medley_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/popup.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "engine/ui/ui_text.h"
#include "game/live/song.h"
#include "game/text/messages.h"

namespace sif {
namespace {

const std::string kRoot = "uil0050_MedleyLayout00";
const std::string kSet = kRoot + "/uil0052_MedSetList00";
const std::string kFrame = kSet + "/uip0085_MedListFrame00";
const std::string kTab = kRoot + "/uil0053_MedTabGroup00/uip0082_MadListTab00";
const std::string kTop = kRoot + "/uip0083_MedTop00";
const std::string kSelect = kRoot + "/uil0055_MedSelectLayout00";
const std::string kPop = kSelect + "/uil0056_MedSelectPop00";
const std::string kPlate = kPop + "/uip0090_MedPopPlate00";
const std::string kTicker = kRoot + "/common_uip0001_Ticker00";

std::string Two(const std::string& prefix, int k) {
    char s[8];
    std::snprintf(s, sizeof(s), "_%02d", k);
    return prefix + s;
}
std::string ListButton(int k) { return Two(kRoot + "/uil0051_MedPresetList00/uip0080_MedSelectBut00", k); }
std::string Row(int k) { return Two(kSet + "/uip0081_MedSetListBut00", k); }
std::string PopRow(int k) { return Two(kPop + "/uip0091_MedPopSelect00", k); }

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

// A message pattern with its %d / %02d / %s filled in order (the game's printf patterns, filled without printf)
std::string Format(const std::string& pattern, std::initializer_list<std::string> values) {
    std::string out;
    auto v = values.begin();
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '%' && v != values.end()) {
            for (const char* spec : {"%02d", "%d", "%s"}) {
                if (pattern.compare(i, std::strlen(spec), spec) == 0) {
                    out += *v++;
                    i += std::strlen(spec) - 1;
                    goto next;
                }
            }
        }
        out += pattern[i];
    next:;
    }
    return out;
}

std::string Num(int n, bool two) {
    char s[8];
    std::snprintf(s, sizeof(s), two ? "%02d" : "%d", n);
    return s;
}

struct Screen {
    const Vfs& vfs;
    const medley::Data& data;
    save::SaveData& save;
    MedleyPlace& place;
    const dlc::Ownership* owned;
    const costume::Data* costumes;
    const UiText* text;
    UiScreen ui;
    Messages messages;
    std::vector<live::SongInfo> songs;
    std::vector<live::DressInfo> dresses;
    std::vector<live::StageInfo> stages;
    std::vector<std::string> member_names = std::vector<std::string>(23);

    // editing (your lists only)
    bool editing = false;
    save::MedleyList draft;
    int line = 0;
    // the Edit Slot / Change Stage panel: 0 closed, 1 Edit Slot (rows Songs, Costumes), 2 Change Stage (row Stage)
    int panel = 0, panel_row = 0;
    medley::Entry slot{};
    int stage_pick = 0;
    // Rename: the name being typed (PC keyboard; the game uses the PS4 IME)
    bool renaming = false;
    std::string typed;

    int Group() const { return save.mode == 1 ? 1 : 0; }
    const medley::SetList* Recommended(int k) const {
        const auto lists = medley::OfGroup(data, Group());
        return size_t(k) < lists.size() ? lists[size_t(k)] : nullptr;
    }
    save::MedleyList Own(int k) const {
        const auto it = save.medleys.find({Group(), k});
        return it == save.medleys.end() ? save::MedleyList{} : it->second;
    }
    std::string OwnName(int k, const save::MedleyList& l) const {
        return l.name.empty() ? Format(messages.Get(626), {Num(k + 1, false)}) : l.name;
    }
    // The list shown: (name, stage, 20 entries)
    void Shown(std::string& name, int& stage, std::array<medley::Entry, medley::kEntries>& entries) const {
        entries = {};
        if (place.tab == 1) {
            const medley::SetList* l = Recommended(place.list);
            if (!l) return;
            name = l->name[size_t(Language(Lang()))], stage = l->stage;
            for (size_t i = 0; i < l->entries.size() && i < entries.size(); ++i) entries[i] = l->entries[i];
            return;
        }
        const save::MedleyList l = editing ? draft : Own(place.list);
        name = OwnName(place.list, l), stage = l.stage > 0 ? l.stage : medley::DefaultStage(Group());
        entries = l.entries;
        if (renaming) name = typed + "_";
    }
    std::string lang_;
    const std::string& Lang() const { return lang_; }
    // A ui_text style with another alignment or colour (shadPS4 b1, g0: dark names on the unfocused plates, the list
    // lines left-aligned); shortcut: the colours are read off those pictures
    TextStyle Restyle(int id, int halign, uint32_t color) const {
        TextStyle t = text->Style(id) ? *text->Style(id) : TextStyle{};
        if (halign >= 0) t.halign = halign;
        if (color) t.color = color;
        return t;
    }

    const live::SongInfo* Song(int id) const {
        for (const auto& s : songs) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
    std::string DressName(int id) const {
        for (const auto& d : dresses) {
            if (d.id == id) return d.name;
        }
        return {};
    }
    const live::StageInfo* Stage(int model) const {
        for (const auto& s : stages) {
            if (s.model == model) return &s;
        }
        return nullptr;
    }
    // What editing offers: the group's songs (Saint Snow's with Aqours) and costumes, owned ones only
    std::vector<int> SongChoices() const {
        std::vector<int> out;
        for (const auto& s : songs) {
            if ((Group() == 0 ? s.group == 0 : s.group != 0) && (!owned || owned->Song(s.id))) out.push_back(s.id);
        }
        return out;
    }
    std::vector<int> CostumeChoices() const {
        std::vector<int> out;
        for (const auto& d : dresses) {
            // shortcut: the group's costumes by id (0-99 µ's, 100+ Aqours and Saint Snow); the game's own rule for
            // which costume a song can wear (message 640) is not traced
            if ((Group() == 0) != (d.id < 100)) continue;
            if (costumes && !costume::Owns(*costumes, save, owned, d.id)) continue;
            out.push_back(d.id);
        }
        return out;
    }

    void Apply() {
        const int group = Group();
        ui.HideInstance(kRoot + "/common_uil0100_ComBGGroup_Ms00", group != 0);
        ui.HideInstance(kRoot + "/common_uil0110_ComBGGroup_Aqours00", group != 1);
        ui.HideNode(kRoot + "/common_uip0008_GroupTab00", "select_group00", group != 0);
        ui.HideNode(kRoot + "/common_uip0008_GroupTab00", "select_group01", group != 1);
        // the header: "Set List Selection" / "Set list editing" (setup_med_title_text01_T rows of 0.25)
        for (const char* m : {"title_page01", "title_page01_M", "title_page_name00", "title_page_name00_M"}) {
            ui.SetMaterialUv(kTop, m, 0, editing ? 0.25f : 0.0f);
        }
        const bool own = place.tab == 0;
        ui.HideNode(kTab, "tab_original00_on", !own);
        ui.HideNode(kTab, "tab_original00_off", own);
        ui.HideNode(kTab, "tab_recom00_on", own);
        ui.HideNode(kTab, "tab_recom00_off", !own);
        ui.HideNode(kTab, "group_original_on", !own);
        ui.HideNode(kTab, "group_recom_on", own);
        ui.SetInstanceText(kRoot + "/uip0092_Medsetchange00", 0, messages.Get(624));  // "Switch"
        ui.HideInstance(kRoot + "/uip0086_MedSetList00", own);
        ui.SetInstanceText(kRoot + "/uip0086_MedSetList00", 0, messages.Get(625));  // "You cannot edit ..."
        // the ten lists
        for (int k = 0; k < medley::kOwnLists; ++k) {
            const std::string b = ListButton(k);
            const medley::SetList* r = Recommended(k);
            ui.SetInstanceText(b, 0, own ? OwnName(k, Own(k)) : r ? r->name[size_t(Language(Lang()))] : "");
            const bool on = k == place.list;
            ui.SetInstanceTextStyle(b, 0, Restyle(110, -1, on ? 0 : 0xFF505050));
            ui.HideNode(b, "med_button_original_on", !own || !on);
            ui.HideNode(b, "med_button_original_off", !own || on);
            ui.HideNode(b, "med_button_preset_on", own || !on);
            ui.HideNode(b, "med_button_preset_off", own || on);
            ui.HideNode(b, "med_button_eff", !on || editing);
        }
        // the chosen list
        std::string name;
        int stage = 0;
        std::array<medley::Entry, medley::kEntries> entries{};
        Shown(name, stage, entries);
        ui.SetInstanceText(kFrame, 0, name);
        const live::StageInfo* st = Stage(stage);
        ui.SetInstanceText(kFrame, 1, st ? st->name : medley::StageFolder(stage));
        if (st) {
            char thumb[80];
            std::snprintf(thumb, sizeof(thumb), "ui/ui_stage_thumbnail.pac/texture.pac/stage_%03d", st->id);
            ui.SetMaterialTexture(kFrame, "stage_img00_M", thumb);
        }
        ui.HideNode(kFrame, "group_logo_ms", group != 0);
        ui.HideNode(kFrame, "group_logo_aq", group != 1);
        ui.HideNode(kFrame, "group_logo_ss", true);
        ui.HideNode(kFrame, "back_plate_original00", !own);
        ui.HideNode(kFrame, "back_plate_preset00", own);
        for (int k = 0; k < medley::kEntries; ++k) {
            const std::string row = Row(k);
            const medley::Entry& e = entries[size_t(k)];
            const live::SongInfo* s = e.song > 0 ? Song(e.song) : nullptr;
            ui.SetInstanceText(row, 0, s ? MedleyRow(k + 1, s->title, s->unit, s->members, member_names)
                                         : Format(messages.Get(629), {Num(k + 1, true)}));
            const std::string dress = e.song > 0 && e.costume >= 0 ? DressName(e.costume) : "";
            ui.SetInstanceText(row, 1, dress.empty() ? messages.Get(630) : dress);
            ui.SetInstanceTextStyle(row, 0, Restyle(25, 0, 0));
            ui.SetInstanceTextStyle(row, 1, Restyle(25, 0, 0));
            ui.HideNode(row, "select00_eff", !editing || k != line);
            ui.HideNode(row, "music_lock00", !(e.song > 0 && owned && !owned->Song(e.song)));
            ui.HideNode(row, "costume_lock00", !(e.costume >= 0 && owned && !owned->Costume(e.costume)));
        }
        // the help line: 413 Your, 412 Recommended!, 415 editing; 416-418 in the panel
        const int help = panel == 1 ? (panel_row == 0 ? 416 : 417) : panel == 2 ? 418 : editing ? 415 : own ? 413 : 412;
        ui.SetInstanceText(kTicker, 0, messages.Get(help));
        ApplyPanel();
    }

    // The Edit Slot / Change Stage panel (uil0056_MedSelectPop00). shortcut: the game picks a song, costume or stage
    // from the card strip under it (uil0013_MusicList00, as the song select); here Left / Right on a row step through
    // the choices and the strip is not shown
    void ApplyPanel() {
        ui.HideInstance(kSelect, panel == 0);
        for (const char* part : {"/uil0013_MusicList00", "/uil0017_MenuPage00", "/common_uip0201_ComBeltBG_Member01_00",
                                 "/common_uip0201_ComBeltBG_Member01_01",
                                 "/common_uip0052_ButtonSquare00", "/uip0016_SortInfo00"}) {
            ui.HideInstance(kSelect + part, true);
        }
        ui.HideInstance(kPop + "/uil0057_MedColorChange00", true);  // shortcut: "Change to special color" not built
        if (panel == 0) return;
        ui.SetInstanceText(kPlate, 0, panel == 1 ? Format(messages.Get(632), {Num(line + 1, true)}) : messages.Get(633));
        ui.SetInstanceText(kPlate, 1, panel == 1 ? messages.Get(634) : messages.Get(636));
        ui.SetInstanceText(kPlate, 2, panel == 1 ? messages.Get(635) : "");
        ui.SetInstanceText(kPlate, 3, "");
        const int rows = panel == 1 ? 2 : 1;
        for (int r = 0; r < 3; ++r) {
            const std::string p = PopRow(r);
            ui.HideInstance(p, r >= rows);
            if (r >= rows) continue;
            const int kind = panel == 2 ? 2 : r;  // 0 song, 1 costume, 2 stage
            const bool on = r == panel_row;
            ui.HideNode(p, "select_music00_on", kind != 0 || !on);
            ui.HideNode(p, "select_music00_off", kind != 0 || on);
            ui.HideNode(p, "select_costume00_on", kind != 1 || !on);
            ui.HideNode(p, "select_costume00_off", kind != 1 || on);
            ui.HideNode(p, "select_stage00_on", kind != 2 || !on);
            ui.HideNode(p, "select_stage00_off", kind != 2 || on);
            ui.HideNode(p, "select_plate00_eff", !on);
            ui.HideNode(p, "lock_icon00", true);
            for (const char* n : {"thumbnail_img00", "thumbnail_img00_line"}) ui.HideNode(p, n, kind == 2);
            for (const char* n : {"thumbnail_stage00", "thumbnail_stage00_frame00", "thumbnail_stage00_frame01"}) {
                ui.HideNode(p, n, kind != 2);
            }
            ui.SetInstanceText(p, 2, "");
            for (int b = 0; b < 2; ++b) ui.SetInstanceTextStyle(p, b, Restyle(6, 0, on ? 0 : 0xFF000000));
            if (kind == 0) {
                const live::SongInfo* s = Song(slot.song);
                ui.SetInstanceText(p, 0, s ? s->title : messages.Get(630));
                ui.SetInstanceText(p, 1, "");
                char cover[80];
                std::snprintf(cover, sizeof(cover), "ui/ui_music_thumbnail.pac/texture.pac/cd_cover_%04d", slot.song);
                if (s) ui.SetMaterialTexture(p, "thumbnail_img00_M", cover);
            } else if (kind == 1) {
                const std::string n = slot.costume >= 0 ? DressName(slot.costume) : "";
                ui.SetInstanceText(p, 0, n.empty() ? messages.Get(630) : n);
                ui.SetInstanceText(p, 1, "");
                if (slot.costume >= 0) ui.SetMaterialTexture(p, "thumbnail_img00_M", costume::ThumbPath(slot.costume));
            } else {
                const live::StageInfo* st = Stage(stage_pick);
                ui.SetInstanceText(p, 0, "");
                ui.SetInstanceText(p, 1, st ? st->name : medley::StageFolder(stage_pick));
                if (st) {
                    char thumb[80];
                    std::snprintf(thumb, sizeof(thumb), "ui/ui_stage_thumbnail.pac/texture.pac/stage_%03d", st->id);
                    ui.SetMaterialTexture(p, "thumbnail_stage00_M", thumb);
                }
            }
        }
        ui.SetMaterialUv(kPop + "/common_uip0005_SelectButton00", "button_text00_M", 0, 3 * 0.125f);  // "OK"
        ui.HideNode(kPop + "/common_uip0005_SelectButton00", "button_bg00_on", false);
        ui.HideNode(kPop + "/common_uip0005_SelectButton00", "button_bg00_off", true);
    }

    // Steps a panel row's value by `dir` through its choices
    void Step(int dir) {
        const auto step = [&](const std::vector<int>& choices, int current) {
            if (choices.empty()) return current;
            auto it = std::find(choices.begin(), choices.end(), current);
            long i = it == choices.end() ? (dir > 0 ? -1 : 0) : long(it - choices.begin());
            i = (i + dir + long(choices.size())) % long(choices.size());
            return choices[size_t(i)];
        };
        if (panel == 2) {
            std::vector<int> models;
            for (const auto& s : stages) models.push_back(s.model);
            stage_pick = step(models, stage_pick);
        } else if (panel_row == 0) {
            slot.song = step(SongChoices(), slot.song);
            if (slot.costume < 0) {  // a new entry: the first costume, as the game prefills one (R2 g0)
                const auto c = CostumeChoices();
                if (!c.empty()) slot.costume = c.front();
            }
        } else {
            slot.costume = step(CostumeChoices(), slot.costume);
            slot.colour = 0;
        }
    }
};

}

MedleyPick RunMedleySelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                           const medley::Data& data, save::SaveData& save, MedleyPlace& place,
                           const dlc::Ownership* owned, const costume::Data* costumes, long max_frames) {
    route::SetScreen("medley");
    MedleyPick pick;
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("medley: text: {}", error);
    Screen s{vfs, data, save, place, owned, costumes, &text};
    s.lang_ = lang;
    if (!s.ui.Load(renderer, vfs, "ui2/20_setup", lang, &error, &text)) {
        LogError("medley: {}", error);
        pick.closed = false;
        return pick;
    }
    const int language = Language(lang);
    if (!s.messages.Load(vfs, language, &error)) LogWarn("medley: messages: {}", error);
    if (!live::LoadSongList(vfs, language, s.songs, &error)) LogWarn("medley: songs: {}", error);
    if (!live::LoadDressList(vfs, language, s.dresses, &error)) LogWarn("medley: costumes: {}", error);
    if (!live::LoadStageList(vfs, language, s.stages, &error)) LogWarn("medley: stages: {}", error);
    for (int m = 1; m <= 22; ++m) {
        if (m != 10 && m != 20) s.member_names[size_t(m)] = s.messages.Get(197 + (m <= 9 ? m - 1 : m <= 19 ? m - 2 : m - 3));
    }
    Popup popup;
    popup.Load(renderer, vfs, lang);
    enum class Asked { None, Start, CannotStart, Discard };
    Asked asked = Asked::None;
    place.tab = std::clamp(place.tab, 0, 1), place.list = std::clamp(place.list, 0, medley::kOwnLists - 1);
    s.Apply();
    s.ui.Play("*", UiScreen::ClipName(1), false);  // "in"
    SDL_SetWindowTitle(window, "SIF port - Live Show Medley: Set List Selection");
    LogInfo("medley select: group {}, tab {}, list {}", s.Group(), place.tab, place.list);
    const auto log_list = [&] {
        std::string name;
        int stage = 0;
        std::array<medley::Entry, medley::kEntries> entries{};
        s.Shown(name, stage, entries);
        int n = 0;
        for (const auto& e : entries) n += e.song > 0;
        LogInfo("medley select: {} list {} '{}', {} songs, stage {}", place.tab ? "recommended" : "your", place.list + 1,
                name, n, stage);
    };
    log_list();
    const auto write_own = [&] {
        if (s.draft == save::MedleyList{}) {
            save.medleys.erase({s.Group(), place.list});
        } else {
            save.medleys[{s.Group(), place.list}] = s.draft;
        }
    };
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) {
                open = false, pick.closed = true;
                continue;
            }
            if (s.renaming && e.type == SDL_EVENT_TEXT_INPUT) {  // up to 16 characters (message 627)
                for (const char* c = e.text.text; *c;) {
                    const size_t n = (static_cast<unsigned char>(*c) & 0x80) == 0 ? 1
                                     : (static_cast<unsigned char>(*c) & 0xE0) == 0xC0 ? 2
                                     : (static_cast<unsigned char>(*c) & 0xF0) == 0xE0 ? 3 : 4;
                    if (Utf8Length(s.typed) < 16) s.typed.append(c, std::min(n, std::strlen(c)));
                    c += std::min(n, std::strlen(c));
                }
                s.Apply();
                continue;
            }
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const SDL_Scancode k = e.key.scancode;
            if (s.renaming) {
                if (k == SDL_SCANCODE_BACKSPACE && !s.typed.empty()) {
                    do s.typed.pop_back();
                    while (!s.typed.empty() && (static_cast<unsigned char>(s.typed.back()) & 0xC0) == 0x80);
                } else if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_ESCAPE) {
                    if (k == SDL_SCANCODE_RETURN) {
                        save::MedleyList l = s.Own(place.list);
                        l.name = s.typed;
                        s.draft = l;
                        write_own();
                        LogInfo("medley select: list {} renamed '{}'", place.list + 1, s.typed);
                    }
                    s.renaming = false;
                    SDL_StopTextInput(window);
                }
                s.Apply();
                continue;
            }
            if (e.key.repeat && k != SDL_SCANCODE_UP && k != SDL_SCANCODE_DOWN && k != SDL_SCANCODE_LEFT &&
                k != SDL_SCANCODE_RIGHT) {
                continue;
            }
            if (popup.IsOpen()) {
                const int r = popup.Key(k);
                if (r == Popup::kOpen) continue;
                if (asked == Asked::Start && r == 1) {  // OK
                    std::string name;
                    int stage = 0;
                    std::array<medley::Entry, medley::kEntries> entries{};
                    s.Shown(name, stage, entries);
                    pick.start = true, pick.group = s.Group();
                    pick.run = {name, s.Group(), stage, std::vector<medley::Entry>(entries.begin(), entries.end())};
                    LogInfo("medley select: start '{}'", name);
                    fade.Leave(s.ui);
                } else if (asked == Asked::Discard && r == 1) {
                    s.editing = false;
                    LogInfo("medley select: edits discarded");
                }
                asked = Asked::None;
                s.Apply();
                continue;
            }
            if (s.panel) {  // Edit Slot / Change Stage
                switch (k) {
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_DOWN:
                        if (s.panel == 1) s.panel_row = 1 - s.panel_row;
                        break;
                    case SDL_SCANCODE_LEFT: s.Step(-1); break;
                    case SDL_SCANCODE_RIGHT: s.Step(1); break;
                    case SDL_SCANCODE_SPACE:
                    case SDL_SCANCODE_RETURN:  // OK
                        if (s.panel == 1) {
                            s.draft.entries[size_t(s.line)] = s.slot;
                            LogInfo("medley edit: slot {} = song {:04d}, costume {}", s.line + 1, s.slot.song, s.slot.costume);
                        } else {
                            s.draft.stage = s.stage_pick;
                            LogInfo("medley edit: stage {}", s.stage_pick);
                        }
                        s.panel = 0;
                        break;
                    case SDL_SCANCODE_ESCAPE: s.panel = 0; break;
                    default: break;
                }
                s.Apply();
                continue;
            }
            if (s.editing) {
                auto& list = s.draft.entries;
                switch (k) {
                    case SDL_SCANCODE_UP: s.line = (s.line + medley::kEntries - 1) % medley::kEntries; break;
                    case SDL_SCANCODE_DOWN: s.line = (s.line + 1) % medley::kEntries; break;
                    case SDL_SCANCODE_PAGEDOWN:  // R1 Clear
                        list[size_t(s.line)] = {};
                        LogInfo("medley edit: slot {} cleared", s.line + 1);
                        break;
                    case SDL_SCANCODE_END:  // R2 Insert: an empty slot here, the last one drops off
                        std::rotate(list.begin() + s.line, list.end() - 1, list.end());
                        list[size_t(s.line)] = {};
                        LogInfo("medley edit: slot {} inserted", s.line + 1);
                        break;
                    case SDL_SCANCODE_HOME:  // L2 Delete: the slots below move up
                        std::rotate(list.begin() + s.line, list.begin() + s.line + 1, list.end());
                        list.back() = {};
                        LogInfo("medley edit: slot {} deleted", s.line + 1);
                        break;
                    case SDL_SCANCODE_Q:  // Square Change Stage
                        s.panel = 2, s.panel_row = 0;
                        s.stage_pick = s.draft.stage > 0 ? s.draft.stage : medley::DefaultStage(s.Group());
                        break;
                    case SDL_SCANCODE_SPACE:  // Cross Modify: Edit Slot NN
                        s.panel = 1, s.panel_row = 0;
                        s.slot = list[size_t(s.line)];
                        if (s.slot.song <= 0) s.slot = {};
                        LogInfo("medley edit: slot {} open", s.line + 1);
                        break;
                    case SDL_SCANCODE_RETURN:  // touchpad Finish Editing
                        write_own();
                        s.editing = false;
                        LogInfo("medley edit: list {} saved", place.list + 1);
                        log_list();
                        break;
                    case SDL_SCANCODE_ESCAPE:  // Circle Cancel
                        if (s.draft == s.Own(place.list)) {
                            s.editing = false;
                        } else {
                            asked = Asked::Discard;
                            popup.OpenMessage(Popup::kConfirm, s.messages.Get(637), {Popup::kCancel, Popup::kOk}, 0);
                        }
                        break;
                    default: break;
                }
                s.Apply();
                continue;
            }
            switch (k) {
                case SDL_SCANCODE_UP: place.list = (place.list + medley::kOwnLists - 1) % medley::kOwnLists; break;
                case SDL_SCANCODE_DOWN: place.list = (place.list + 1) % medley::kOwnLists; break;
                case SDL_SCANCODE_PAGEUP:
                case SDL_SCANCODE_PAGEDOWN: place.tab = 1 - place.tab; break;  // L1 / R1 Switch
                case SDL_SCANCODE_HOME:  // L2 Toggle Mode (µ's / Aqours)
                    save.mode = 1 - s.Group();
                    LogInfo("medley select: group {}", s.Group());
                    break;
                case SDL_SCANCODE_Q:  // Square Rename List (your lists)
                    if (place.tab == 0) {
                        s.renaming = true;
                        s.typed = s.Own(place.list).name;
                        SDL_StartTextInput(window);
                        LogInfo("medley select: rename list {}", place.list + 1);
                    }
                    break;
                case SDL_SCANCODE_SPACE:  // Cross Edit List (your lists)
                    if (place.tab == 0) {
                        s.editing = true, s.line = 0;
                        s.draft = s.Own(place.list);
                        LogInfo("medley edit: list {}", place.list + 1);
                    }
                    break;
                case SDL_SCANCODE_RETURN: {  // touchpad Start Live Show
                    std::string name;
                    int stage = 0;
                    std::array<medley::Entry, medley::kEntries> entries{};
                    s.Shown(name, stage, entries);
                    const bool any = std::any_of(entries.begin(), entries.end(), [&](const medley::Entry& x) {
                        return x.song > 0 && s.Song(x.song) && (!owned || owned->Song(x.song));
                    });
                    const std::string quoted = Format(s.messages.Get(641), {name});
                    if (any) {
                        asked = Asked::Start;  // "Confirmation of medley start": Back / OK, Back focused (shadPS4 b3)
                        popup.OpenMessage(Popup::kMedleyStart, quoted + "\n" + s.messages.Get(642), {Popup::kBack, Popup::kOk}, 0);
                    } else {
                        asked = Asked::CannotStart;  // "Since no songs can be used, "name" cannot start." (643-645)
                        popup.OpenMessage(Popup::kConfirm,
                                          s.messages.Get(643) + "\n" + Format(s.messages.Get(644), {name}) + "\n" +
                                              s.messages.Get(645),
                                          {Popup::kOk}, 0);
                    }
                    LogInfo("medley select: {}", any ? "start?" : "cannot start");
                    break;
                }
                case SDL_SCANCODE_ESCAPE:  // Circle Back
                    fade.Leave(s.ui);
                    break;
                default: break;
            }
            if (k == SDL_SCANCODE_UP || k == SDL_SCANCODE_DOWN || k == SDL_SCANCODE_PAGEUP || k == SDL_SCANCODE_PAGEDOWN ||
                k == SDL_SCANCODE_HOME) {
                log_list();
            }
            s.Apply();
        }
        const uint64_t now = SDL_GetTicksNS();
        s.ui.Update(double(now - last) / 1e9);
        popup.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        s.ui.DrawLayout(renderer, kRoot, 1.0f);
        popup.Draw(renderer);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    if (s.renaming) SDL_StopTextInput(window);
    s.ui.Release(renderer);
    popup.Release(renderer);
    if (!open) pick.closed = true;
    return pick;
}

}
