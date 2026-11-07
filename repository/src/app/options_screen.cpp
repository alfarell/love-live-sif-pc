#include "app/options_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "engine/audio/audio_output.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"

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
bool RunButtons(SDL_Window* window, Renderer& renderer, UiScreen& ui, AppSettings& settings, long max_frames) {
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
    SetText(ui, root + "/uip0020_menutitle_plate00", 0, "Live Show (AC)");
    // shortcut: Reset and OK are keys (T, Enter), not focusable buttons; the 3- and 6-lane configs are not built
    for (const char* b : {"common_uip0005_SelectButton00_00", "common_uip0005_SelectButton00_01"}) {
        ui.HideInstance(root + "/" + b, true);
    }
    for (int i = 10; i < 16; ++i) ui.HideInstance(slot_path(i), true), ui.HideInstance(ring_path(i), true);
    PadSlots slots = DefaultPadSlots();
    PadSlotsFromText(settings.controls.pad_lanes, slots);
    int focus = 0;
    const auto show = [&] {
        for (int i = 0; i < kPadSlots; ++i) {
            const std::string p = slot_path(i);
            const int b = int(slots[size_t(i)]);
            ui.SetMaterialUv(p, "cont_button00_M", 0.25f * float(b % 4), 0.25f * float(b / 4));
            ui.HideNode(p, "button_plate00_select", i != focus);
            ui.HideNode(p, "button_plate00_select_eff", i != focus);
            ui.HideNode(p, "button_plate00_on", true);
            ui.HideNode(p, "button_plate00_on_eff", true);
            ui.HideNode(p, "button_plate00_off", i == focus);
        }
        for (int l = 0; l < 9; ++l) {
            ui.HideNode(ring_path(l), "judge_ring00_blue", l != kLaneOfSlot[size_t(focus)]);
            ui.HideNode(ring_path(l), "judge_ring00_eff", true);
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
            PadButton& b = slots[size_t(focus)];
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT: focus = (focus + kPadSlots - 1) % kPadSlots; break;
                case SDL_SCANCODE_RIGHT: focus = (focus + 1) % kPadSlots; break;
                case SDL_SCANCODE_UP: b = PadButton((int(b) + n - 1) % n); break;
                case SDL_SCANCODE_DOWN: b = PadButton((int(b) + 1) % n); break;
                case SDL_SCANCODE_T: slots = DefaultPadSlots(); break;
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
    settings.controls.pad_lanes = slots == DefaultPadSlots() ? std::string() : PadSlotsToText(slots);
    SetPadSlots(slots);
    LogInfo("options: pad lanes {}", PadSlotsToText(slots));
    return open;
}

}

// shortcut: the labels are the port's own English words (the game takes them from message.pac, not decoded yet) and
// only the settings the port has are shown; the other rows (more sound channels, brightness, judge ring, voice,
// "iine", language) and the two buttons are hidden until they exist
bool RunOptions(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, AudioOutput& audio,
                AppSettings& settings, OptionsPage page, long max_frames) {
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("options: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/60_option", lang, &error, &text)) {
        LogError("options: {}", error);
        return true;
    }
    if (page == OptionsPage::kButtons) {
        const bool open = RunButtons(window, renderer, ui, settings, max_frames);
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
        rows[0].label = "Note speed";
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
            std::snprintf(value, sizeof(value), "%.1f s", settings.play.approach_ms / 1000.0);
            SetText(ui, rows[0].box, 0, value);
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
                settings.play.approach_ms = std::clamp(settings.play.approach_ms + 100 * step, 400, 3000);
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
    LogInfo("options: volume {:.2f}, note speed {} ms, timing offset {} ms", settings.audio.volume, settings.play.approach_ms,
            settings.timing.audio_offset_ms);
    return open;
}

}
