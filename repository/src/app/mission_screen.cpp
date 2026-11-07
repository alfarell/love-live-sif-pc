#include "app/mission_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>

#include "app/fade.h"
#include "app/input.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"

namespace sif {
namespace {

constexpr const char* kRoot = "uil0001_MisLayout00";
// uil0003_MisplateLayout00 places plates _00 .. _06; _00 sits above the list frame and _06 below it (the game scrolls
// them through the frame's stencil), so the port fills the 5 inside: shortcut: whole-row steps, no smooth scroll
constexpr int kFirstPlate = 1, kPlates = 5;
// eve_mis_gauge00_01 fills u 162..862 of 1024
constexpr float kGaugeU0 = 162.0f / 1024.0f, kGaugeU1 = 862.0f / 1024.0f;

// The tabs (uip0006_missonPageTab00 label atlas eve_mis_type_tab_text00_00_T, rows of 1/8: Member, Group, Songs,
// Costumes) and the frame layout drawn for each active tab (uil0002_MisframeLayout00 places all four)
constexpr int kTabs = 4;
constexpr std::array<const char*, kTabs> kTabLayouts = {"uil0006_missipntabOshi00", "uil0007_missipntabCom00",
                                                        "uil0008_missipntabMusic00", "uil0009_missipntabDress00"};
// The tab of a condition kind: member (recommended table, 20-34), group (common, 0-19), songs (35-39), costumes (40-48)
int TabOf(int kind) { return kind >= 40 ? 3 : kind >= 35 ? 2 : kind >= 20 ? 0 : 1; }

void ApplyTabs(UiScreen& ui, int active) {
    for (int v = 0; v < kTabs; ++v) {
        const std::string layout = std::string(kRoot) + "/uil0002_MisframeLayout00/" + kTabLayouts[size_t(v)];
        ui.HideInstance(layout, v != active);
        for (int k = 0; k < kTabs; ++k) {
            char holder[64];
            std::snprintf(holder, sizeof(holder), "/uil0005_missipntabAll00_%02d", k);
            const std::string tab = layout + holder + "/uip0006_missonPageTab00";
            ui.SetMaterialUv(tab, "M_tab_text00", 0.0f, 0.125f * float(k));
            ui.HideNode(tab, "missontab00_ON", k != active);
            ui.HideNode(tab, "missontab00_OFF", k == active);
            // boxes 0/1: the tab's reward count and its label (rewards); shortcut: rewards are not claimed, so empty
            ui.SetInstanceText(tab, 0, "");
            ui.SetInstanceText(tab, 1, "");
            ui.HideInstance(layout + holder + "/common_uip0017_New00", true);
        }
    }
}

std::string Holder(int i) {
    char s[128];
    std::snprintf(s, sizeof(s), "%s/uil0003_MisplateLayout00/uil0004_missionplateAll00_%02d", kRoot, i);
    return s;
}

struct Row {
    const mission::Mission* m = nullptr;
    std::string title, text;
    mission::Progress progress;
    const mission::Award* accolade = nullptr;
};

void Apply(UiScreen& ui, const std::vector<Row>& rows, int top, int cursor) {
    for (int i : {0, 6}) ui.HideInstance(Holder(i), true);
    for (int i = 0; i < kPlates; ++i) {
        const std::string holder = Holder(kFirstPlate + i), plate = holder + "/uip0001_mission_plate00";
        const int r = top + i;
        ui.HideInstance(holder, r >= int(rows.size()));
        if (r >= int(rows.size())) continue;
        const Row& row = rows[size_t(r)];
        const bool done = row.progress.Done(), focused = r == cursor;
        // text boxes (probed): 0 title, 2 description, 3 progress, 4 reward count, 5 "press to claim"; 1, 6, 7 unused
        ui.SetInstanceText(plate, 0, row.title);
        ui.SetInstanceText(plate, 2, row.text);
        const int need = row.progress.need, have = std::clamp(row.progress.have, 0, need);
        // not tracked by the port yet: "-" and an empty gauge
        ui.SetInstanceText(plate, 3,
                           (row.progress.have < 0 ? std::string("-") : std::to_string(have)) + "/" + std::to_string(need));
        ui.SetInstanceText(plate, 4, row.accolade ? "" : "x" + std::to_string(row.m->reward_amount));
        for (int b : {1, 5, 6, 7}) ui.SetInstanceText(plate, b, "");
        ui.SetMaterialClip(plate, "M_misgauge00", kGaugeU0 + (kGaugeU1 - kGaugeU0) * float(have) / float(need));
        ui.HideNode(plate, "status_icon00", !done);  // COMPLETED
        ui.HideNode(plate, "status_icon01", true);   // GET! shortcut: rewards are not claimed or granted
        ui.HideNode(plate, "mission_plate00_ON", !focused);
        ui.HideNode(plate, "mission_plate00_OFF", focused);
        ui.HideNode(plate, "mission_plate00_ef", !focused);
        // shortcut: no reward picture (accolade plates, item, card and costume icons come from other packages, e.g.
        // 03_card title_plate_NNNN, not loaded here)
        for (const char* node : {"reward_iconA00", "reward_iconB00", "reward_iconC00", "reward_icon_name00",
                                 "reward_icon_title00", "reward_icon_frame00"}) {
            ui.HideNode(plate, node, true);
        }
        ui.HideInstance(holder + "/common_uip0017_New00", true);
        ui.HideInstance(holder + "/common_uip0051_ButtonCircle00", true);
    }
}

}

bool RunMissions(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const MissionData& data,
                 long max_frames) {
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("missions: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/50_event", lang, &error, &text)) {
        LogError("missions: {}", error);
        return true;
    }
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    std::vector<live::DressInfo> dresses;
    if (!live::LoadDressList(vfs, language, dresses, &error)) LogWarn("missions: costumes: {}", error);
    std::array<std::vector<Row>, kTabs> tabs;
    if (data.missions && data.save) {
        for (const mission::Mission& m : data.missions->rows) {
            if (size_t(m.Kind()) >= data.missions->conditions.size()) continue;
            const int tab = TabOf(m.Kind());
            if (tab == 2 && data.owned && !data.owned->Song(m.p1)) continue;     // no add-on for the song
            if (tab == 3 && data.owned && !data.owned->Costume(m.p1)) continue;  // or the costume
            Row row{&m, {}, {}, mission::ProgressOf(m, *data.save)};
            // [p1]: song title, costume name; members by number (shortcut: member names are not read yet)
            std::string p1 = std::to_string(m.p1);
            if (tab == 2 && data.songs) {
                for (const auto& s : *data.songs) {
                    if (s.id == m.p1) p1 = s.title;
                }
            }
            for (const auto& d : dresses) {
                if (tab == 3 && d.id == m.p1) p1 = d.name;
            }
            const auto& c = data.missions->conditions[size_t(m.Kind())];
            row.title = mission::Fill(c.title[size_t(language)], m, p1);
            row.text = mission::Fill(c.text[size_t(language)], m, p1);
            if (m.reward_type == 4 && data.accolades) row.accolade = mission::FindAward(*data.accolades, m.reward_id);
            tabs[size_t(tab)].push_back(std::move(row));
        }
    }
    LogInfo("missions: {} member, {} group, {} song, {} costume", tabs[0].size(), tabs[1].size(), tabs[2].size(),
            tabs[3].size());
    int tab = 1, cursor = 0, top = 0;  // start on Group (the general missions)
    ApplyTabs(ui, tab);
    Apply(ui, tabs[size_t(tab)], top, cursor);
    SDL_SetWindowTitle(window, "SIF port - task list");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const int n = int(tabs[size_t(tab)].size());
            const int old_tab = tab;
            switch (e.key.scancode) {
                case SDL_SCANCODE_ESCAPE: fade.Leave(ui); break;
                case SDL_SCANCODE_UP: cursor = std::max(0, cursor - 1); break;
                case SDL_SCANCODE_DOWN: cursor = std::min(std::max(0, n - 1), cursor + 1); break;
                case SDL_SCANCODE_PAGEUP: cursor = std::max(0, cursor - kPlates); break;
                case SDL_SCANCODE_PAGEDOWN: cursor = std::min(std::max(0, n - 1), cursor + kPlates); break;
                case SDL_SCANCODE_LEFT: tab = (tab + kTabs - 1) % kTabs; break;
                case SDL_SCANCODE_RIGHT: tab = (tab + 1) % kTabs; break;
                default: break;
            }
            if (tab != old_tab) cursor = top = 0, ApplyTabs(ui, tab);
            top = std::clamp(top, std::max(0, cursor - kPlates + 1), cursor);
            Apply(ui, tabs[size_t(tab)], top, cursor);
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
    return open;
}

}
