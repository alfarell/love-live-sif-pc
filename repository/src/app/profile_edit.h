#pragma once
// Edit Profile Card (M12 P6b; ui2/40_collection uil0050_ProcEditLayout; shadPS4 work/m12/shad/p04-p16,
// work/m12/shad2/p02-p09): the player's one profile card, edited from its current values (or defaults for the first).

#include <filesystem>
#include <string>

#include "app/profile_card.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;
namespace profile {
struct Data;
}
namespace save {
struct SaveData;
}

struct ProfileEditSetup {
    save::SaveData* save = nullptr;
    std::filesystem::path data_dir;  // photos/NN.png and profile_card.png
    const Nameplate* plate = nullptr;
    ProfileCardTexts texts;
    const profile::Data* frames = nullptr;
    int member = 1;  // the card-setting member: her frames and skill cards
    std::string lang;
};

// PageUp/PageDown (L1/R1) and Left/Right on the tab bar pick a tab: Card Frame, Photo, Accolade, Name Plate, Badge,
// Skill Card, Finish Editing. Down enters the tab's list, arrows move, Enter chooses (skill cards: up to three, in
// order; Enter again removes). On the Photo tab Square (Q) opens the photo position adjustment (Left/Right move the
// photo, Square or Esc closes). Finish Editing saves the card into `setup.save` (the caller writes it) and copies its
// photo to profile_card.png. Esc in a list returns to the tab bar; Esc there leaves without saving. False if the
// window was closed.
bool RunProfileEdit(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const ProfileEditSetup& setup,
                    long max_frames);

}
