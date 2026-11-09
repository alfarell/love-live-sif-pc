#include "app/input.h"

#include <sstream>
#include <utility>

#include "app/route.h"
#include "engine/core/log.h"

namespace sif {
namespace {

PadLayout g_layout = PadLayout::Menu;
PadSlots g_slots = DefaultPadSlots();
std::array<bool, 2> g_trigger_down{};  // L2, R2

constexpr std::pair<SDL_GamepadButton, SDL_Scancode> kMenu[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_SCANCODE_RIGHT},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_SCANCODE_UP},     {SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_SCANCODE_DOWN},
    {SDL_GAMEPAD_BUTTON_SOUTH, SDL_SCANCODE_SPACE},    {SDL_GAMEPAD_BUTTON_EAST, SDL_SCANCODE_ESCAPE},
    {SDL_GAMEPAD_BUTTON_START, SDL_SCANCODE_RETURN},   {SDL_GAMEPAD_BUTTON_NORTH, SDL_SCANCODE_T},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_SCANCODE_PAGEUP}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_SCANCODE_PAGEDOWN},
};
// the live screen's lane keys (A S D F Space J K L ;), left to right
constexpr SDL_Scancode kLaneKeys[9] = {SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_SPACE,
                                       SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON};
constexpr const char* kNames[] = {"UP",     "LEFT",  "DOWN", "RIGHT", "TRIANGLE", "SQUARE",
                                  "CROSS",  "CIRCLE", "L1",  "L2",    "R1",       "R2"};
// SDL's button for each PadButton (L2/R2 are axes, handled apart)
constexpr SDL_GamepadButton kSdlButton[] = {
    SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_NORTH,   SDL_GAMEPAD_BUTTON_WEST,      SDL_GAMEPAD_BUTTON_SOUTH,     SDL_GAMEPAD_BUTTON_EAST,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_INVALID, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_INVALID};

SDL_Scancode LiveKey(PadButton b) {
    for (int s = 0; s < kPadSlots; ++s) {  // a button in two slots plays the first one's lane
        if (g_slots[size_t(s)] == b) return kLaneKeys[kLaneOfSlot[size_t(s)]];
    }
    return SDL_SCANCODE_UNKNOWN;
}

SDL_Scancode Mapped(Uint8 button) {
    if (g_layout == PadLayout::Live) {
        if (button == SDL_GAMEPAD_BUTTON_START) return SDL_SCANCODE_ESCAPE;
        for (int b = 0; b < int(PadButton::Count); ++b) {
            if (kSdlButton[b] == button) return LiveKey(PadButton(b));
        }
    } else {
        for (const auto& [b, key] : kMenu) {
            if (b == button) return key;
        }
    }
    return SDL_SCANCODE_UNKNOWN;
}

void ToKey(SDL_Event& e, SDL_Scancode key, bool down, Uint64 timestamp) {
    e = {};
    e.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.timestamp = timestamp;
    e.key.scancode = key;
    e.key.key = SDL_GetKeyFromScancode(key, SDL_KMOD_NONE, false);
    e.key.down = down;
}

}

const char* PadButtonName(PadButton b) { return b < PadButton::Count ? kNames[int(b)] : "?"; }

PadSlots DefaultPadSlots() {
    using B = PadButton;
    return {B::L2, B::L1, B::Left, B::Down, B::Right, B::Square, B::Cross, B::Circle, B::R1, B::R2};
}

std::string PadSlotsToText(const PadSlots& slots) {
    std::string s;
    for (PadButton b : slots) s += (s.empty() ? "" : " ") + std::string(PadButtonName(b));
    return s;
}

bool PadSlotsFromText(const std::string& text, PadSlots& out) {
    std::istringstream in(text);
    PadSlots slots{};
    std::string word;
    for (PadButton& slot : slots) {
        if (!(in >> word)) return false;
        int b = 0;
        while (b < int(PadButton::Count) && word != kNames[b]) ++b;
        if (b == int(PadButton::Count)) return false;
        slot = PadButton(b);
    }
    if (in >> word) return false;
    out = slots;
    return true;
}

void SetPadSlots(const PadSlots& slots) { g_slots = slots; }
const PadSlots& CurrentPadSlots() { return g_slots; }

void SetPadLayout(PadLayout layout) { g_layout = layout; }

bool PollEvent(SDL_Event& e) {
    if (route::Inject(e)) return true;  // a scripted route (M9) first
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
        if (key != SDL_SCANCODE_UNKNOWN) ToKey(e, key, e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN, e.gbutton.timestamp);
    } else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
                                                           e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
        const int t = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 0 : 1;
        const bool down = e.gaxis.value > SDL_JOYSTICK_AXIS_MAX / 2;
        if (down != g_trigger_down[size_t(t)]) {
            g_trigger_down[size_t(t)] = down;
            const SDL_Scancode key = g_layout == PadLayout::Live ? LiveKey(t ? PadButton::R2 : PadButton::L2)
                                                                  : SDL_SCANCODE_UNKNOWN;
            if (key != SDL_SCANCODE_UNKNOWN) ToKey(e, key, down, e.gaxis.timestamp);
        }
    }
    return true;
}

}
