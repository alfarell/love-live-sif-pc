#pragma once
// The result screen (ui2/95_result, layout uil0001_ResultTop00) for one finished live.

#include <array>
#include <string>

#include "game/save/save.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

struct LiveResult {
    int song = -1, variant = 0;
    std::array<int, 5> counts{};  // Perfect, Great, Good, Bad, Miss
    int max_combo = 0;
    double score = 0;
    bool finished = false;  // the song played to its end (not quit)
    bool closed = false;    // the window was closed
    double clear_rate = 0;  // score / maximum score
    int rank = 0;           // 0 D, 1 C, 2 B, 3 A, 4 S, 5 SS, 6 SSS (the result atlas's order)
    std::string costume;    // the costume worn, "dNNN" ("" = the base model)
};

// Shows the result until a key or pad button (or `max_frames`); false if the window was closed. `news`: which
// bests this play improved (the "NEW" marks).
bool RunResult(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& result,
               long max_frames, const save::NewBests& news = {});

}
