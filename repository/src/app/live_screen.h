#pragma once
// M4 vertical slice screens: one song played on 9 lanes (2D, no 3D stage yet) and an input-offset calibration.

#include <cstdint>

struct SDL_Window;

namespace sif {

class AudioOutput;
class Renderer;
class Vfs;
struct AppSettings;

struct LiveOptions {
    int song = 10;
    int variant = 4;          // 00-04 ac_easy..ac_challenge, 10-13 cs_basic..cs_master
    bool autoplay = false;
    double approach_ms = 1300;  // note travel time; Hypothesis: s_CommonParam holds per-difficulty values (1300 for ac 04?)
    long max_frames = -1;
};

// Returns 0 when the song ends or the window is closed, 1 on a load error.
int RunLive(SDL_Window* window, Renderer& renderer, AudioOutput& audio, const Vfs& vfs, const AppSettings& settings,
            const LiveOptions& options);

// Plays a click track and measures the mean tap offset; stores it in settings.timing.audio_offset_ms.
// Returns true if a value was measured.
bool RunCalibration(SDL_Window* window, Renderer& renderer, AudioOutput& audio, AppSettings& settings, long max_frames);

}
