#include "app/medley_live.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/live_screen.h"
#include "app/result_screen.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "engine/ui/ui_text.h"
#include "game/live/song.h"
#include "game/text/messages.h"

namespace sif {

namespace {

constexpr const char* kEnd = "uil0030_MedleyEndLayout00";
const std::string kList = std::string(kEnd) + "/uil0031_MedSetList00";

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

}

std::string MedleyRow(int number, const std::string& title, int unit, const std::vector<int>& members,
                      const std::vector<std::string>& member_names) {
    // shortcut: the game's own singer text is not traced (eboot FUN_00157af0 formats "%02d.%s / %s"); this matches its
    // rows in shadPS4 (end2): the group's name for a whole-group song, else the members' names
    std::string singers = unit == 0 ? "μ's" : unit == 4 ? "Aqours" : unit == 10 ? "Saint Snow" : "";
    const bool group = !singers.empty();
    for (const int m : members) {
        if (group || m < 0 || size_t(m) >= member_names.size() || member_names[size_t(m)].empty()) continue;
        if (!singers.empty()) singers += " ";
        singers += member_names[size_t(m)];
    }
    char head[8];
    std::snprintf(head, sizeof(head), "%02d.", number);
    return head + title + " / " + singers;
}

MedleyOutcome RunMedley(SDL_Window* window, Renderer& renderer, AudioOutput& audio, const Vfs& vfs,
                        const AppSettings& settings, const LiveOptions& base, const MedleyRun& run, long max_frames) {
    MedleyOutcome out;
    std::vector<medley::Entry> entries;
    for (const medley::Entry& e : run.entries) {
        if (e.song > 0) entries.push_back(e);
    }
    const std::string stage = medley::StageFolder(run.stage > 0 ? run.stage : medley::DefaultStage(run.group));
    LogInfo("medley: '{}', {} songs on {}", run.name, entries.size(), stage);
    LoadingScreen loading;
    loading.Load(renderer, vfs, base.lang);
    std::vector<bool> failed(entries.size(), false);
    bool played = false;  // a song has played: the next one starts with the title
    for (size_t i = 0; i < entries.size() && !out.closed;) {
        const medley::Entry& e = entries[i];
        LiveOptions opt = base;
        opt.song = e.song;
        opt.variant = 11;  // shortcut: a view-mode live needs a chart only for its length; the home chart as View Live Show
        opt.view = opt.medley = true;
        opt.member = 0;  // every member who dances the song
        opt.stage = stage;
        opt.skills.clear();
        opt.wear.clear();
        char dress[16];
        std::snprintf(dress, sizeof(dress), "d%03d", std::max(0, e.costume));
        opt.costume = e.costume >= 0 ? dress : "";
        if (e.costume >= 0 && e.colour > 0) {  // the special colour: chr/cNN/dNNN/dNNN_vC.pac where a member has it
            char special[24];
            std::snprintf(special, sizeof(special), "d%03d_v%d", e.costume, e.colour);
            for (int m = 1; m <= 22; ++m) opt.wear[m] = special;
        }
        // L1 / R1 reach the nearest song that loads; R1 on the last song does nothing (shadPS4 R2)
        const auto next = [&](size_t from, int dir) -> long {
            for (long k = long(from) + dir; k >= 0 && k < long(entries.size()); k += dir) {
                if (!failed[size_t(k)]) return k;
            }
            return -1;
        };
        opt.medley_prev = next(i, -1) >= 0;
        opt.medley_next = next(i, 1) >= 0;
        opt.medley_title = played;  // the title between songs, not before the first
        opt.max_frames = max_frames;
        LogInfo("medley: song {} of {}: {:04d}, costume {}{}", i + 1, entries.size(), e.song, opt.costume,
                e.colour ? ", colour " + std::to_string(e.colour) : std::string());
        LiveResult result;
        loading.Show(renderer);
        if (RunLive(window, renderer, audio, vfs, settings, opt, &result) != 0) {
            LogWarn("medley: song {:04d} skipped (cannot be loaded)", e.song);
            failed[i] = true;
            const long k = next(i, 1);
            if (k < 0) break;
            i = size_t(k);
            continue;
        }
        out.closed = result.closed;
        played = true;
        out.seconds += result.played_ms / 1000.0;
        if (result.finished) out.watched.push_back(e);
        if (result.skip != 0) {
            const long k = next(i, result.skip);
            if (k >= 0) i = size_t(k);
            continue;
        }
        if (!result.finished) break;  // Esc: the medley ends here
        const long k = next(i, 1);
        if (k < 0) {
            out.finished = true;
            break;
        }
        i = size_t(k);
    }
    loading.Release(renderer);
    LogInfo("medley end: {}, {:.0f} s, {} songs played to their end", out.closed ? "closed" : out.finished ? "finished" : "left",
            out.seconds, out.watched.size());
    return out;
}

bool RunMedleyEnd(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const MedleyRun& run,
                  long max_frames, MedleyOutcome& out) {
    route::SetScreen("medley_end");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("medley end: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/95_result", lang, &error, &text)) {
        LogError("medley end: {}", error);
        return true;
    }
    const int language = Language(lang);
    Messages messages;
    if (!messages.Load(vfs, language, &error)) LogWarn("medley end: messages: {}", error);
    const bool aqours = run.group == 1;
    ui.HideInstance(std::string(kEnd) + "/common_uil0100_ComBGGroup_Ms00", aqours);
    ui.HideInstance(std::string(kEnd) + "/common_uil0110_ComBGGroup_Aqours00", !aqours);
    ui.HideNode(std::string(kEnd) + "/common_uip0008_GroupTab00", "select_group00", aqours);
    ui.HideNode(std::string(kEnd) + "/common_uip0008_GroupTab00", "select_group01", !aqours);
    // shortcut: the navigator member in 3D on the right (uip0053_MedChara00, a render target) is not drawn, as on View
    // Live Show's end screen
    ui.HideInstance(std::string(kEnd) + "/uip0053_MedChara00", true);
    ui.SetInstanceText(kList + "/uip0052_MedSetListTitle00", 0, run.name);
    ui.SetInstanceText(std::string(kEnd) + "/common_uip0001_Ticker00", 0, messages.Get(426));  // "The Live Show has ended."
    // member names: messages 197-216 in the order µ's 1-9, Aqours 11-19, Saint Snow 21-22
    std::vector<std::string> names(23);
    for (int m = 1; m <= 22; ++m) {
        if (m == 10 || m == 20) continue;
        names[size_t(m)] = messages.Get(197 + (m <= 9 ? m - 1 : m <= 19 ? m - 2 : m - 3));
    }
    std::vector<live::SongInfo> songs;
    if (!live::LoadSongList(vfs, language, songs, &error)) LogWarn("medley end: songs: {}", error);
    int n = 0;
    for (const medley::Entry& e : run.entries) {
        if (e.song <= 0 || n >= medley::kEntries) continue;
        std::string row;
        for (const live::SongInfo& s : songs) {
            if (s.id == e.song) row = MedleyRow(n + 1, s.title, s.unit, s.members, names);
        }
        char path[96];
        std::snprintf(path, sizeof(path), "%s/uip0050_MedSongTitle00_%02d", kList.c_str(), n);
        ui.SetInstanceText(path, 0, row);
        LogInfo("medley end: row {}", row);
        ++n;
    }
    for (int k = n; k < medley::kEntries; ++k) {
        char path[96];
        std::snprintf(path, sizeof(path), "%s/uip0050_MedSongTitle00_%02d", kList.c_str(), k);
        ui.HideInstance(path, true);
    }
    // the arrow to the second column: only with more than 10 songs (shortcut: from the layout, not traced)
    ui.HideInstance(kList + "/uip0054_MedListArrow00", n <= 10);
    ui.Play("*", UiScreen::ClipName(1), false);  // "in"
    SDL_SetWindowTitle(window, "SIF port - Live Show Medley: end the live show");
    LogInfo("medley end: '{}', {} songs", run.name, n);
    // shortcut: the game's OPTIONS "What will you do next?" box (570, 571, 566) is not built yet (R5 builds the
    // medley's popups); its choices are keys here: OPTIONS (Enter) Watch the Same Medley List, Cross (Space, "End")
    // Return to Medley Selection, Circle (Esc) Return to Main Menu
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int choice = e.key.scancode == SDL_SCANCODE_RETURN ? 0
                               : e.key.scancode == SDL_SCANCODE_SPACE ? 1
                               : e.key.scancode == SDL_SCANCODE_ESCAPE ? 2
                                                                       : -1;
            if (choice < 0) continue;
            out.choice = choice;
            LogInfo("medley end: choice {} ({})", choice, messages.Get(choice == 0 ? 570 : choice == 1 ? 571 : 566));
            fade.Leave(ui);
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, kEnd, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    out.closed = out.closed || !open;
    return open;
}

}
