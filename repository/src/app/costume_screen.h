#pragma once
// The costume screens (ui2/30_costume): the List of Costume Materials and Create/Change Costumes.

#include <string>
#include <utility>
#include <vector>

#include "game/costume/costume.h"
#include "game/dlc/dlc.h"
#include "game/save/save.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

// The materials the player has (uil0030_MaterialListLayout00): Left/Right switch µ's and Aqours, Esc leaves.
// False if the window was closed.
bool RunMaterials(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const costume::Data& data,
                  const save::SaveData& save, long max_frames);

// Create/Change Costumes (uil0001_CosLayout00 > uil0002_CosCreate00): the group's costumes with their state (owned,
// can be made, short of materials, add-on only). Arrows move, PageUp/PageDown (pad L1/R1) switch µ's / Aqours /
// Saint Snow, Enter opens Confirm Costume Creation (materials needed and owned), where Enter makes it (costume::Craft)
// and Esc goes back. `names`: costume names in the UI language. False if the window was closed.
bool RunCostumes(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const costume::Data& data,
                 const std::vector<std::pair<int, std::string>>& names, const dlc::Ownership* addons, save::SaveData& save,
                 long max_frames);

}
