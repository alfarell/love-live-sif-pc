#pragma once
// Live screens: one song played on 9 lanes over the 3D stage with the members dancing (M5), and an input-offset
// calibration.

#include <cstdint>
#include <string>

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
    // shortcut: the song's stage is not traced (analysis/202611011121-m5-live-presentation/); chosen here, default the
    // live hall. The line-up is the members who have a dance file for the song
    std::string stage = "st017";
    int member = 0;  // only chr/cNN (1-9 = mu's); 0 = every member who dances the song
    long max_frames = -1;
    std::string lang = "en";  // UI language of the in-game HUD (ui2/90_ingame_<lang>): en, cn, tw or "" (Japanese)
};

// Returns 0 when the song ends or the window is closed, 1 on a load error.
int RunLive(SDL_Window* window, Renderer& renderer, AudioOutput& audio, const Vfs& vfs, const AppSettings& settings,
            const LiveOptions& options);

// Plays a click track and measures the mean tap offset; stores it in settings.timing.audio_offset_ms.
// Returns true if a value was measured.
bool RunCalibration(SDL_Window* window, Renderer& renderer, AudioOutput& audio, AppSettings& settings, long max_frames);

}
