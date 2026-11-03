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

enum class OptionsPage { kSound, kGame };

// Up/Down pick a row, Left/Right change it, Enter or Esc leaves. False if the window was closed.
bool RunOptions(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, AudioOutput& audio,
                AppSettings& settings, OptionsPage page, long max_frames);

}
