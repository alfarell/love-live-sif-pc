#include "app/options_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/route.h"
#include "engine/audio/audio_output.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/text/messages.h"

namespace sif {
namespace {

// A row: a label plate and a value, both by placed-part path. Sound rows are one part (uip0021_sound_param00:
// boxes 0/1 label, 2/3 value, a volume gauge); game rows are a plate (uip0002_opt_setplate00: boxes 0/1) and a value
// box (uip0040_gameOpt_box00: boxes 0/1). Every text appears in two boxes (Hypothesis: normal and highlighted
// styles), so both are set.
struct Row {
    std::string plate, box;  // box empty: the value is in the plate (sound rows)
    std::string label;
};

void SetText(UiScreen& ui, const std::string& part, int first_box, const std::string& text) {
    ui.SetInstanceText(part, first_box, text);
    ui.SetInstanceText(part, first_box + 1, text);
}

void Focus(UiScreen& ui, const Row& row, bool on) {
    for (const char* node : {"select_plate_on", "select_eff00_on", "menu_borderline00_on", "volume_gauge_icon00_on",
                             "volume_gauge_icon01_on"}) {
        ui.HideNode(row.plate, node, !on);
    }
    for (const char* node : {"menu_borderline00_off", "volume_gauge_icon00_off", "volume_gauge_icon01_off"}) {
        ui.HideNode(row.plate, node, on);
    }
    if (row.box.empty()) return;
    for (const char* node : {"opt_box00_on", "arrow00_left_on", "arrow00_right_on"}) ui.HideNode(row.box, node, !on);
    for (const char* node : {"opt_box00_off", "arrow00_left_off", "arrow00_right_off"}) ui.HideNode(row.box, node, on);
}

// The Button Controls ring (uil0011_keyConfigLayout00 > uil0012_keySetting00): its "button_9" group has 10 button plates
// (uip0031_controllButton00_00-09) and 9 lane rings (uip0032_controllRing00_00-08); "button_6" (_10-_15) is the 6-lane
// form. The plate's icon is a 1/4 x 1/4 cell of opt_ope_ingame_button00 in PadButton order.
// `home`: the 6-lane form for one home config (plates and rings _10-_15, M10 K4), else the 9-lane AC form.
bool RunButtons(SDL_Window* window, Renderer& renderer, UiScreen& ui, AppSettings& settings, long max_frames,
                bool home = false, HomeConfig config = HomeConfig::Easy) {
    route::SetScreen("controls");
    const std::string root = "uil0011_keyConfigLayout00", ring = root + "/uil0012_keySetting00/";
    const auto slot_path = [&](int i) {
        char s[64];
        std::snprintf(s, sizeof(s), "uip0031_controllButton00_%02d", i);
        return ring + s;
    };
    const auto ring_path = [&](int i) {
        char s[64];
        std::snprintf(s, sizeof(s), "uip0032_controllRing00_%02d", i);
        return ring + s;
    };
    ui.SetMaterialUv(root + "/uil0001_option_set00/uip0001_opt_title_text00", "M_opt_title_text00", 0, 0.375f);
    // the game's 6-lane form is headed "Live Show Player Controller Settings" (work/m7/kc_other.png)
    SetText(ui, root + "/uip0020_menutitle_plate00", 0, home ? "Live Show Player Controller Settings" : "Live Show (AC)");
    // shortcut: Reset and OK are keys (T, Enter), not focusable buttons
    for (const char* b : {"common_uip0005_SelectButton00_00", "common_uip0005_SelectButton00_01"}) {
        ui.HideInstance(root + "/" + b, true);
    }
    for (int i = 0; i < 16; ++i) {  // the other form's plates and rings
        const bool mine = home ? i >= 10 : i < 10;
        ui.HideInstance(slot_path(i), !mine);
        if (i != 9) ui.HideInstance(ring_path(i), !mine);
    }
    PadSlots slots = DefaultPadSlots();
    PadSlotsFromText(settings.controls.pad_lanes, slots);
    std::string& home_text = config == HomeConfig::Easy ? settings.controls.pad_lanes_home_easy : settings.controls.pad_lanes_home_hard;
    HomeSlots home_slots = DefaultHomeSlots();
    HomeSlotsFromText(home_text, home_slots);
    const int count = home ? kHomeSlots : kPadSlots;
    const auto button = [&](int i) -> PadButton& { return home ? home_slots[size_t(i)] : slots[size_t(i)]; };
    int focus = 0;
    const auto show = [&] {
        for (int i = 0; i < count; ++i) {
            const std::string p = slot_path(home ? 10 + i : i);
            const int b = int(button(i));
            ui.SetMaterialUv(p, "cont_button00_M", 0.25f * float(b % 4), 0.25f * float(b / 4));
            ui.HideNode(p, "button_plate00_select", i != focus);
            ui.HideNode(p, "button_plate00_select_eff", i != focus);
            ui.HideNode(p, "button_plate00_on", true);
            ui.HideNode(p, "button_plate00_on_eff", true);
            ui.HideNode(p, "button_plate00_off", i == focus);
        }
        for (int l = 0; l < (home ? kHomeSlots : 9); ++l) {  // the focused slot's ring turns blue
            const bool lit = home ? l == focus : l == kLaneOfSlot[size_t(focus)];
            ui.HideNode(ring_path(home ? 10 + l : l), "judge_ring00_blue", !lit);
            ui.HideNode(ring_path(home ? 10 + l : l), "judge_ring00_eff", true);
        }
    };
    show();
    SDL_SetWindowTitle(window, "SIF port - options: button controls");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;
            const int n = int(PadButton::Count);
            PadButton& b = button(focus);
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT: focus = (focus + count - 1) % count; break;
                case SDL_SCANCODE_RIGHT: focus = (focus + 1) % count; break;
                case SDL_SCANCODE_UP: b = PadButton((int(b) + n - 1) % n); break;
                case SDL_SCANCODE_DOWN: b = PadButton((int(b) + 1) % n); break;
                case SDL_SCANCODE_T:
                    if (home) home_slots = DefaultHomeSlots();
                    else slots = DefaultPadSlots();
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    if (!e.key.repeat) fade.Leave(ui);
                    break;
                default: break;
            }
            show();
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, root, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    // shortcut: a button in two slots is kept (it plays the first slot's lane), as the game's own rule is not traced
    if (home) {
        home_text = home_slots == DefaultHomeSlots() ? std::string() : HomeSlotsToText(home_slots);
        SetHomeSlots(config, home_slots);
        LogInfo("options: pad lanes {} {}", config == HomeConfig::Easy ? "BEGINNER, STANDARD" : "HYPER, ADVENTURE",
                HomeSlotsToText(home_slots));
    } else {
        settings.controls.pad_lanes = slots == DefaultPadSlots() ? std::string() : PadSlotsToText(slots);
        SetPadSlots(slots);
        LogInfo("options: pad lanes {}", PadSlotsToText(slots));
    }
    return open;
}

// Control Settings (uil0010_controllerOpt_layout00, the game's screen in work/shadps4-run/kc-15.png): "Live Show
// Player Controller Settings" with three rows, each a label plate and a Modify button (uil0046_gameOpt_iinePlate00_00
// -02: uip0002_opt_setplate00, uip0004_opt_setplate01): "BEGINNER, STANDARD" and "HYPER, ADVENTURE" under Play Live
// Show, "EASY, NORMAL, HARD, EXTREME, CHALLENGE" under Play Live Show (AC) (messages 724-726). Up/Down pick a row,
// Enter opens its Button Controls, Esc leaves. shortcut: the Vibration row and the OK button are hidden (the port has
// no vibration; Esc is OK)
bool RunControlSettings(SDL_Window* window, Renderer& renderer, UiScreen& ui, const Messages& messages, AppSettings& settings,
                        long max_frames) {
    const std::string root = "uil0010_controllerOpt_layout00";
    const auto message = [&](int id, const char* fallback) {
        const std::string& m = messages.Get(id);
        return m.empty() ? std::string(fallback) : m;
    };
    static constexpr const char* kRowNames[3] = {"BEGINNER, STANDARD", "HYPER, ADVENTURE",
                                                 "EASY, NORMAL, HARD, EXTREME, CHALLENGE"};
    const auto row = [&](int r) {
        char s[96];
        std::snprintf(s, sizeof(s), "%s/uil0046_gameOpt_iinePlate00_%02d/", root.c_str(), r);
        return std::string(s);
    };
    int focus = 0;
    bool open = true;
    while (open) {
        route::SetScreen("controlset");
        ui.SetMaterialUv(root + "/uil0001_option_set00/uip0001_opt_title_text00", "M_opt_title_text00", 0, 0.25f);
        SetText(ui, root + "/uip0020_menutitle_plate00_00", 0, "Live Show Player Controller Settings");
        SetText(ui, root + "/uip0030_controllerItem00", 0, "Play Live Show");
        ui.SetInstanceText(root + "/uip0030_controllerItem00", 1, "Play Live Show (AC)");
        for (const char* hidden : {"uip0020_menutitle_plate00_01", "uil0049_gameOpt_onoff_button00", "common_uip0005_SelectButton00"}) {
            ui.HideInstance(root + "/" + hidden, true);
        }
        for (int r = 0; r < 3; ++r) SetText(ui, row(r) + "uip0002_opt_setplate00", 0, message(724 + r, kRowNames[r]));
        const auto show = [&] {
            for (int r = 0; r < 3; ++r) {
                const std::string plate = row(r) + "uip0002_opt_setplate00", button = row(r) + "uip0004_opt_setplate01";
                ui.SetInstanceText(button, 0, "Modify");
                ui.HideNode(plate, "select_plate_on", r != focus);
                ui.HideNode(plate, "menu_borderline00_on", r != focus);
                ui.HideNode(plate, "menu_borderline00_off", r == focus);
                ui.HideNode(button, "opt_setplate01_on", r != focus);
                ui.HideNode(button, "opt_setplate01_ef", r != focus);
                ui.HideNode(button, "opt_setplate01_off", r == focus);
            }
        };
        show();
        SDL_SetWindowTitle(window, "SIF port - options: control settings");
        ScreenFade fade;
        int chosen = -1;
        uint64_t last = SDL_GetTicksNS();
        for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
            SDL_Event e;
            while (PollEvent(e)) {
                if (e.type == SDL_EVENT_QUIT) open = false;
                if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
                switch (e.key.scancode) {
                    case SDL_SCANCODE_UP: focus = (focus + 2) % 3; break;
                    case SDL_SCANCODE_DOWN: focus = (focus + 1) % 3; break;
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_SPACE: chosen = focus, fade.Leave(ui); break;
                    case SDL_SCANCODE_ESCAPE:
                    case SDL_SCANCODE_BACKSPACE: fade.Leave(ui); break;
                    default: break;
                }
                show();
            }
            const uint64_t now = SDL_GetTicksNS();
            ui.Update(double(now - last) / 1e9);
            fade.Update(double(now - last) / 1e9);
            last = now;
            if (!renderer.BeginFrame()) {
                SDL_Delay(5);
                continue;
            }
            ui.DrawLayout(renderer, root, 1.0f);
            fade.Draw(renderer);
            renderer.EndFrame();
        }
        if (!open || chosen < 0) break;
        LogInfo("options: control settings row {}", kRowNames[chosen]);
        ui.Play("*", UiScreen::ClipName(1), false);  // the next screen's "in" (the fade played "out")
        open = chosen == 2 ? RunButtons(window, renderer, ui, settings, max_frames)
                           : RunButtons(window, renderer, ui, settings, max_frames, true,
                                        chosen == 0 ? HomeConfig::Easy : HomeConfig::Hard);
        ui.Play("*", UiScreen::ClipName(1), false);
    }
    return open;
}

}

