#pragma once
// Gamepads as keyboard keys: every screen reads keys, so pad buttons are turned into the keys it already handles.

#include <SDL3/SDL.h>

#include <array>
#include <string>

namespace sif {

// Menu: D-pad = arrows, South (Cross) = Space (confirm), East (Circle) = Esc (back), Start = Return.
// Live: the lane buttons of the Button Controls (below); Start = Esc.
enum class PadLayout { Menu, Live };
void SetPadLayout(PadLayout layout);

// The 12 buttons the game's Button Controls offer, in the order of its icon atlas (ui2/60_option
// opt_ope_ingame_button00, 4 per row).
enum class PadButton { Up, Left, Down, Right, Triangle, Square, Cross, Circle, L1, L2, R1, R2, Count };
const char* PadButtonName(PadButton b);  // "L2", "SQUARE" ...

// The 9-lane (AC) Button Controls: 10 slots, left to right; slots 4 and 5 both play the centre lane.
constexpr int kPadSlots = 10;
constexpr std::array<int, kPadSlots> kLaneOfSlot = {0, 1, 2, 3, 4, 4, 5, 6, 7, 8};
using PadSlots = std::array<PadButton, kPadSlots>;
// The game's defaults, read off its own Button Controls screen (M7): L2 L1 Left Down Right Square Cross Circle R1 R2.
PadSlots DefaultPadSlots();
std::string PadSlotsToText(const PadSlots& slots);               // "L2 L1 LEFT ..."
bool PadSlotsFromText(const std::string& text, PadSlots& out);  // false (out unchanged) unless 10 known names
void SetPadSlots(const PadSlots& slots);

// SDL_PollEvent that also opens pads as they connect and delivers a mapped pad button as a key event (same
// timestamp, so live judgement timing is unchanged). L2/R2 (analog triggers in SDL) count as pressed past half way.
// Unmapped buttons arrive as pad events.
bool PollEvent(SDL_Event& e);

}
