#pragma once
// Live Show Medley (M16 R4): a Set List played song after song as View Live Show lives on one stage, then its
// "Live Show Medley: End the Live Show" screen. Analysis: analysis/202611231413-live-show-medley/ (R1 eboot, R2 shadPS4).

#include <string>
#include <vector>

#include "game/medley/medley.h"

struct SDL_Window;

namespace sif {

class AudioOutput;
class Renderer;
class Vfs;
struct AppSettings;
struct LiveOptions;

// What a medley plays: a recommended list or one of the player's own (save::MedleyList).
struct MedleyRun {
    std::string name;  // shown on the end screen
    int group = 0;     // 0 µ's, 1 Aqours
    int stage = 0;     // the stage of the whole list (stNNN); 0 = the group's default
    std::vector<medley::Entry> entries;  // empty slots (song 0) are left out
};

struct MedleyOutcome {
    bool closed = false;    // the window was closed
    bool finished = false;  // the last song played to its end (not left with Esc)
    double seconds = 0;     // the time the songs played (missions 14 and 15)
    std::vector<medley::Entry> watched;  // the entries whose song played to its end (missions 37 and 47)
    // the end screen's choice: 0 Watch the Same Medley List (570), 1 Return to Medley Selection (571), 2 Return to
    // Main Menu (566)
    int choice = 1;
};

// `base`: the live options to start from (language, wear, ...); the song, costume, stage and medley flags are set
// per entry. A song that cannot be loaded is skipped, as the game skips a missing or locked one.
MedleyOutcome RunMedley(SDL_Window* window, Renderer& renderer, AudioOutput& audio, const Vfs& vfs,
                        const AppSettings& settings, const LiveOptions& base, const MedleyRun& run, long max_frames);

// "Live Show Medley: End the Live Show" (95_result uil0030_MedleyEndLayout00; shadPS4 work/m16/shad/end2): the list's
// name and its songs as "NN.Song / Singers". Sets `out.choice`; false if the window was closed.
bool RunMedleyEnd(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const MedleyRun& run,
                  long max_frames, MedleyOutcome& out);

// One end-screen row: "01.Kimetayo Hand in Hand / Riko Sakurauchi Chika Takami You Watanabe"; a song of the whole
// group shows the group's name. `member_names`: member id -> name.
std::string MedleyRow(int number, const std::string& title, int unit, const std::vector<int>& members,
                      const std::vector<std::string>& member_names);

}
