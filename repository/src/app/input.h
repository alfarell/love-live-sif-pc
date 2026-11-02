#pragma once
// Gamepads as keyboard keys: every screen reads keys, so pad buttons are turned into the keys it already handles.

#include <SDL3/SDL.h>

namespace sif {

// Menu: D-pad = arrows, South (Cross) = Space (confirm), East (Circle) = Esc (back), Start = Return.
// Live: the 9 lanes left to right = D-pad Left, Up, Right, Down, L1 or R1 (centre), West (Square), North (Triangle),
// East (Circle), South (Cross); Start = Esc. shortcut: the live layout is a guess, the game's own pad mapping is not
// traced; replace it once the eboot's input table is read.
enum class PadLayout { Menu, Live };
void SetPadLayout(PadLayout layout);

// SDL_PollEvent that also opens pads as they connect and delivers a mapped pad button as a key event (same
// timestamp, so live judgement timing is unchanged). Unmapped buttons arrive as pad events.
bool PollEvent(SDL_Event& e);

}
