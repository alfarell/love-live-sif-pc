// SIF port entry point: settings, game-folder check and Vfs mount, renderer, audio; then one of
//   - the M6 boot flow: --title (logos, notice, title screen; confirming starts the --live song, default 10:4),
//     with --lang en|cn|tw|jp (UI language, default en); --menu starts at the top menu, --select at the song select;
//   - a developer view of one UI layout: --ui SCREEN:LAYOUT[:CLIP] (e.g. 10_top_menu:uil0001_TopMenu00:a003), with
//     --ui-hide NODE,... (nodes hidden in every part) and --ui-hide-path PATH,... (placed instances hidden);
//   - the M4 live slice: --live ID:VARIANT (e.g. 10:4 = song 0010, ac_challenge), with --autoplay;
//   - calibration: --calibrate (tap Space to a click track; stores timing.audio_offset_ms);
//   - the M3 check: one game texture (--show PATH) and one cue (--play SHEET:CUE, or "none").
// Other options: --frames N (close after N frames), --crash (forced access violation), --data DIR (log, settings and
// dump folder; default <exe>/data), --game DIR (the CUSA24620 dump; saved to settings).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "app/live_screen.h"
#include "app/song_select.h"
#include "app/title_screen.h"
#include "app/top_menu.h"
#include "app/ui_viewer.h"
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
    bool live = false, calibrate = false, title = false, menu = false, select = false;
    std::string ui, ui_hide, ui_hide_path;
    std::string lang = "en";
    sif::LiveOptions live_options;
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
        else if (!std::strcmp(argv[i], "--autoplay")) o.live_options.autoplay = true;
        else if (!std::strcmp(argv[i], "--calibrate")) o.calibrate = true;
        else if (!std::strcmp(argv[i], "--title")) o.title = true;
        else if (!std::strcmp(argv[i], "--menu")) o.menu = true;
        else if (!std::strcmp(argv[i], "--select")) o.select = true;
        else if (!std::strcmp(argv[i], "--ui") && i + 1 < argc) o.ui = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-hide") && i + 1 < argc) o.ui_hide = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-hide-path") && i + 1 < argc) o.ui_hide_path = argv[++i];
        else if (!std::strcmp(argv[i], "--lang") && i + 1 < argc) o.lang = argv[++i];
        else if (!std::strcmp(argv[i], "--stage") && i + 1 < argc) o.live_options.stage = argv[++i];
        else if (!std::strcmp(argv[i], "--member") && i + 1 < argc) o.live_options.member = int(std::strtol(argv[++i], nullptr, 10));
        else if (!std::strcmp(argv[i], "--live") && i + 1 < argc) {
            o.live = true;
            char* rest = nullptr;
            o.live_options.song = int(std::strtol(argv[++i], &rest, 10));
            if (rest && *rest == ':') o.live_options.variant = int(std::strtol(rest + 1, nullptr, 10));
        }
    }
    o.live_options.max_frames = o.frames;
    o.live_options.lang = o.lang == "jp" ? "" : o.lang;
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
    Options options = ParseOptions(argc, argv);
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

    sif::AudioOutput audio;
    const bool audio_ok = audio.Open(settings.audio.buffer_frames, settings.audio.volume, &why);
    if (!audio_ok) sif::LogError("audio: {}", why);  // keep running without sound

    if (!options.ui.empty()) {
        const int code = sif::RunUiViewer(window, renderer, vfs, options.ui, options.lang == "jp" ? "" : options.lang,
                                          options.frames, options.ui_hide, options.ui_hide_path);
        audio.Close();
        renderer.Shutdown();
        SDL_DestroyWindow(window);
        return Quit(code, code ? "ui viewer failed" : "normal end");
    }
    if (options.calibrate || options.live || options.title || options.menu || options.select) {
        int code = 0;
        // title -> top menu -> song select -> live; shortcut: every top-menu entry leads to the song select
        const std::string lang = options.lang == "jp" ? "" : options.lang;
        const bool boot = options.title || options.menu || options.select;
        if (boot) {
            options.live = false;
            if ((!options.title || sif::RunTitle(window, renderer, vfs, lang, options.frames)) &&
                (options.select || sif::RunTopMenu(window, renderer, vfs, lang, options.frames) >= 0)) {
                const sif::SongChoice choice = sif::RunSongSelect(window, renderer, vfs, lang, options.frames);
                if (choice.song >= 0) {
                    options.live = true;
                    options.live_options.song = choice.song;
                    options.live_options.variant = choice.variant;
                }
            }
        }
        if (boot && !options.live) {
            code = 0;
        } else if (!audio_ok) {
            code = 1;  // both modes need the audio clock
        } else if (options.calibrate) {
            if (sif::RunCalibration(window, renderer, audio, settings, options.frames) &&
                !sif::SaveAppSettings(settings_path, settings)) {
                sif::LogWarn("settings: cannot write {}", sif::os::PathToUtf8(settings_path));
            }
        } else {
            code = sif::RunLive(window, renderer, audio, vfs, settings, options.live_options);
        }
        audio.Close();
        renderer.Shutdown();
        SDL_DestroyWindow(window);
        return Quit(code, code ? "mode failed" : "normal end");
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

    if (const size_t colon = options.play.find(':'); audio_ok && options.play != "none" && colon != std::string::npos) {
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
