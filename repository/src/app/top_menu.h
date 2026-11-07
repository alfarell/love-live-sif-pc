#pragma once
// The top menu (ui2/10_top_menu), driven by the rules traced from the eboot's TopMenu screen (0x1B8790-0x1BF7xx;
// analysis/202611011550-m6-ui-boot-flow/).

#include <string>
#include <vector>

struct SDL_Window;

namespace sif {

class AudioOutput;
class Renderer;
class Vfs;

// The player's nameplate in the header (uil0009_PlayerDataTop00 places 03_card's name plate, badge and accolade plate).
struct NameplateItem {
    int id = 0;
    int plate = 0;  // accolade: ui_honorific_title.pac title_plate_NNNN (row +0x08); badge: ui_badge.pac badge_plate_NNN_0
                    // (row +0x04 category)
    std::string name;
};
struct Nameplate {
    std::string player;
    std::vector<NameplateItem> accolades, badges;  // the choosable ones
    size_t accolade = 0, badge = 0;                // the chosen ones (indices); the menu changes them
};

// Shows the top menu until the player confirms a submenu entry (returns its entry id) or quits (returns -1).
// `mode` (optional) gets the wheel mode the entry belongs to (0 Play .. 5 Event; entry ids repeat across modes).
// With `plate`, the header shows the nameplate; Profile Card (entry 15) edits it in place: Up/Down accolade, Left/Right
// badge, Enter or Esc done (shortcut: the game's Profile Card screen, 40_collection uil0031_PlateChangeMenu00, is not
// built). With `audio`, the navigator greets by the time of day on entry and says another line on T (pad Triangle).
int RunTopMenu(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
               int* mode = nullptr, AudioOutput* audio = nullptr, Nameplate* plate = nullptr);

}
