#pragma once
// The result screen (ui2/95_result, layout uil0001_ResultTop00) for one finished live.

#include <array>
#include <string>

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
};

// Shows the result until a key or pad button (or `max_frames`); false if the window was closed.
bool RunResult(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& result,
               long max_frames);

}
