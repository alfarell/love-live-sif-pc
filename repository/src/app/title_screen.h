#pragma once
// Boot and title (M6): CRI and Square Enix logos, the notice, then the title screen from ui2/01_title.

#include <string>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

// Returns true when the player presses any key or pad button on the title screen, false when the
// window is closed, Esc is pressed or `max_frames` runs out. `lang`: "en", "cn", "tw" or "" (Japanese).
bool RunTitle(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames);

}
