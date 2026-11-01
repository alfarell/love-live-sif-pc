#pragma once

#include <string>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

// Shows one ui2 layout until Esc or `max_frames`: `spec` is "SCREEN:LAYOUT[:CLIP]" (SCREEN without "ui2/").
int RunUiViewer(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& spec, const std::string& lang,
                long max_frames);

}
