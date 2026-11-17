#pragma once
// Collection -> Photo Gallery and Collection -> Member Card (M12 P5; ui2/40_collection uil0011_GalleryComWin00;
// shadPS4 work/m12/shad/c04-c11, shad2/c01).

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;
namespace card {
struct Data;
}
namespace costume {
struct Data;
}
namespace dlc {
struct Ownership;
}
namespace save {
struct SaveData;
}

// The photos of View Live Show, newest first, 7 to a row (all 30 fit the window's five rows). Arrows move, Square (Q)
// marks a favourite (25 at most), Delete removes a photo that is not a favourite, Space shows it full screen, Esc
// leaves. Changes go into `save` (the caller writes it); a deleted photo's file is removed from `data_dir`. False if
// the window was closed.
bool RunPhotoGallery(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                     const std::filesystem::path& data_dir, save::SaveData& save, long max_frames);

// The member cards of one member, 7 to a row: owned ones face up, the others as card backs ("(Not possessed)" with the
// costume name). L1/R1 (Page Up/Down) change the member, Tab the group, arrows move, Esc leaves. A member card is
// owned with its costume (costume::Owns). False if the window was closed.
struct MemberCardSetup {
    const card::Data* cards = nullptr;
    const costume::Data* costumes = nullptr;
    const save::SaveData* save = nullptr;
    const dlc::Ownership* addons = nullptr;
    std::vector<std::pair<int, std::string>> names;  // costume id -> name in the UI language
    int member = 1;                                   // the tab shown first
};
bool RunMemberCards(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                    const MemberCardSetup& setup, long max_frames);

}
