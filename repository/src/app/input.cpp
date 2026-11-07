#include "app/input.h"

#include <utility>

#include "engine/core/log.h"

namespace sif {
namespace {

PadLayout g_layout = PadLayout::Menu;

constexpr std::pair<SDL_GamepadButton, SDL_Scancode> kMenu[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_SCANCODE_RIGHT},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_SCANCODE_UP},     {SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_SCANCODE_DOWN},
    {SDL_GAMEPAD_BUTTON_SOUTH, SDL_SCANCODE_SPACE},    {SDL_GAMEPAD_BUTTON_EAST, SDL_SCANCODE_ESCAPE},
    {SDL_GAMEPAD_BUTTON_START, SDL_SCANCODE_RETURN},   {SDL_GAMEPAD_BUTTON_NORTH, SDL_SCANCODE_T},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_SCANCODE_PAGEUP}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_SCANCODE_PAGEDOWN},
};
// the live screen's lane keys (A S D F Space J K L ;), left to right
constexpr std::pair<SDL_GamepadButton, SDL_Scancode> kLive[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_SCANCODE_A},      {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_SCANCODE_S},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_SCANCODE_D},     {SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_SCANCODE_F},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_SCANCODE_SPACE}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_SCANCODE_SPACE},
    {SDL_GAMEPAD_BUTTON_WEST, SDL_SCANCODE_J},           {SDL_GAMEPAD_BUTTON_NORTH, SDL_SCANCODE_K},
    {SDL_GAMEPAD_BUTTON_EAST, SDL_SCANCODE_L},           {SDL_GAMEPAD_BUTTON_SOUTH, SDL_SCANCODE_SEMICOLON},
    {SDL_GAMEPAD_BUTTON_START, SDL_SCANCODE_ESCAPE},
};

SDL_Scancode Mapped(Uint8 button) {
    if (g_layout == PadLayout::Live) {
        for (const auto& [b, key] : kLive) {
            if (b == button) return key;
        }
    } else {
        for (const auto& [b, key] : kMenu) {
            if (b == button) return key;
        }
    }
    return SDL_SCANCODE_UNKNOWN;
}

}

void SetPadLayout(PadLayout layout) { g_layout = layout; }

bool PollEvent(SDL_Event& e) {
    if (!SDL_PollEvent(&e)) return false;
    if (e.type == SDL_EVENT_JOYSTICK_ADDED) {
        LogInfo("input: device {} '{}' connected ({})", e.jdevice.which, SDL_GetJoystickNameForID(e.jdevice.which),
                SDL_IsGamepad(e.jdevice.which) ? "gamepad" : "joystick without a gamepad mapping");
    } else if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (SDL_OpenGamepad(e.gdevice.which)) {  // closed by SDL_Quit
            LogInfo("input: gamepad {} opened", e.gdevice.which);
        } else {
            LogWarn("input: cannot open gamepad {}: {}", e.gdevice.which, SDL_GetError());
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        const SDL_Scancode key = Mapped(e.gbutton.button);
        if (key != SDL_SCANCODE_UNKNOWN) {
            const bool down = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            const Uint64 timestamp = e.gbutton.timestamp;
            e = {};
            e.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            e.key.timestamp = timestamp;
            e.key.scancode = key;
            e.key.key = SDL_GetKeyFromScancode(key, SDL_KMOD_NONE, false);
            e.key.down = down;
        }
    }
    return true;
}

}
