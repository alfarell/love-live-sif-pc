#pragma once
// SIF port settings (settings.ini in the data folder). The INI parser in settings.cpp is copied from pt-pc;
// the settings struct is SIF's own.

#include <filesystem>
#include <string>

namespace sif {

struct AppSettings {
    struct Display {
        int width = 1280;
        int height = 720;
        int fullscreen = 0;
        bool vsync = true;
    } display;
    struct Audio {
        float volume = 1.0f;
        std::string output_device;  // empty = system default
        int buffer_frames = 256;    // device buffer request; small for rhythm-game latency
    } audio;
    struct Game {
        std::string folder;  // the user's CUSA24620 dump; empty = ask / detect
        // the user's own add-ons (extracted folders with sce_sys/param.sfo, or .pkg files); empty = the "Extracted"
        // and "DLC" folders next to the dump, if there are any
        std::string dlc_folder;
    } game;
    struct Timing {
        int audio_offset_ms = 0;  // calibration: positive = notes later
        int input_offset_ms = 0;
    } timing;
    struct Play {
        int approach_ms = 1300;  // note travel time from the centre circle to the ring (note speed)
    } play;
    struct Controls {
        std::string pad_lanes;  // the pad's 9-lane Button Controls, 10 button names; empty = the game's defaults
    } controls;
};

bool LoadAppSettings(const std::filesystem::path& path, AppSettings& out);
bool SaveAppSettings(const std::filesystem::path& path, const AppSettings& settings);

}