// shortcut: the labels are the port's own English words (the game takes them from message.pac, not decoded yet) and
// only the settings the port has are shown; the other rows (more sound channels, brightness, judge ring, voice,
// "iine", language) and the two buttons are hidden until they exist
bool RunOptions(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, AudioOutput& audio,
                AppSettings& settings, OptionsPage page, long max_frames) {
    route::SetScreen("options");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("options: text: {}", error);
    // the Rhythm Icon Speed row in the game's words: message 711 and 728-732 (Slow .. Fast)
    Messages messages;
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    if (!messages.Load(vfs, language, &error)) LogWarn("options: messages: {}", error);
    const auto message = [&](int id, const char* fallback) {
        const std::string& m = messages.Get(id);
        return m.empty() ? std::string(fallback) : m;
    };
    const std::string speed_label = message(711, "Rhythm Icon Speed");
    static constexpr const char* kSpeedNames[5] = {"Slow", "A Bit Slow", "Normal", "A Bit Fast", "Fast"};
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/60_option", lang, &error, &text)) {
        LogError("options: {}", error);
        return true;
    }
    if (page == OptionsPage::kButtons) {
        const bool open = RunControlSettings(window, renderer, ui, messages, settings, max_frames);
        ui.Release(renderer);
        return open;
    }
    const bool sound = page == OptionsPage::kSound;
    const std::string root = sound ? "uil0020_soundOpt_layout00" : "uil0040_gameOpt_layout00";
    // the title atlas (opt_titile_test00): rows of 1/8, Sound, Game Settings, Control Settings, Button Controls
    ui.SetMaterialUv(root + "/uil0001_option_set00/uip0001_opt_title_text00", "M_opt_title_text00", 0, sound ? 0 : 0.125f);
    for (const char* other : {"common_uip0005_SelectButton00_00", "common_uip0005_SelectButton00_01", "uip0020_menutitle_plate00_01"}) {
        ui.HideInstance(root + "/" + other, true);
    }
    SetText(ui, root + "/uip0020_menutitle_plate00_00", 0, sound ? "Sound" : "Live");
    std::vector<Row> rows;
    if (sound) {
        for (int k = 1; k < 6; ++k) ui.HideInstance(root + "/uip0021_sound_param00_0" + std::to_string(k), true);
        rows.push_back({root + "/uip0021_sound_param00_00", "", "Volume"});
    } else {
        for (const char* other : {"uil0043_gameOpt_brightnessPlate00", "uil0044_gameOpt_judgeringPlate00",
                                  "uil0045_gameOpt_voicePlate00", "uil0046_gameOpt_iinePlate00", "uil0047_gameOpt_languagePlate00"}) {
            ui.HideInstance(root + "/" + other, true);
        }
        for (const char* plate : {"uil0041_gameOpt_speedPlate00", "uil0042_gameOpt_timingPlate00"}) {
            const std::string p = root + "/" + plate;
            rows.push_back({p + "/uip0002_opt_setplate00", p + "/uip0040_gameOpt_box00", ""});
        }
        rows[0].label = speed_label;
        rows[1].label = "Timing offset";
    }
    for (const Row& r : rows) SetText(ui, r.plate, 0, r.label);
    // the volume knob: clip a012 of the sound row, posed at the volume in percent (Hypothesis: 0-100 frames span it)
    if (sound) ui.Play(rows[0].plate, "a012", false, 1);

    int focus = 0;
    const auto show = [&] {
        for (size_t i = 0; i < rows.size(); ++i) Focus(ui, rows[i], int(i) == focus);
        char value[32];
        if (sound) {
            const int percent = int(std::lround(settings.audio.volume * 100));
            std::snprintf(value, sizeof(value), "%d", percent);
            SetText(ui, rows[0].plate, 2, value);
            ui.SetMaterialClip(rows[0].plate, "M_volume_gauge00_01", settings.audio.volume);  // the gauge's fill
            ui.SetClipTime(rows[0].plate, 1, float(percent));
        } else {
            SetText(ui, rows[0].box, 0, message(728 + settings.play.note_speed, kSpeedNames[settings.play.note_speed]));
            std::snprintf(value, sizeof(value), "%+d ms", settings.timing.audio_offset_ms);
            SetText(ui, rows[1].box, 0, value);
        }
    };
    show();
    SDL_SetWindowTitle(window, sound ? "SIF port - options: sound" : "SIF port - options: game");
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || fade.leaving) continue;  // held arrows repeat
            int step = 0;
            switch (e.key.scancode) {
                case SDL_SCANCODE_UP: focus = (focus + int(rows.size()) - 1) % int(rows.size()); break;
                case SDL_SCANCODE_DOWN: focus = (focus + 1) % int(rows.size()); break;
                case SDL_SCANCODE_LEFT: step = -1; break;
                case SDL_SCANCODE_RIGHT: step = 1; break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    if (!e.key.repeat) fade.Leave(ui);
                    break;
                default: break;
            }
            if (step && sound) {
                settings.audio.volume = std::clamp(std::round(settings.audio.volume * 20 + step) / 20, 0.0f, 1.0f);
                audio.SetVolume(settings.audio.volume);
            } else if (step && focus == 0) {
                settings.play.note_speed = std::clamp(settings.play.note_speed + step, 0, 4);
            } else if (step) {
                settings.timing.audio_offset_ms = std::clamp(settings.timing.audio_offset_ms + 5 * step, -300, 300);
            }
            show();
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, root, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    LogInfo("options: volume {:.2f}, note speed {}, timing offset {} ms", settings.audio.volume,
            kSpeedNames[settings.play.note_speed], settings.timing.audio_offset_ms);
    return open;
}

}
