#include "app/first_start.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>

#include "app/fade.h"
#include "app/input.h"
#include "app/member_popup.h"
#include "app/navigator.h"
#include "app/popup.h"
#include "app/route.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/member/member.h"
#include "game/save/save.h"
#include "game/text/messages.h"

namespace sif {
namespace {

constexpr const char* kName = "uil0050_namaset_layout00";
constexpr const char* kNameBox = "uil0050_namaset_layout00/uil0052_nameset_window00/uip0052_nameset_textBox00";
constexpr const char* kField = "uil0052_nameset_window00";  // drawn again higher up as the name field
constexpr const char* kFieldBox = "uil0052_nameset_window00/uip0052_nameset_textBox00";
constexpr size_t kMaxName = 10;  // message 693 "Enter a name (max 10 char.)"

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

std::string WithName(std::string text, const std::string& name) {
    if (const size_t at = text.find("%s"); at != std::string::npos) text.replace(at, 2, name);
    return text;
}

}

int RunNameEntry(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, std::string& name,
                 bool can_leave, long max_frames) {
    route::SetScreen("name");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("name entry: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/01_title", lang, &error, &text)) {
        LogError("name entry: {}", error);
        return 0;
    }
    Messages messages;
    if (!messages.Load(vfs, Language(lang), &error)) LogWarn("name entry: messages: {}", error);
    Popup popup;
    popup.Load(renderer, vfs, lang);
    std::string typed = name;
    // the window's two lines are the hints 694 and 695. shortcut: the PS4 keyboard's own window is drawn as a copy of the
    // text box without its plate, higher up: "Enter a name (max 10 char.)" over the field
    ui.SetInstanceText(kNameBox, 0, messages.Get(694));
    ui.SetInstanceText(kNameBox, 1, messages.Get(695));
    ui.SetInstanceText(kFieldBox, 0, messages.Get(693));
    ui.HideNode(kFieldBox, "nama_plate00", true);
    const auto show = [&] {
        const std::string field = typed.empty() ? messages.Get(6) : typed;  // the placeholder "New Player"
        ui.SetInstanceText(kFieldBox, 1, field + (popup.IsOpen() ? "" : "_"));
    };
    show();
    SDL_StartTextInput(window);
    LogInfo("name entry: open");
    int result = 0;
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) {
                open = false, result = -1;
                continue;
            }
            if (fade.leaving) continue;
            if (e.type == SDL_EVENT_TEXT_INPUT && !popup.IsOpen()) {
                for (const char* c = e.text.text; *c;) {
                    const unsigned char b = static_cast<unsigned char>(*c);
                    const size_t n = std::min(b < 0x80 ? size_t(1) : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4, std::strlen(c));
                    if (Utf8Length(typed) < kMaxName) typed.append(c, n);
                    c += n;
                }
                show();
                continue;
            }
            if (e.type != SDL_EVENT_KEY_DOWN) continue;
            const SDL_Scancode k = e.key.scancode;
            if (popup.IsOpen()) {
                const int r = popup.Key(k);
                if (r == 1) {  // OK
                    name = typed.empty() ? messages.Get(6) : typed;
                    result = 1;
                    LogInfo("name entry: name '{}'", name);
                    fade.Leave(ui);
                } else if (r != Popup::kOpen) {  // Back or Esc: typing again
                    SDL_StartTextInput(window);
                }
                show();
                continue;
            }
            if (k == SDL_SCANCODE_BACKSPACE && !typed.empty()) {
                do typed.pop_back();
                while (!typed.empty() && (static_cast<unsigned char>(typed.back()) & 0xC0) == 0x80);
            } else if (k == SDL_SCANCODE_RETURN && !e.key.repeat) {
                SDL_StopTextInput(window);
                popup.OpenMessage(Popup::kConfirmName, WithName(messages.Get(696), typed.empty() ? messages.Get(6) : typed),
                                  {Popup::kBack, Popup::kOk}, 0);
                LogInfo("name entry: confirm '{}'", typed.empty() ? messages.Get(6) : typed);
            } else if (k == SDL_SCANCODE_ESCAPE && can_leave && !e.key.repeat) {
                LogInfo("name entry: left");
                fade.Leave(ui);
            }
            show();
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        popup.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, kName, 1.0f);
        ui.DrawLayout(renderer, kField, 1.0f, 960, 300);
        popup.Draw(renderer);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    SDL_StopTextInput(window);
    popup.Release(renderer);
    ui.Release(renderer);
    return result;
}

bool RunFirstStart(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                   save::SaveData& save, long max_frames) {
    std::string name;
    if (RunNameEntry(window, renderer, vfs, lang, name, false, max_frames) != 1) return false;
    save.player = name;

    route::SetScreen("first_member");
    MemberPopup popup;
    if (!popup.OpenFirst(renderer, vfs, lang)) return true;  // no layout: keep the new-game members
    Navigator nav;
    int shown = 0;
    bool open = true, chosen = false;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        if (popup.Focused() != shown) {  // she follows the cursor (shortcut: loaded at once, no transition)
            shown = popup.Focused();
            std::string error;
            // the group's default costume, as the top menu of a new save. Aqours' d105 matches f09; shortcut: µ's
            // members wear another stage costume in f06 (white jacket, plaid skirt; its id is not found, d019 and d031
            // are not it), the port shows d000
            if (!nav.Load(renderer, vfs, shown, -1, false, &error)) LogWarn("first member: {}", error);
            last = SDL_GetTicksNS();
        }
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            if (popup.Key(e.key.scancode) == MemberPopup::Result::Chosen) {
                const int m = popup.Chosen();
                save.mode = member::Mode(m), save.navigator[save.mode] = m;
                chosen = true;
                LogInfo("first member: {}, mode {}", m, save.mode);
                fade.leaving = true, fade.t = 0;  // the popup has no "out" clips to play
            }
        }
        const uint64_t now = SDL_GetTicksNS();
        popup.Update(double(now - last) / 1e9);
        nav.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        popup.Draw(renderer);
        renderer.Use3D();
        nav.Draw(renderer, Navigator::CostumeCamera(float(renderer.Width()) / float(std::max(1, renderer.Height()))));
        popup.DrawTicker(renderer);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    nav.Release(renderer);
    popup.Release(renderer);
    return chosen;
}

}
