// SIF port entry point: settings, game-folder check and Vfs mount, renderer, audio; then one of
//   - the M6 boot flow: --title (logos, notice, title screen; confirming starts the --live song, default 10:4),
//     with --lang en|cn|tw|jp (UI language, default en); --menu starts at the top menu, --select at the song select;
//   - a developer view of one UI layout: --ui SCREEN:LAYOUT[:CLIP] (e.g. 10_top_menu:uil0001_TopMenu00:a003), with
//     --ui-hide NODE,... (nodes hidden in every part) and --ui-hide-path PATH,... (placed instances hidden);
//   - the M4 live slice: --live ID:VARIANT (e.g. 10:4 = song 0010, ac_challenge), with --autoplay;
//   - calibration: --calibrate (tap Space to a click track; stores timing.audio_offset_ms);
//   - the M3 check: one game texture (--show PATH) and one cue (--play SHEET:CUE, or "none"); --movie PATH plays a
//     USM movie there instead of the texture, looping (M8).
// Other options: --frames N (close after N frames), --crash (forced access violation), --data DIR (log, settings and
// dump folder; default <exe>/data), --game DIR (the CUSA24620 dump; saved to settings).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "app/costume_screen.h"
#include "app/fade.h"
#include "app/first_run.h"
#include "app/input.h"
#include "app/live_screen.h"
#include "app/mission_screen.h"
#include "app/options_screen.h"
#include "app/profile_screen.h"
#include "app/result_screen.h"
#include "app/route.h"
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
#include "engine/video/movie.h"
#include "game/costume/costume.h"
#include "game/dlc/dlc.h"
#include "game/live/song.h"
#include "game/mission/mission.h"
#include "game/save/save.h"

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
    std::filesystem::path route, shots;  // --route FILE (M9): a scripted walkthrough; --shots DIR: where its shots go
    bool hidden = false;                 // --hidden: no visible window (routes)
    std::string movie;  // --movie: a USM played in place of the --show texture, looping (M8 check)
    std::string play = kDefaultCue;
    bool live = false, calibrate = false, title = false, menu = false, select = false, result_demo = false;
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
        else if (!std::strcmp(argv[i], "--movie") && i + 1 < argc) o.movie = argv[++i];
        else if (!std::strcmp(argv[i], "--route") && i + 1 < argc) o.route = sif::os::PathFromUtf8(argv[++i]);
        else if (!std::strcmp(argv[i], "--shots") && i + 1 < argc) o.shots = sif::os::PathFromUtf8(argv[++i]);
        else if (!std::strcmp(argv[i], "--hidden")) o.hidden = true;
        else if (!std::strcmp(argv[i], "--play") && i + 1 < argc) o.play = argv[++i];
        else if (!std::strcmp(argv[i], "--autoplay")) o.live_options.autoplay = true;
        else if (!std::strcmp(argv[i], "--calibrate")) o.calibrate = true;
        else if (!std::strcmp(argv[i], "--title")) o.title = true;
        else if (!std::strcmp(argv[i], "--menu")) o.menu = true;
        else if (!std::strcmp(argv[i], "--select")) o.select = true;
        else if (!std::strcmp(argv[i], "--result-demo")) o.result_demo = true;
        else if (!std::strcmp(argv[i], "--ui") && i + 1 < argc) o.ui = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-hide") && i + 1 < argc) o.ui_hide = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-hide-path") && i + 1 < argc) o.ui_hide_path = argv[++i];
        else if (!std::strcmp(argv[i], "--lang") && i + 1 < argc) o.lang = argv[++i];
        else if (!std::strcmp(argv[i], "--stage") && i + 1 < argc) o.live_options.stage = argv[++i];
        else if (!std::strcmp(argv[i], "--costume") && i + 1 < argc) o.live_options.costume = argv[++i];
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
    // no mode chosen (e.g. started by double-click): play the game from its title screen; the M3 texture/cue check
    // runs only when --show, --play or --movie asks for it (M8 V6)
    bool check = false;
    for (int i = 1; i < argc; ++i) {
        check |= !std::strcmp(argv[i], "--show") || !std::strcmp(argv[i], "--play") || !std::strcmp(argv[i], "--movie") ||
                 !std::strcmp(argv[i], "--crash");
    }
    if (!check && !o.live && !o.calibrate && !o.title && !o.menu && !o.select && !o.result_demo && o.ui.empty()) {
        o.title = true;
    }
    if (o.data_dir.empty()) o.data_dir = sif::os::ExecutableFile().parent_path() / "data";
    return o;
}

