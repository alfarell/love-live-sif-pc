#pragma once
// Live Show Medley's Set List screens (M16 R5; ui2/20_setup uil0050_MedleyLayout00; shadPS4 work/m16/shad b1-b3,
// f3, g0-g5): "Set List Selection" with the Your and Recommended! tabs (L1/R1), the group's ten lists and the chosen
// list's 20 lines; "Set list editing" for your own lists (Clear, Insert, Delete, Change Stage, Edit Slot), Rename, and
// the start confirmation. Analysis: analysis/202611231413-live-show-medley/.

#include <string>

#include "app/medley_live.h"
#include "game/costume/costume.h"
#include "game/dlc/dlc.h"
#include "game/medley/medley.h"
#include "game/save/save.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

// Where the screen was left, kept between visits (the list played comes back focused)
struct MedleyPlace {
    int tab = 0;   // 0 Your, 1 Recommended!
    int list = 0;  // 0-9
};

struct MedleyPick {
    bool closed = false;  // the window was closed
    bool start = false;   // false: backed out (Circle)
    MedleyRun run;
    int group = 0;
};

// `save`: your lists (edited and renamed here; the caller writes the file) and the current group (L2 Toggle Mode
// changes save.mode). `owned` / `costumes` (optional): only owned songs and costumes are offered when editing.
MedleyPick RunMedleySelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                           const medley::Data& data, save::SaveData& save, MedleyPlace& place,
                           const dlc::Ownership* owned, const costume::Data* costumes, long max_frames);

}
