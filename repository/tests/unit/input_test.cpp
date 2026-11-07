// Pad buttons as keys (src/app/input.cpp), driven by an SDL virtual gamepad: no hardware needed.
#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

#include "app/input.h"

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// Polls (through PollEvent) until a key event arrives or ~1 s passes; SDL_SCANCODE_UNKNOWN if none.
static SDL_Scancode NextKey(bool& down) {
    for (int i = 0; i < 100; ++i) {
        SDL_UpdateJoysticks();
        SDL_Event e;
        while (sif::PollEvent(e)) {
            if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
                down = e.type == SDL_EVENT_KEY_DOWN;
                return e.key.scancode;
            }
        }
        SDL_Delay(10);
    }
    return SDL_SCANCODE_UNKNOWN;
}

static void Press(SDL_Joystick* joy, SDL_GamepadButton b, SDL_Scancode want, const char* what) {
    bool down = false;
    SDL_SetJoystickVirtualButton(joy, b, true);
    const SDL_Scancode k = NextKey(down);
    Check(k == want && down, std::string(what) + ": press gives key " + std::to_string(int(k)));
    SDL_SetJoystickVirtualButton(joy, b, false);
    const SDL_Scancode k2 = NextKey(down);
    Check(k2 == want && !down, std::string(what) + ": release gives key-up " + std::to_string(int(k2)));
}

int main() {
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        std::printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.name = "sif test pad";
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    SDL_Joystick* joy = SDL_OpenJoystick(id);
    Check(id != 0 && joy, "virtual gamepad attached");
    bool down = false;
    NextKey(down);  // lets PollEvent see the "added" events and open the gamepad
    Press(joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT, "menu D-pad left");
    Press(joy, SDL_GAMEPAD_BUTTON_SOUTH, SDL_SCANCODE_SPACE, "menu Cross");
    sif::SetPadLayout(sif::PadLayout::Live);
    // the game's defaults: L2 L1 Left Down (Right or Square) Cross Circle R1 R2
    Press(joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_SCANCODE_D, "live lane 2 (Left)");
    Press(joy, SDL_GAMEPAD_BUTTON_WEST, SDL_SCANCODE_SPACE, "live lane 4 (Square)");
    Press(joy, SDL_GAMEPAD_BUTTON_SOUTH, SDL_SCANCODE_J, "live lane 5 (Cross)");
    Press(joy, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_SCANCODE_L, "live lane 7 (R1)");
    SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
    SDL_Scancode k = NextKey(down);
    Check(k == SDL_SCANCODE_SEMICOLON && down, "live lane 8: R2 pulled gives ; " + std::to_string(int(k)));
    SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0);
    k = NextKey(down);
    Check(k == SDL_SCANCODE_SEMICOLON && !down, "live lane 8: R2 released gives ; up");
    // a remap: Triangle on lane 0
    sif::PadSlots slots = sif::DefaultPadSlots();
    slots[0] = sif::PadButton::Triangle;
    sif::SetPadSlots(slots);
    Press(joy, SDL_GAMEPAD_BUTTON_NORTH, SDL_SCANCODE_A, "remapped lane 0 (Triangle)");
    sif::PadSlots back{};
    Check(sif::PadSlotsFromText(sif::PadSlotsToText(slots), back) && back == slots, "slots text round trip");
    Check(sif::PadSlotsToText(sif::DefaultPadSlots()) == "L2 L1 LEFT DOWN RIGHT SQUARE CROSS CIRCLE R1 R2", "default text");
    Check(!sif::PadSlotsFromText("L2 L1", back) && back == slots, "short text refused");
    SDL_Quit();
    std::printf("%s\n", failures ? "input_test FAILED" : "input_test ok");
    return failures ? 1 : 0;
}