// Logs the missions this live completed (done in `after`, not in `before`). shortcut: logged only; the game's mission
// screen and result pop-up are not built
// The choosable nameplate items: a starting accolade, the three group badges and the layout's own name plate, then the
// claimed mission rewards (type 4 accolades and badges, type 6 name plates), then the saved choice if it is none of
// those. shortcut: the starters are accolade 2 "新入生" (newcomer) and badges
// 901001-903001 ("<group> 大好き"), Hypothesis (not traced in the eboot); the game's other badge unlocks are not known.
sif::Nameplate MakeNameplate(const sif::mission::Missions& missions, const std::vector<sif::mission::Award>& accolades,
                             const std::vector<sif::mission::Award>& badges, const sif::save::SaveData& save) {
    sif::Nameplate plate{save.player};
    const auto add = [](std::vector<sif::NameplateItem>& list, const std::vector<sif::mission::Award>& from, int id) {
        const sif::mission::Award* a = sif::mission::FindAward(from, id);
        const bool listed = std::any_of(list.begin(), list.end(), [&](const sif::NameplateItem& i) { return i.id == id; });
        if (a && !listed) list.push_back({a->id, a->group, a->name});
    };
    add(plate.accolades, accolades, 2);
    for (int id : {901001, 902001, 903001}) add(plate.badges, badges, id);
    for (int id : sif::mission::Owned(save, 4)) {
        add(plate.accolades, accolades, id);
        add(plate.badges, badges, id);
    }
    for (int id : sif::mission::Owned(save, 3)) add(plate.badges, badges, id);  // type 3: badges (5003 "どさんこ")
    add(plate.accolades, accolades, save.accolade);
    add(plate.badges, badges, save.badge);
    // name plates: the id is the picture (name_plate_PPP_SSSS); shortcut: no names (the plates have none in the table)
    // the starting plate: the pink "Wai-Wai!" (name_plate_001_9001, the first s_name_plate_datas row), as the game's
    // Change Accolade or Name Plate screen shows on a new save (shadPS4, M7)
    plate.name_plates.push_back({19001, 19001, ""});
    std::vector<int> plates = sif::mission::Owned(save, 6);
    if (save.name_plate) plates.push_back(save.name_plate);
    for (int id : plates) {
        if (std::none_of(plate.name_plates.begin(), plate.name_plates.end(), [&](const auto& i) { return i.id == id; })) {
            plate.name_plates.push_back({id, id, ""});
        }
    }
    const auto index = [](const std::vector<sif::NameplateItem>& list, int id) {
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].id == id) return i;
        }
        return size_t(0);
    };
    plate.accolade = index(plate.accolades, save.accolade);
    plate.badge = index(plate.badges, save.badge);
    plate.name_plate = index(plate.name_plates, save.name_plate);
    return plate;
}

