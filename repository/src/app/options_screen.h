#pragma once
// Options (ui2/60_option): the Sound and Game Settings pages, showing the settings the port has (volume; note speed
// and timing offset). Changes apply at once; the caller writes settings.ini afterwards.

#include <string>

#include "engine/platform/settings.h"

struct SDL_Window;

namespace sif {

class AudioOutput;
class Renderer;
class Vfs;

enum class OptionsPage { kSound, kGame, kButtons };

// Sound and Game: Up/Down pick a row, Left/Right change it. Buttons (the 9-lane pad Button Controls,
// settings.controls.pad_lanes): Left/Right pick a slot, Up/Down change its button, T (pad Triangle) resets to the game's
// defaults. Enter or Esc leaves. False if the window was closed.
bool RunOptions(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, AudioOutput& audio,
                AppSettings& settings, OptionsPage page, long max_frames);

}
