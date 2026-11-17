#include "app/input.h"

#include <span>
#include <sstream>
#include <utility>

#include "app/route.h"
#include "engine/core/log.h"

namespace sif {
namespace {

PadLayout g_layout = PadLayout::Menu;
PadSlots g_slots = DefaultPadSlots();
std::array<HomeSlots, 2> g_home_slots = {DefaultHomeSlots(), DefaultHomeSlots()};
bool g_live_home = false;
HomeConfig g_live_config = HomeConfig::Easy;
std::array<bool, 2> g_trigger_down{};  // L2, R2
std::array<int, 2> g_stick{};          // View: the left stick's last direction per axis (-1, 0, 1)

constexpr std::pair<SDL_GamepadButton, SDL_Scancode> kMenu[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_SCANCODE_RIGHT},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_SCANCODE_UP},     {SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_SCANCODE_DOWN},
    {SDL_GAMEPAD_BUTTON_SOUTH, SDL_SCANCODE_SPACE},    {SDL_GAMEPAD_BUTTON_EAST, SDL_SCANCODE_ESCAPE},
    {SDL_GAMEPAD_BUTTON_START, SDL_SCANCODE_RETURN},   {SDL_GAMEPAD_BUTTON_NORTH, SDL_SCANCODE_T},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_SCANCODE_PAGEUP}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_SCANCODE_PAGEDOWN},
    {SDL_GAMEPAD_BUTTON_WEST, SDL_SCANCODE_Q},
};
constexpr std::pair<SDL_GamepadButton, SDL_Scancode> kView[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_SCANCODE_COMMA}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_SCANCODE_PERIOD},
    {SDL_GAMEPAD_BUTTON_SOUTH, SDL_SCANCODE_SPACE},    {SDL_GAMEPAD_BUTTON_EAST, SDL_SCANCODE_P},
    {SDL_GAMEPAD_BUTTON_WEST, SDL_SCANCODE_Q},         {SDL_GAMEPAD_BUTTON_START, SDL_SCANCODE_ESCAPE},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_SCANCODE_PAGEUP}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_SCANCODE_PAGEDOWN},
};
using KeyTable = std::span<const std::pair<SDL_GamepadButton, SDL_Scancode>>;
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
    if (g_live_home) {
        const HomeSlots& home = g_home_slots[size_t(g_live_config)];
        for (int s = 0; s < kHomeSlots; ++s) {
            if (home[size_t(s)] == b) return kLaneKeys[kLaneOfHomeSlot[size_t(s)]];
        }
        return SDL_SCANCODE_UNKNOWN;
    }
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
        for (const auto& [b, key] : g_layout == PadLayout::View ? KeyTable(kView) : KeyTable(kMenu)) {
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

template <size_t N>
std::string SlotsToText(const std::array<PadButton, N>& slots) {
    std::string s;
    for (PadButton b : slots) s += (s.empty() ? "" : " ") + std::string(PadButtonName(b));
    return s;
}

template <size_t N>
bool SlotsFromText(const std::string& text, std::array<PadButton, N>& out) {
    std::istringstream in(text);
    std::array<PadButton, N> slots{};
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

std::string PadSlotsToText(const PadSlots& slots) { return SlotsToText(slots); }
bool PadSlotsFromText(const std::string& text, PadSlots& out) { return SlotsFromText(text, out); }

HomeSlots DefaultHomeSlots() {
    using B = PadButton;
    return {B::L1, B::Left, B::Down, B::Cross, B::Circle, B::R1};
}
std::string HomeSlotsToText(const HomeSlots& slots) { return SlotsToText(slots); }
bool HomeSlotsFromText(const std::string& text, HomeSlots& out) { return SlotsFromText(text, out); }
void SetHomeSlots(HomeConfig config, const HomeSlots& slots) { g_home_slots[size_t(config)] = slots; }
const HomeSlots& CurrentHomeSlots(HomeConfig config) { return g_home_slots[size_t(config)]; }
void SetLiveHome(bool home, HomeConfig config) { g_live_home = home, g_live_config = config; }

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
    } else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && g_layout == PadLayout::View &&
               (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX || e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)) {
        // the left stick as arrow keys, pressed past a third of the way (one key per axis at a time)
        const int a = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX ? 0 : 1;
        const int dir = e.gaxis.value > SDL_JOYSTICK_AXIS_MAX / 3 ? 1 : e.gaxis.value < SDL_JOYSTICK_AXIS_MIN / 3 ? -1 : 0;
        if (dir != g_stick[size_t(a)]) {
            const SDL_Scancode keys[2][2] = {{SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT}, {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN}};
            const int old = g_stick[size_t(a)];
            g_stick[size_t(a)] = dir;
            // release the old direction now; a new one is delivered by the next motion event of this axis
            if (old) ToKey(e, keys[a][old > 0], false, e.gaxis.timestamp);
            else ToKey(e, keys[a][dir > 0], true, e.gaxis.timestamp);
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
                                                           e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
        const int t = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 0 : 1;
        const bool down = e.gaxis.value > SDL_JOYSTICK_AXIS_MAX / 2;
        if (down != g_trigger_down[size_t(t)]) {
            g_trigger_down[size_t(t)] = down;
            const SDL_Scancode key = g_layout == PadLayout::Live   ? LiveKey(t ? PadButton::R2 : PadButton::L2)
                                     : g_layout == PadLayout::View ? (t ? SDL_SCANCODE_END : SDL_SCANCODE_HOME)
                                                                   : SDL_SCANCODE_UNKNOWN;
            if (key != SDL_SCANCODE_UNKNOWN) ToKey(e, key, down, e.gaxis.timestamp);
        }
    }
    return true;
}

}
