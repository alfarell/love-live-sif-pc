#pragma once
// Collection → Profile Card: the game's "Change Accolade or Name Plate" screen (ui2/40_collection
// uil0050_ProcEditLayout).

#include <filesystem>
#include <string>

#include "app/profile_card.h"
#include "app/top_menu.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;
namespace save {
struct SaveData;
}

// PageUp/PageDown (pad L1/R1) switch the Accolade, Name Plate and Badge tabs; arrows move in the tab's list; Enter
// chooses the focused item (the preview shows it); Esc leaves. `plate` holds the choice afterwards. False if the window
// was closed.
bool RunProfile(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, Nameplate& plate,
                long max_frames);

// Collection -> Profile Card's menu (M12 P6a; uil0090_ProcMenuLayout00, shadPS4 work/m12/shad2/c03, p09): the current
// profile card on the left, the current accolade and nameplate, and three buttons. Up/Down move, Enter or Space
// chooses, Esc leaves. Returns 1 Change Accolade or Name Plate, 2 Create a New Profile Card (only with a photo),
// 0 back, -1 the window was closed. Everyone's Profile Cards (online) cannot be chosen.
struct ProfileMenuSetup {
    const save::SaveData* save = nullptr;
    std::filesystem::path data_dir;  // profile_card.png
    const Nameplate* plate = nullptr;
    ProfileCardTexts texts;
    int member = 1;  // the card-setting member
};
int RunProfileMenu(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                   const ProfileMenuSetup& setup, long max_frames);

}
