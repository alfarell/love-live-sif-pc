#include "app/mission_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
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
        const int need = row.progress.need, have = std::min(row.progress.have, need);
        ui.SetInstanceText(plate, 3, std::to_string(have) + "/" + std::to_string(need));
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
    std::vector<Row> rows;
    if (data.missions && data.save) {
        for (const mission::Mission& m : data.missions->rows) {
            if (size_t(m.Kind()) >= data.missions->conditions.size()) continue;
            const bool per_song = m.Kind() >= 35 && m.Kind() <= 39;
            if (per_song && data.owned && !data.owned->Song(m.p1)) continue;
            Row row{&m, {}, {}, mission::ProgressOf(m, *data.save)};
            if (row.progress.have < 0) continue;  // shortcut: only the kinds the port tracks are listed
            std::string p1 = std::to_string(m.p1);
            if (per_song && data.songs) {
                for (const auto& s : *data.songs) {
                    if (s.id == m.p1) p1 = s.title;
                }
            }
            const auto& c = data.missions->conditions[size_t(m.Kind())];
            row.title = mission::Fill(c.title[size_t(language)], m, p1);
            row.text = mission::Fill(c.text[size_t(language)], m, p1);
            if (m.reward_type == 4 && data.accolades) row.accolade = mission::FindAward(*data.accolades, m.reward_id);
            rows.push_back(std::move(row));
        }
    }
    LogInfo("missions: {} listed", rows.size());
    int cursor = 0, top = 0;
    Apply(ui, rows, top, cursor);
    SDL_SetWindowTitle(window, "SIF port - task list");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const int n = int(rows.size());
            switch (e.key.scancode) {
                case SDL_SCANCODE_ESCAPE: fade.Leave(ui); break;
                case SDL_SCANCODE_UP: cursor = std::max(0, cursor - 1); break;
                case SDL_SCANCODE_DOWN: cursor = std::min(std::max(0, n - 1), cursor + 1); break;
                case SDL_SCANCODE_LEFT: cursor = std::max(0, cursor - kPlates); break;  // a page; shortcut: the
                case SDL_SCANCODE_RIGHT: cursor = std::min(std::max(0, n - 1), cursor + kPlates); break;  // game's tabs
                // (all / member / common / song / costume, uil0005-0009) are not wired; Left/Right page instead
                default: break;
            }
            top = std::clamp(top, std::max(0, cursor - kPlates + 1), cursor);
            Apply(ui, rows, top, cursor);
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
