#include "app/result_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/live/song.h"

namespace sif {
namespace {

constexpr const char* kRoot = "uil0001_ResultTop00";
constexpr const char* kLayout = "uil0001_ResultTop00/uil0002_ResultLayout00";

}

// shortcut: the result screen's own logic (eboot) is not traced: the combo result (FULL COMBO), records and rewards
// show their parts' defaults; the values the port knows are filled in (score, counts, max combo, clear rate, rank,
// title, cover)
bool RunResult(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& r,
               long max_frames, const save::NewBests& news) {
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
    // the play-score window: number boxes 0 score, 1 max combo, 2 clear rate (its digit row has '.' and '%'; the
    // two-decimal format is a guess)
    const std::string score = std::string(kLayout) + "/uil0005_PlayScoreWin00/uip0003_PlayScore00";
    ui.SetInstanceNumber(score, 0, std::to_string(long(r.score)));
    ui.SetInstanceNumber(score, 1, std::to_string(r.max_combo));
    char rate[16];
    std::snprintf(rate, sizeof(rate), "%.2f%%", std::floor(r.clear_rate * 10000) / 100);
    ui.SetInstanceNumber(score, 2, rate);
    // the rank letter: res_result_evaluation00 holds D C B A over S SS SSS in cells of 1/4 x 1/2
    const std::string rank = std::string(kLayout) + "/uil0007_ScoreRateGroup00/uip0005_ScoreRate00";
    for (const char* material : {"score_rate_M", "score_rate_eff_M"}) {
        ui.SetMaterialUv(rank, material, 0.25f * float(r.rank % 4), 0.5f * float(r.rank / 4));
    }
    // the FULL COMBO banner: only when nothing broke the combo (Good, Bad and Miss do)
    if (r.counts[2] + r.counts[3] + r.counts[4] > 0) ui.HideInstance(std::string(kLayout) + "/uip0004_ComboResult00", true);
    // "NEW" by the score, max combo and clear rate rows (Hypothesis: New_00..02 follow the rows top to bottom)
    const bool is_new[3] = {news.score, news.combo, news.rate};
    for (int k = 0; k < 3; ++k) {
        ui.HideInstance(std::string(kLayout) + "/uil0005_PlayScoreWin00/common_uip0017_New_0" + std::to_string(k), !is_new[k]);
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
            // difficulty: res_difficulty_level00 has the song select's label order (4 x 4: home BEGINNER .. ADVENTURE,
            // then EASY(AC) .. CHALLENGE(AC)); res_difficulty_star_plate00 one colour per arcade difficulty (5 x 1);
            // the level badge is 00_common's (cell = level - 1). Variants 00-04 are difficulty indexes 0-4, 10-13 are 5-8
            const bool home = r.variant >= 10;
            const int label = home ? r.variant - 10 : 4 + r.variant, index = home ? r.variant - 5 : r.variant;
            const int level = index >= 0 && index < 9 ? s.stars[size_t(index)] : 0;
            const int cell = std::clamp(level, 1, 15) - 1;
            const std::string window = std::string(kLayout) + "/uil0005_PlayScoreWin00";
            for (const std::string& part : {song_data, score}) {
                ui.SetMaterialUv(part, "diff_plate00_M", 0.25f * float(label % 4), 0.25f * float(label / 4));
            }
            ui.SetMaterialUv(score, "star_bg00_M", 0.2f * float(std::clamp(home ? r.variant - 10 : r.variant, 0, 4)), 0);
            ui.SetMaterialUv(window + "/common_uip0010_DifficultyStar00", "difficulty_star00", 0.25f * float(cell % 4),
                             0.25f * float(cell / 4));
        }
    }
    for (int box = 1; box < 5; ++box) ui.SetInstanceText(song_data, box, "");
    ui.HideNode(song_data, "old_rate00", true);
    ui.Play("*", UiScreen::ClipName(1), false);  // "in"
    SDL_SetWindowTitle(window, "SIF port - result");
    LogInfo("result: song {:04d}/{:02d}, score {:.0f}, max combo {}, clear rate {}, rank {}", r.song, r.variant, r.score,
            r.max_combo, rate, r.rank);
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if ((e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) fade.Leave(ui);
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
