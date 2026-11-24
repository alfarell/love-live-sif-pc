#include "app/title_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

#include "app/fade.h"
#include "app/input.h"
#include "app/popup.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/text/messages.h"

namespace sif {
namespace {

// shortcut: stage lengths and fades are guesses; the real ones are in the eboot's title code and the parts' BTL
// timelines (not decoded). Logos and the notice can be skipped with a key.
struct Stage {
    const char* part;   // a part of layout uil0001_Advertise00
    double seconds;
    bool white_bg;
};
constexpr const char* kSaveMessage = "uil0005_SaveMessage00";
constexpr Stage kStages[] = {
    {"uip0005_Logo00_CRI", 2.5, true},
    {"uip0006_Logo01_SQEX", 2.5, true},
    {"uip0001_Notice00", 5.0, false},
    {kSaveMessage, -1, true},  // the autosave notice (691): until a key
};
constexpr double kFade = 0.4;

// The white notice (uil0005_SaveMessage00 at the centre). shortcut: the autosave feather icon under the text is not
// drawn (its part is not found in 01_title)
void SetSaveMessage(UiScreen& ui, const std::string& text) {
    ui.SetInstanceText(std::string(kSaveMessage) + "/uip0015_SaveMessageWin00", 0, text);
    ui.SetMaterialUv(std::string(kSaveMessage) + "/common_uip0005_SelectButton00", "button_text00_M", 0, 0.375f);  // "OK"
}

void DrawSaveMessage(Renderer& renderer, const UiScreen& ui, float alpha) {
    std::map<std::string, float> show;
    for (const Stage& s : kStages) show[s.part] = 0;
    show["uip0016_AdvWhiteeBG00"] = 1;
    ui.DrawLayout(renderer, "uil0001_Advertise00", 1.0f, show);
    ui.DrawLayout(renderer, kSaveMessage, alpha, 960, 540);
}

// the title says "Press Any Key": any key except Esc, or any pad button
bool IsConfirm(const SDL_Event& e) {
    return (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode != SDL_SCANCODE_ESCAPE) ||
           e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
}

}

bool RunTitle(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
              bool new_save) {
    route::SetScreen("title");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("title: text: {}", error);  // the screens still work without text
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/01_title", lang, &error, &text)) {
        LogError("title: {}", error);
        return false;
    }
    Messages messages;
    if (!messages.Load(vfs, lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0, &error)) {
        LogWarn("title: messages: {}", error);
    }
    SetSaveMessage(ui, messages.Get(691));
    Popup popup;  // "Save data creation"
    popup.Load(renderer, vfs, lang);
    // after the title is confirmed: 0 still on the title, 1 "Save data creation", 2 the online notice
    int after = 0;
    uint64_t after_start = 0;
    SDL_SetWindowTitle(window, "SIF port");
    size_t stage = 0;
    uint64_t stage_start = SDL_GetTicksNS();
    bool confirmed = false, running = true;
    ScreenFade fade;  // only its fade-out is used: the stages fade in by themselves
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; running && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        const uint64_t now = SDL_GetTicksNS();
        fade.Update(double(now - last) / 1e9);
        popup.Update(double(now - last) / 1e9);
        last = now;
        const double t = double(SDL_GetTicksNS() - stage_start) / 1e9;
        const bool on_title = stage >= std::size(kStages);
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)) {
                running = false;
            } else if (popup.IsOpen()) {
                if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && popup.Key(e.key.scancode) != Popup::kOpen) {
                    after = 2, after_start = SDL_GetTicksNS();
                    SetSaveMessage(ui, messages.Get(692));
                }
            } else if (IsConfirm(e) && !fade.leaving) {
                if (on_title && after == 0 && new_save) {
                    after = 1;
                    popup.OpenMessage(Popup::kSaveCreation, messages.Get(688), {Popup::kOk}, 0);
                    LogInfo("title: new save");
                } else if (on_title && after == 0) {
                    after = 2, after_start = SDL_GetTicksNS();
                    SetSaveMessage(ui, messages.Get(692));
                } else if (on_title) {
                    confirmed = true;
                    fade.Leave(ui);
                } else {  // skip to the next stage
                    ++stage;
                    stage_start = SDL_GetTicksNS();
                }
            }
        }
        if (!on_title && kStages[stage].seconds >= 0 && t >= kStages[stage].seconds) {
            ++stage;
            stage_start = SDL_GetTicksNS();
            continue;
        }
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        if (after == 2) {
            DrawSaveMessage(renderer, ui, float(std::min(1.0, double(SDL_GetTicksNS() - after_start) / 1e9 / kFade)));
        } else if (on_title) {
            // "Game Start" pulses (shortcut: its clip a003 is not played)
            const float pulse = float(0.55 + 0.45 * std::cos(t * 3.0));
            ui.DrawLayout(renderer, "uil0002_MainTitle00", float(std::min(1.0, t / kFade)), {{"uip0012_GameStart00", pulse}});
        } else {
            // the boot parts are placed by layout uil0001_Advertise00; show only this stage's part (and the white bg)
            const Stage& s = kStages[stage];
            if (s.seconds < 0) {
                DrawSaveMessage(renderer, ui, float(std::min(1.0, t / kFade)));
                if (fade.leaving) fade.Draw(renderer);
                renderer.EndFrame();
                continue;
            }
            std::map<std::string, float> show;
            for (const Stage& other : kStages) show[other.part] = 0;
            show["uip0016_AdvWhiteeBG00"] = s.white_bg ? 1.0f : 0.0f;
            show[s.part] = float(std::clamp(std::min(t / kFade, (s.seconds - t) / kFade), 0.0, 1.0));
            ui.DrawLayout(renderer, "uil0001_Advertise00", 1.0f, show);
        }
        popup.Draw(renderer);
        if (fade.leaving) fade.Draw(renderer);
        renderer.EndFrame();
    }
    popup.Release(renderer);
    ui.Release(renderer);
    LogInfo("title: {}", confirmed ? "start" : "closed");
    return confirmed;
}

}
