#include "app/result_screen.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>
#include <vector>

#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/live/song.h"

namespace sif {
namespace {

constexpr const char* kRoot = "uil0001_ResultTop00";
constexpr const char* kLayout = "uil0001_ResultTop00/uil0002_ResultLayout00";

}

// shortcut: the result screen's own logic (eboot) is not traced: the judgement-row labels, score rate, combo result
// and rank show their parts' defaults; the values the port knows are filled in (score, counts, max combo, title, cover)
bool RunResult(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& r,
               long max_frames) {
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("result: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/95_result", lang, &error, &text)) {
        LogError("result: {}", error);
        return true;
    }
    ui.HideNode("new00", true);
    ui.HideNode("complete00", true);
    // the play-score window: number box 0 = this score (boxes 1 and 2: Hypothesis, high score and rate; not known)
    const std::string score = std::string(kLayout) + "/uil0005_PlayScoreWin00/uip0003_PlayScore00";
    ui.SetInstanceNumber(score, 0, std::to_string(long(r.score)));
    ui.SetInstanceNumber(score, 1, std::to_string(r.max_combo));
    ui.SetInstanceNumber(score, 2, "");
    for (int k = 0; k < 3; ++k) {
        ui.HideInstance(std::string(kLayout) + "/uil0005_PlayScoreWin00/common_uip0017_New_0" + std::to_string(k), true);
    }
    // the judgement list, top to bottom (Hypothesis: Perfect .. Miss, as on the result screens seen in shadPS4)
    for (int k = 0; k < 5; ++k) {
        char path[160];
        std::snprintf(path, sizeof(path), "%s/uil0006_JudgNumberList00/uip0006_JudgNumber00_%02d", kLayout, k);
        ui.SetInstanceNumber(path, 0, std::to_string(r.counts[size_t(k)]));
        // res_judge_text00 stacks the five labels (PERFECT .. MISS) in rows of 1/5 of the texture (code-set, Hypothesis)
        ui.SetMaterialUv(path, "Judg_text00_M", 0, 0.2f * float(k));
    }
    // song data: cover and title; the "records so far" lines need a save
    std::vector<live::SongInfo> songs;
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    const std::string song_data = std::string(kLayout) + "/uip0002_SongData00";
    if (live::LoadSongList(vfs, language, songs)) {
        for (const live::SongInfo& s : songs) {
            if (s.id != r.song) continue;
            ui.SetInstanceText(song_data, 0, s.title);
            char cover[96];
            std::snprintf(cover, sizeof(cover), "ui/ui_music_thumbnail.pac/texture.pac/cd_cover_%04d", s.id);
            ui.SetMaterialTexture(song_data, "cd_jacket00_M", cover);
        }
    }
    for (int box = 1; box < 5; ++box) ui.SetInstanceText(song_data, box, "");
    ui.HideNode(song_data, "old_rate00", true);
    ui.Play("*", UiScreen::ClipName(1), false);  // "in"
    SDL_SetWindowTitle(window, "SIF port - result");
    LogInfo("result: song {:04d}/{:02d}, score {:.0f}, max combo {}", r.song, r.variant, r.score, r.max_combo);
    bool open = true, done = false;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !done && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if ((e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) done = true;
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, kRoot, 1.0f);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    return open;
}

}