void LogNewMissions(const sif::mission::Missions& missions, const std::vector<sif::mission::Award>& accolades,
                    const std::vector<sif::live::SongInfo>& songs, const std::vector<sif::live::DressInfo>& dresses,
                    int language, const sif::save::SaveData& before,
                    const sif::save::SaveData& after) {
    for (const sif::mission::Mission& m : missions.rows) {
        if (size_t(m.Kind()) >= missions.conditions.size() || !sif::mission::ProgressOf(m, after).Done() ||
            sif::mission::ProgressOf(m, before).Done()) {
            continue;
        }
        std::string p1 = std::to_string(m.p1);
        for (const auto& s : songs) {
            if (m.Kind() >= 35 && m.Kind() <= 39 && s.id == m.p1) p1 = s.title;
        }
        for (const auto& d : dresses) {
            if (m.Kind() >= 40 && d.id == m.p1) p1 = d.name;
        }
        const auto& c = missions.conditions[size_t(m.Kind())];
        const sif::mission::Award* accolade = m.reward_type == 4 ? sif::mission::FindAward(accolades, m.reward_id) : nullptr;
        sif::LogInfo("mission {} done: {} ({}){}", m.id, sif::mission::Fill(c.title[size_t(language)], m, p1),
                     sif::mission::Fill(c.text[size_t(language)], m, p1),
                     accolade ? "; accolade " + std::to_string(accolade->id) + " " + accolade->name : std::string());
    }
}

