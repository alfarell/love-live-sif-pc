// SIF port entry point (M3): settings, game-folder check and Vfs mount, renderer, one game texture on screen, one
// cue played through the CRI path.
// Options: --frames N (close after N frames, for tests), --crash (force an access violation after the window opens),
//          --data DIR (log, settings and dump folder; default <exe>/data), --game DIR (the CUSA24620 dump; saved to
//          settings), --show PATH (Vfs path of a btx texture to draw), --play SHEET:CUE (e.g. "sys:11_gatya";
//          "none" to stay silent).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "engine/audio/audio_output.h"
#include "engine/audio/sound_bank.h"
#include "engine/core/crash_report.h"
#include "engine/core/log.h"
#include "engine/formats/btx.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "engine/platform/settings.h"
#include "engine/render/renderer.h"

#ifndef SIF_BUILD_ID
#define SIF_BUILD_ID "dev"
#endif

namespace {

// a member card picture (BGRA8, 672x1100): a quick visual check that the dump, Vfs and renderer all work
constexpr const char* kDefaultTexture = "ui/member_card/000/mbc1000.pac/mbc_1000_chara";
// song 0010 (its chart is the one studied in M2); its wave is in the add-on bank sound/dlc/MUSIC_0010.awb
constexpr const char* kDefaultCue = "music:僕らのLIVE君とのLIFE";

struct Options {
    long frames = -1;
    bool crash = false;
    std::filesystem::path data_dir;
    std::string game;
    std::string show = kDefaultTexture;
    std::string play = kDefaultCue;
};

Options ParseOptions(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) o.frames = std::strtol(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--crash")) o.crash = true;
        else if (!std::strcmp(argv[i], "--data") && i + 1 < argc) o.data_dir = sif::os::PathFromUtf8(argv[++i]);
        else if (!std::strcmp(argv[i], "--game") && i + 1 < argc) o.game = argv[++i];
        else if (!std::strcmp(argv[i], "--show") && i + 1 < argc) o.show = argv[++i];
        else if (!std::strcmp(argv[i], "--play") && i + 1 < argc) o.play = argv[++i];
    }
    if (o.data_dir.empty()) o.data_dir = sif::os::ExecutableFile().parent_path() / "data";
    return o;
}

int Quit(int code, const char* why) {
    SDL_Quit();
    sif::LogExit(code, why);
    return code;
}

}

int main(int argc, char** argv) {
    const Options options = ParseOptions(argc, argv);
    std::error_code ec;
    std::filesystem::create_directories(options.data_dir, ec);
    sif::LogSetFile(options.data_dir / "sif.log");
    sif::InstallCrashReporting(options.data_dir, SIF_BUILD_ID);
    sif::LogInfo("sif-port {} starting, data folder {}", SIF_BUILD_ID, sif::os::PathToUtf8(options.data_dir));

    const auto settings_path = options.data_dir / "settings.ini";
    sif::AppSettings settings;
    if (!sif::LoadAppSettings(settings_path, settings)) sif::LogInfo("settings: none yet, using defaults");
    if (!options.game.empty()) settings.game.folder = options.game;

    sif::Vfs vfs;
    std::string why;
    const auto game_root = sif::os::PathFromUtf8(settings.game.folder);
    if (settings.game.folder.empty() || !sif::IsSifDump(game_root, &why) || !vfs.Mount(game_root)) {
        const std::string text = settings.game.folder.empty()
                                     ? "No game folder set. Start with --game <your CUSA24620 dump folder>."
                                     : "The game folder is not a CUSA24620 dump (" + why + "):\n" + settings.game.folder;
        sif::LogError("game folder: {}", text);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SIF port", text.c_str(), nullptr);
        sif::LogExit(2, "no game folder");
        return 2;
    }
    sif::LogInfo("game folder: {}", settings.game.folder);
    if (!sif::SaveAppSettings(settings_path, settings)) sif::LogWarn("settings: cannot write {}", sif::os::PathToUtf8(settings_path));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        sif::LogError("SDL_Init: {}", SDL_GetError());
        sif::LogExit(1, "SDL_Init failed");
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("SIF port", settings.display.width, settings.display.height,
                                          SDL_WINDOW_RESIZABLE | (settings.display.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    if (!window) {
        sif::LogError("SDL_CreateWindow: {}", SDL_GetError());
        return Quit(1, "window failed");
    }
    sif::LogInfo("window created {}x{}", settings.display.width, settings.display.height);

    sif::Renderer renderer;
    if (!renderer.Init(window, settings.display.vsync, &why)) {
        sif::LogError("renderer: {}", why);
        SDL_DestroyWindow(window);
        return Quit(1, "renderer failed");
    }

    sif::Texture texture;
    {
        std::vector<uint8_t> file;
        sif::btx::Image image;
        const auto t0 = std::chrono::steady_clock::now();
        if (!vfs.Read(options.show, file, &why) || !sif::btx::Parse(file, image, &why) || !renderer.Upload(image, texture, &why)) {
            sif::LogError("texture {}: {}", options.show, why);
        } else {
            const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            sif::LogInfo("texture {}: {}x{}, {} mips, loaded in {:.1f} ms", options.show, image.width, image.height,
                         image.levels.size(), ms);
        }
    }

    sif::AudioOutput audio;
    if (!audio.Open(settings.audio.buffer_frames, settings.audio.volume, &why)) {
        sif::LogError("audio: {}", why);  // keep running without sound
    } else if (const size_t colon = options.play.find(':'); options.play != "none" && colon != std::string::npos) {
        const std::string sheet_name = options.play.substr(0, colon), cue = options.play.substr(colon + 1);
        sif::cri::CueSheet sheet;
        sif::cri::Pcm pcm;
        const auto t0 = std::chrono::steady_clock::now();
        if (!sif::cri::LoadCueSheet(vfs, sheet_name, sheet, &why) || !sif::cri::LoadCue(vfs, sheet, cue, pcm, &why)) {
            sif::LogError("cue {}: {}", options.play, why);
        } else {
            const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            sif::LogInfo("cue {}: {} Hz, {} ch, {:.1f} s, decoded in {:.0f} ms", options.play, pcm.rate, pcm.channels,
                         double(pcm.Frames()) / pcm.rate, ms);
            audio.Play(std::move(pcm));
        }
    }

    if (options.crash) {
        sif::LogInfo("crash test: access violation");
        *static_cast<volatile int*>(nullptr) = 1;
    }

    bool running = true;
    long frame = 0;
    const auto start = std::chrono::steady_clock::now();
    auto next_report = start + std::chrono::seconds(5);
    for (; running && (options.frames < 0 || frame < options.frames); ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }
        if (!renderer.DrawFrame(&texture)) SDL_Delay(10);  // minimized: nothing to present
        if (const auto now = std::chrono::steady_clock::now(); now >= next_report) {
            // fed frames vs wall clock: the gap is what SDL and the device still hold (input to M4's clock design)
            const double wall = std::chrono::duration<double>(now - start).count();
            sif::LogInfo("t={:.1f} s: {} frames drawn, audio fed {} frames ({:.3f} s)", wall, frame, audio.FedFrames(),
                         double(audio.FedFrames()) / 48000.0);
            next_report += std::chrono::seconds(5);
        }
    }
    sif::LogInfo("frames drawn: {}", frame);

    audio.Close();
    renderer.Release(texture);
    renderer.Shutdown();
    SDL_DestroyWindow(window);
    return Quit(0, "normal end");
}
