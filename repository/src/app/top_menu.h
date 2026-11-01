#pragma once
// The top menu (ui2/10_top_menu), driven by the rules traced from the eboot's TopMenu screen (0x1B8790-0x1BF7xx;
// analysis/202611011550-m6-ui-boot-flow/).

#include <string>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

// Shows the top menu until the player confirms a submenu entry (returns its entry id) or quits (returns -1).
int RunTopMenu(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames);

}
