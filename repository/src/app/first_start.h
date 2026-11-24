#pragma once
// The first start on a new save (M17 T3; shadPS4 work/m17/shad/f04-f09, analysis/202611241452-flow-gaps/
// 202611241549-analysis.md): the name entry (ui2/01_title uil0050_namaset_layout00), "Confirm Name" (696, Back / OK)
// and the full-screen "Choose a Member" with the member in 3D. The game takes the name with the PS4's on-screen
// keyboard; the port takes it from the PC keyboard.

#include <string>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;
namespace save {
struct SaveData;
}

// The name entry and its confirmation: typing (up to 10 characters) and Backspace edit `name`; Enter asks "Confirm
// Name" with it, or with "New Player" (message 6) when it is empty; Back returns to typing, OK takes it. Esc leaves
// without a change when `can_leave` (Profile's Player's Name). Returns 1 with the new name in `name`, 0 left, -1 the
// window was closed.
int RunNameEntry(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, std::string& name,
                 bool can_leave, long max_frames);

// The first start after the title: the name, then the first member. Sets save.player, save.mode (the member's group)
// and save.navigator. False when the window was closed.
bool RunFirstStart(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                   save::SaveData& save, long max_frames);

}
