#pragma once
// The top menu (ui2/10_top_menu), driven by the rules traced from the eboot's TopMenu screen (0x1B8790-0x1BF7xx;
// analysis/202611011550-m6-ui-boot-flow/).

#include <string>

struct SDL_Window;

namespace sif {

class AudioOutput;
class Renderer;
class Vfs;

// Shows the top menu until the player confirms a submenu entry (returns its entry id) or quits (returns -1).
// `mode` (optional) gets the wheel mode the entry belongs to (0 Play .. 5 Event; entry ids repeat across modes).
// With `audio`, the navigator greets by the time of day on entry and says another line on T (pad Triangle).
int RunTopMenu(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
               int* mode = nullptr, AudioOutput* audio = nullptr);

}