int Quit(int code, const char* why) {
    if (const int routed = sif::route::ExitCode(code); routed != code || sif::route::Active()) {  // a route decides
        why = routed ? "route failed" : "route passed";                                         // 0 pass, 3 fail
        code = routed;
    }
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
    options.live_options.approach_ms = settings.play.approach_ms;
    {
        sif::PadSlots slots = sif::DefaultPadSlots();
        if (!settings.controls.pad_lanes.empty() && !sif::PadSlotsFromText(settings.controls.pad_lanes, slots)) {
            sif::LogWarn("settings: controls.pad_lanes '{}' is not 10 button names; using the defaults",
                         settings.controls.pad_lanes);
        }
        sif::SetPadSlots(slots);
    }
    if (!options.game.empty()) settings.game.folder = options.game;

    sif::Vfs vfs;
    std::string why;
    // first run, or a folder that is no longer a dump: explain and let the user choose it (M8 V5)
    if (settings.game.folder.empty() || !sif::IsSifDump(sif::os::PathFromUtf8(settings.game.folder), &why)) {
        const std::string problem = settings.game.folder.empty()
                                        ? "No game folder is set yet."
                                        : "The game folder is not a CUSA24620 dump (" + why + "):\n" + settings.game.folder;
        sif::LogWarn("game folder: {}", problem);
        std::string chosen;
        if (SDL_Init(SDL_INIT_VIDEO)) chosen = sif::ChooseGameFolder(settings.game.folder, problem);
        if (chosen.empty()) {
            sif::LogExit(2, "no game folder");
            return 2;
        }
        settings.game.folder = chosen;
    }
    const auto game_root = sif::os::PathFromUtf8(settings.game.folder);
    if (!vfs.Mount(game_root)) {
        const std::string text = "The game folder cannot be read:\n" + settings.game.folder;
        sif::LogError("game folder: {}", text);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SIF port", text.c_str(), nullptr);
        sif::LogExit(2, "game folder unreadable");
        return 2;
    }
    sif::LogInfo("game folder: {}", settings.game.folder);
    if (!sif::SaveAppSettings(settings_path, settings)) sif::LogWarn("settings: cannot write {}", sif::os::PathToUtf8(settings_path));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        sif::LogError("SDL_Init: {}", SDL_GetError());
        sif::LogExit(1, "SDL_Init failed");
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("SIF port", settings.display.width, settings.display.height,
                                          SDL_WINDOW_RESIZABLE | (settings.display.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) |
                                              (options.hidden ? SDL_WINDOW_HIDDEN : 0));
    if (!window) {
        sif::LogError("SDL_CreateWindow: {}", SDL_GetError());
        return Quit(1, "window failed");
    }
    sif::LogInfo("window created {}x{}{}", settings.display.width, settings.display.height, options.hidden ? " (hidden)" : "");
    if (!options.route.empty()) {
        const auto shots = options.shots.empty() ? options.data_dir / "shots" : options.shots;
        if (!sif::route::Start(options.route, window, shots, &why)) {
            sif::LogError("route: {}", why);
            SDL_DestroyWindow(window);
            SDL_Quit();
            sif::LogExit(3, "route not readable");
            return 3;
        }
    }

    sif::Renderer renderer;
    if (!renderer.Init(window, settings.display.vsync, &why)) {
        sif::LogError("renderer: {}", why);
        SDL_DestroyWindow(window);
        return Quit(1, "renderer failed");
    }

    sif::AudioOutput audio;
    const bool audio_ok = audio.Open(settings.audio.buffer_frames, settings.audio.volume, &why);
    if (!audio_ok) sif::LogError("audio: {}", why);  // keep running without sound

    if (options.result_demo) {  // developer: the result screen with song 0010/04's autoplay values
        sif::LiveResult demo{10, 4, {583, 0, 0, 0, 0}, 583, 199220, true, false, 1.00001, 4};
        sif::RunResult(window, renderer, vfs, options.lang == "jp" ? "" : options.lang, demo, options.frames, {true, true, true});
        audio.Close();
        renderer.Shutdown();
        SDL_DestroyWindow(window);
        return Quit(0, "normal end");
    }
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
        // title -> top menu -> song select -> live -> result -> song select ...; Esc in the song select goes back to the
        // top menu, Esc there or closing the window ends. shortcut: every top-menu entry leads to the song select
        const std::string lang = options.lang == "jp" ? "" : options.lang;
        const bool boot = options.title || options.menu || options.select;
        if (boot && audio_ok && (!options.title || sif::RunTitle(window, renderer, vfs, lang, options.frames))) {
            bool open = true;
            sif::SongChoice last;
            // the port's own save; one that cannot be read is left alone (played without saving), never overwritten
            const auto save_path = options.data_dir / "save.txt";
            sif::save::SaveData save;
            std::string save_error;
            const bool can_save = sif::save::Load(save_path, save, &save_error);
            if (!can_save) sif::LogWarn("save: {}: {}; playing without saving", sif::os::PathToUtf8(save_path), save_error);
            sif::LoadingScreen loading;
            loading.Load(renderer, vfs, lang);
            const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
            sif::mission::Missions missions;
            std::vector<sif::live::SongInfo> songs;
            if (!sif::mission::Load(vfs, missions, &why)) sif::LogWarn("missions: {}", why);
            std::vector<sif::mission::Award> accolades, badges;
            if (!sif::mission::LoadAwards(vfs, accolades, badges, &why)) sif::LogWarn("awards: {}", why);
            if (!sif::live::LoadSongList(vfs, language, songs, &why)) sif::LogWarn("missions: songs: {}", why);
            std::vector<sif::live::DressInfo> dresses;
            if (!sif::live::LoadDressList(vfs, language, dresses, &why)) sif::LogWarn("missions: costumes: {}", why);
            sif::costume::Data costume_data;
            const bool has_costumes = sif::costume::Load(vfs, costume_data, &why);
            if (!has_costumes) sif::LogWarn("costumes: {}", why);
            std::mt19937 drop_random{std::random_device{}()};
            // add-on ownership from the user's own add-ons; without the catalog nothing is locked
            sif::dlc::Ownership owned;
            sif::dlc::Catalog catalog;
            if (sif::dlc::LoadCatalog(vfs, catalog, &why)) {
                std::vector<std::filesystem::path> dlc_folders;
                if (!settings.game.dlc_folder.empty()) {
                    dlc_folders.push_back(sif::os::PathFromUtf8(settings.game.dlc_folder));
                } else {
                    for (const char* sibling : {"Extracted", "DLC"}) dlc_folders.push_back(game_root.parent_path() / sibling);
                }
                const auto labels = sif::dlc::ScanOwned(dlc_folders);
                owned = sif::dlc::Resolve(catalog, labels);
                sif::LogInfo("add-ons: {} found; {} of {} add-on songs and {} of {} add-on costumes locked", labels.size(),
                             owned.locked_songs.size(), catalog.songs.size(), owned.locked_costumes.size(),
                             catalog.costumes.size());
            } else {
                sif::LogWarn("add-ons: {}", why);
            }
            for (bool menu = !options.select; open;) {
                if (menu) loading.Show(renderer);
                if (menu) {
                    int mode = -1;
                    sif::Nameplate plate = MakeNameplate(missions, accolades, badges, save);
                    const int entry = sif::RunTopMenu(window, renderer, vfs, lang, options.frames, &mode, &audio, &plate);
                    if (entry < 0) break;
                    if (mode == 4 && entry == 15) {  // Collection: Profile Card (Change Accolade or Name Plate)
                        loading.Show(renderer);
                        const size_t shown_accolade = plate.accolade, shown_badge = plate.badge, shown_plate = plate.name_plate;
                        open = sif::RunProfile(window, renderer, vfs, lang, plate, options.frames);
                        // only a real choice is saved
                        if (plate.accolade != shown_accolade || plate.badge != shown_badge || plate.name_plate != shown_plate) {
                            save.accolade = plate.accolades[plate.accolade].id, save.badge = plate.badges[plate.badge].id;
                            save.name_plate = plate.name_plates[plate.name_plate].id;
                            if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                        }
                        continue;
                    }
                    // Options (mode 3): entry 13 Sound, 10 Game Settings, 12 Control Settings (straight to the 9-lane
                    // Button Controls); the others still lead to the song select (shortcut: Costumes are not built)
                    if (mode == 3 && (entry == 13 || entry == 10 || entry == 12)) {
                        loading.Show(renderer);
                        open = sif::RunOptions(window, renderer, vfs, lang, audio, settings,
                                               entry == 13   ? sif::OptionsPage::kSound
                                               : entry == 10 ? sif::OptionsPage::kGame
                                                             : sif::OptionsPage::kButtons,
                                               options.frames);
                        options.live_options.approach_ms = settings.play.approach_ms;
                        if (!sif::SaveAppSettings(settings_path, settings)) {
                            sif::LogWarn("settings: cannot write {}", sif::os::PathToUtf8(settings_path));
                        }
                        continue;
                    }
                    if (mode == 1 && entry == 5 && has_costumes) {  // Costumes: Create/Change Costumes
                        loading.Show(renderer);
                        std::vector<std::pair<int, std::string>> names;
                        for (const auto& d : dresses) names.push_back({d.id, d.name});
                        open = sif::RunCostumes(window, renderer, vfs, lang, costume_data, names, &owned, save, options.frames);
                        if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                        continue;
                    }
                    if (mode == 1 && entry == 7 && has_costumes) {  // Costumes: Costume Materials
                        loading.Show(renderer);
                        open = sif::RunMaterials(window, renderer, vfs, lang, costume_data, save, options.frames);
                        continue;
                    }
                    if (mode == 5 && entry == 20) {  // Event: Task List
                        loading.Show(renderer);
                        open = sif::RunMissions(window, renderer, vfs, lang,
                                                {&missions, &accolades, &songs, &save, &owned}, options.frames);
                        if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                        continue;
                    }
                }
                loading.Show(renderer);
                const sif::SongChoice choice = sif::RunSongSelect(window, renderer, vfs, lang, options.frames, last, &save, &owned,
                                                                  has_costumes ? &costume_data : nullptr);
                if (choice.closed || (choice.song < 0 && options.select)) break;
                menu = choice.song < 0;
                if (menu) continue;
                last = choice;
                options.live_options.song = choice.song;
                options.live_options.variant = choice.variant;
                if (!choice.stage.empty()) options.live_options.stage = choice.stage;
                options.live_options.costume = choice.costume;
                sif::LiveResult result;
                loading.Show(renderer);
                options.live_options.wear.clear();  // each member's own costume (Create/Change Costumes)
                for (const auto& [member, dress] : save.wear) {
                    char folder[16];
                    std::snprintf(folder, sizeof(folder), "d%03d", dress);
                    options.live_options.wear[member] = folder;
                }
                code = sif::RunLive(window, renderer, audio, vfs, settings, options.live_options, &result);
                sif::save::NewBests news;
                if (code == 0 && !result.closed && result.finished) {
                    loading.Show(renderer);
                    const bool full_combo = result.counts[2] + result.counts[3] + result.counts[4] == 0;
                    const sif::save::SaveData before = save;
                    const int costume = result.costume.size() == 4 && result.costume[0] == 'd'
                                            ? std::atoi(result.costume.c_str() + 1) : -1;
                    news = sif::save::Record(save, {result.song, result.variant, int(result.score), result.max_combo, result.rank,
                                                    result.clear_rate, full_combo, result.counts[0], result.counts[1], costume});
                    if (has_costumes) {  // the song's materials (costume::RollDrops)
                        std::string got;
                        for (const auto& [item, amount] : sif::costume::RollDrops(costume_data, result.song, drop_random, save)) {
                            const sif::costume::Item* i = sif::costume::FindItem(costume_data, item);
                            got += (got.empty() ? "" : ", ") + (i ? i->name[size_t(language)] : std::to_string(item)) + " x" +
                                   std::to_string(amount);
                        }
                        sif::LogInfo("materials: {}", got.empty() ? "none" : got);
                    }
                    if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                    LogNewMissions(missions, accolades, songs, dresses, language, before, save);
                }
                open = code == 0 && !result.closed &&
                       (!result.finished || sif::RunResult(window, renderer, vfs, options.live_options.lang, result,
                                                           options.frames, news));
            }
            loading.Release(renderer);
        } else if (boot) {
            code = audio_ok ? 0 : 1;
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

    sif::video::Movie movie;
    sif::Texture movie_texture;
    if (!options.movie.empty()) {
        if (!movie.Open(vfs, options.movie, &why) ||
            !renderer.CreateTexture(uint32_t(movie.Info().width), uint32_t(movie.Info().height), movie_texture, &why)) {
            sif::LogError("movie {}: {}", options.movie, why);
            movie.Close();
        } else {
            const auto& i = movie.Info();
            sif::LogInfo("movie {}: {}x{}, {:.3f} fps, {} frames", options.movie, i.width, i.height, i.fps, i.frames);
        }
    }
    double movie_ms = 0;  // decode + upload time, for the frame-cost note
    long movie_updates = 0;

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
        while (sif::PollEvent(event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }
        if (movie.IsOpen()) {  // the picture for the time since start, looping; uploaded only when it changes
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const int n = int(t * movie.Info().fps) % movie.Info().frames;
            if (n != movie.Current()) {
                const auto t0 = std::chrono::steady_clock::now();
                if (const auto* rgba = movie.Frame(n)) renderer.UpdateTexture(movie_texture, *rgba);
                movie_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                ++movie_updates;
            }
        }
        if (!renderer.DrawFrame(movie.IsOpen() ? &movie_texture : &texture)) SDL_Delay(10);  // minimized: nothing to present
        if (const auto now = std::chrono::steady_clock::now(); now >= next_report) {
            // fed frames vs wall clock: the gap is what SDL and the device still hold (input to M4's clock design)
            const double wall = std::chrono::duration<double>(now - start).count();
            sif::LogInfo("t={:.1f} s: {} frames drawn, audio fed {} frames ({:.3f} s)", wall, frame, audio.FedFrames(),
                         double(audio.FedFrames()) / 48000.0);
            next_report += std::chrono::seconds(5);
        }
    }
    sif::LogInfo("frames drawn: {}", frame);
    if (movie_updates) {
        sif::LogInfo("movie: {} pictures shown, decode + upload {:.2f} ms each on average", movie_updates,
                     movie_ms / double(movie_updates));
    }

    audio.Close();
    renderer.Release(movie_texture);
    renderer.Release(texture);
    renderer.Shutdown();
    SDL_DestroyWindow(window);
    return Quit(0, "normal end");
}
