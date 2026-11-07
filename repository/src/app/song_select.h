#pragma once
// Song and difficulty select (ui2/20_setup, layout uil0001_StepLayout00), driven by the parts and rules traced so far
// (analysis/202611011550-m6-ui-boot-flow/).

#include <string>

#include "game/dlc/dlc.h"
#include "game/save/save.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

struct SongChoice {
    int song = -1;     // -1: backed out
    int variant = 4;   // chart variant 00-04 (arcade difficulties)
    bool closed = false;  // the window was closed
    std::string stage;    // the chosen stage folder, "stNNN" ("" = the live screen's default)
    std::string costume;  // the chosen costume folder, "dNNN" ("" = the live screen's default pick)
};

// `previous`: the last choice, focused again (song and difficulty) when coming back from a live. `save` (optional):
// the records shown on the difficulty cards and plates. `owned` (optional): only the songs and costumes it owns are
// listed (shortcut: add-ons the user has not installed are left out; the game shows them locked).
SongChoice RunSongSelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
                         const SongChoice& previous = {}, const save::SaveData* save = nullptr,
                         const dlc::Ownership* owned = nullptr);

}
