#include "app/title_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"

namespace sif {
namespace {

// shortcut: stage lengths and fades are guesses; the real ones are in the eboot's title code and the parts' BTL
// timelines (not decoded). Logos and the notice can be skipped with a key.
struct Stage {
    const char* part;   // a part of layout uil0001_Advertise00
    double seconds;
    bool white_bg;
};
constexpr Stage kStages[] = {
    {"uip0005_Logo00_CRI", 2.5, true},
    {"uip0006_Logo01_SQEX", 2.5, true},
    {"uip0001_Notice00", 5.0, false},
};
constexpr double kFade = 0.4;

// the title says "Press Any Key": any key except Esc, or any pad button
bool IsConfirm(const SDL_Event& e) {
    return (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode != SDL_SCANCODE_ESCAPE) ||
           e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
}

}

bool RunTitle(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames) {
    UiScreen ui;
    std::string error;
    if (!ui.Load(renderer, vfs, "ui2/01_title", lang, &error)) {
        LogError("title: {}", error);
        return false;
    }
    SDL_SetWindowTitle(window, "SIF port");
    size_t stage = 0;
    uint64_t stage_start = SDL_GetTicksNS();
    bool confirmed = false, running = true;
    for (long frame = 0; running && !confirmed && (max_frames < 0 || frame < max_frames); ++frame) {
        const double t = double(SDL_GetTicksNS() - stage_start) / 1e9;
        const bool on_title = stage >= std::size(kStages);
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)) {
                running = false;
            } else if (IsConfirm(e)) {
                if (on_title) {
                    confirmed = true;
                } else {  // skip to the next stage
                    ++stage;
                    stage_start = SDL_GetTicksNS();
                }
            }
        }
        if (!on_title && t >= kStages[stage].seconds) {
            ++stage;
            stage_start = SDL_GetTicksNS();
            continue;
        }
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        if (on_title) {
            // "Game Start" pulses (shortcut: its clip a003 is not played)
            const float pulse = float(0.55 + 0.45 * std::cos(t * 3.0));
            ui.DrawLayout(renderer, "uil0002_MainTitle00", float(std::min(1.0, t / kFade)), {{"uip0012_GameStart00", pulse}});
        } else {
            // the boot parts are placed by layout uil0001_Advertise00; show only this stage's part (and the white bg)
            const Stage& s = kStages[stage];
            std::map<std::string, float> show;
            for (const Stage& other : kStages) show[other.part] = 0;
            show["uip0016_AdvWhiteeBG00"] = s.white_bg ? 1.0f : 0.0f;
            show[s.part] = float(std::clamp(std::min(t / kFade, (s.seconds - t) / kFade), 0.0, 1.0));
            ui.DrawLayout(renderer, "uil0001_Advertise00", 1.0f, show);
        }
        renderer.EndFrame();
    }
    ui.Release(renderer);
    LogInfo("title: {}", confirmed ? "start" : "closed");
    return confirmed;
}

}
