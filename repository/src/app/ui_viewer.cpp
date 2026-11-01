// Developer view of one ui2 layout: --ui SCREEN:LAYOUT[:CLIP], e.g. --ui 10_top_menu:uil0001_TopMenu00:a003.
// Draws the layout (nested layouts and borrowed parts included) and loops CLIP on every part that has it.
#include "app/ui_viewer.h"

#include <SDL3/SDL.h>

#include <string>

#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"

namespace sif {

int RunUiViewer(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& spec, const std::string& lang,
                long max_frames) {
    const size_t a = spec.find(':'), b = a == std::string::npos ? a : spec.find(':', a + 1);
    const std::string screen = "ui2/" + spec.substr(0, a);
    const std::string layout = a == std::string::npos ? "" : spec.substr(a + 1, b == std::string::npos ? b : b - a - 1);
    const std::string clip = b == std::string::npos ? "" : spec.substr(b + 1);
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("ui viewer: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, screen, lang, &error, &text)) {
        LogError("ui viewer: {}", error);
        return 1;
    }
    std::string layouts;
    for (const std::string& l : ui.Layouts()) layouts += " " + l;
    LogInfo("ui viewer: {} layouts:{}", screen, layouts);
    if (!clip.empty()) ui.Play("*", clip, true);
    SDL_SetWindowTitle(window, ("SIF port - ui " + spec).c_str());
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; max_frames < 0 || frame < max_frames; ++frame) {
        SDL_Event e;
        bool quit = false;
        while (SDL_PollEvent(&e)) {
            quit |= e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE);
        }
        if (quit) break;
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, layout, 1.0f);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    return 0;
}

}
