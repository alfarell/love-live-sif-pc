#pragma once
// Song and difficulty select (ui2/20_setup, layout uil0001_StepLayout00), driven by the parts and rules traced so far
// (analysis/202611011550-m6-ui-boot-flow/).

#include <string>

#include "game/costume/costume.h"
#include "game/dlc/dlc.h"
#include "game/save/save.h"
#include "app/skill_screen.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

struct SongChoice {
    int song = -1;     // -1: backed out
    int variant = 4;   // chart variant: 00-04 arcade difficulties, 10-13 home (BEGINNER .. ADVENTURE)
    bool closed = false;  // the window was closed
    std::string stage;    // the chosen stage folder, "stNNN" ("" = the live screen's default)
    std::string costume;  // the chosen costume folder, "dNNN" ("" = the live screen's default pick)
    bool booster = false; // a Wai-Wai! Booster used in Prepare (M14 F5; spent after the live)
    int member = 0;       // a temporary member chosen at message 591 (M17 T5), 0 = none; kept until the top menu
};

// `previous`: the last choice, focused again (song and difficulty) when coming back from a live. `save` (optional):
// the records shown on the difficulty cards and plates. `owned` (optional): only the songs and costumes it owns are
// listed (shortcut: add-ons the user has not installed are left out; the game shows them locked). `costumes` (optional,
// with `save`): only the costumes the player owns (costume::Owns: from the start, rewarded, crafted or an add-on).
// `home`: "Play Live Show" (the four home charts, variants 10-13) instead of "Play Live Show (AC)" (M10 K3).
// `skills` (optional): after the costume step comes Prepare for the Live Show, with the played member's skill cards
// and Skill Card Selection (M11 S4); without it the costume step starts the live as before.
// `view`: View Live Show (M12 P4): song (with the "LIVE VIEW MODE" banner), stage, costume and a Prepare step with the
// camera help panel; no difficulty step. The choice's variant is the home STANDARD chart (11), whose live it shows.
// M17 T5: with `save`, only the songs of its mode are listed (µ's mode: group 0; Aqours mode: Aqours and Saint Snow).
// With `skills` (a played live), a song whose singers do not include the played member asks message 591 ("You cannot
// play (%s) with this member. ...", Cancel / OK); OK opens the member box of the song's singers, and the member chosen
// there plays (SongChoice::member). Song 1380 does not ask in Aqours mode (eboot FUN_00173b60, FUN_000730a0).
SongChoice RunSongSelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
                         const SongChoice& previous = {}, const save::SaveData* save = nullptr,
                         const dlc::Ownership* owned = nullptr, const costume::Data* costumes = nullptr, bool home = false,
                         const SkillSetup* skills = nullptr, bool view = false);

}
