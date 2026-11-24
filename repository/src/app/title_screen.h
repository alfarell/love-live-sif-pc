#pragma once
// Boot and title (M6): CRI and Square Enix logos, the notice, the autosave notice (691, M17), then the title screen
// from ui2/01_title. After the title: "Save data creation" (688) on a new save, then the online notice (692). The two
// notices are uil0005_SaveMessage00 on white and wait for a key (shadPS4 work/m17/shad/f00-f03, every start).

#include <string>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

// Returns true when the player presses any key or pad button on the title screen, false when the
// window is closed, Esc is pressed or `max_frames` runs out. `lang`: "en", "cn", "tw" or "" (Japanese).
bool RunTitle(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
              bool new_save = false);

}
