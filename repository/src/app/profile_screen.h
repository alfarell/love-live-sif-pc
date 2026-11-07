#pragma once
// Collection → Profile Card: the game's "Change Accolade or Name Plate" screen (ui2/40_collection
// uil0050_ProcEditLayout).

#include <string>

#include "app/top_menu.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

// PageUp/PageDown (pad L1/R1) switch the Accolade, Name Plate and Badge tabs; arrows move in the tab's list; Enter
// chooses the focused item (the preview shows it); Esc leaves. `plate` holds the choice afterwards. False if the window
// was closed.
bool RunProfile(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, Nameplate& plate,
                long max_frames);

}
