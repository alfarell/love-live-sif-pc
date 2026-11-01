#pragma once
// Song and difficulty select (ui2/20_setup, layout uil0001_StepLayout00), driven by the parts and rules traced so far
// (analysis/202611011550-m6-ui-boot-flow/).

#include <string>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

struct SongChoice {
    int song = -1;     // -1: backed out
    int variant = 4;   // chart variant 00-04 (arcade difficulties)
};

SongChoice RunSongSelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames);

}
