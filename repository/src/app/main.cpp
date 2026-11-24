// SIF port entry point: settings, game-folder check and Vfs mount, renderer, audio; then one of
//   - the M6 boot flow: --title (logos, notice, title screen; confirming starts the --live song, default 10:4),
//     with --lang en|cn|tw|jp (UI language, default en); --menu starts at the top menu, --select at the song select;
//   - a developer view of one UI layout: --ui SCREEN:LAYOUT[:CLIP] (e.g. 10_top_menu:uil0001_TopMenu00:a003), with
//     --ui-hide NODE,... (nodes hidden in every part) and --ui-hide-path PATH,... (placed instances hidden), --ui-centre
//     (a popup layout drawn at the centre);
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
#include <tuple>
#include <vector>

#include "app/collection_screen.h"
#include "app/costume_screen.h"
#include "app/event_screen.h"
#include "app/fade.h"
#include "app/first_run.h"
#include "app/input.h"
#include "app/live_intro.h"
#include "app/medley_live.h"
#include "app/medley_screen.h"
#include "app/live_screen.h"
#include "app/mission_screen.h"
#include "app/options_screen.h"
#include "app/profile_edit.h"
#include "app/profile_screen.h"
#include "app/skill_screen.h"
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
#include "engine/formats/png.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "engine/platform/settings.h"
#include "engine/render/renderer.h"
#include "engine/video/movie.h"
#include "game/card/card.h"
#include "game/costume/costume.h"
#include "game/dlc/dlc.h"
#include "game/event/event.h"
#include "game/friendship/friendship.h"
#include "game/live/song.h"
#include "game/medley/medley.h"
#include "game/member/member.h"
#include "game/mission/mission.h"
#include "game/profile/profile.h"
#include "game/save/save.h"
#include "game/text/messages.h"

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
    bool ui_centre = false;
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
        else if (!std::strcmp(argv[i], "--approach") && i + 1 < argc) o.live_options.approach_ms = std::strtod(argv[++i], nullptr);
        else if (!std::strcmp(argv[i], "--calibrate")) o.calibrate = true;
        else if (!std::strcmp(argv[i], "--title")) o.title = true;
        else if (!std::strcmp(argv[i], "--menu")) o.menu = true;
        else if (!std::strcmp(argv[i], "--select")) o.select = true;
        else if (!std::strcmp(argv[i], "--result-demo")) o.result_demo = true;
        else if (!std::strcmp(argv[i], "--ui") && i + 1 < argc) o.ui = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-hide") && i + 1 < argc) o.ui_hide = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-hide-path") && i + 1 < argc) o.ui_hide_path = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-centre")) o.ui_centre = true;
        else if (!std::strcmp(argv[i], "--lang") && i + 1 < argc) o.lang = argv[++i];
        else if (!std::strcmp(argv[i], "--stage") && i + 1 < argc) o.live_options.stage = argv[++i];
        else if (!std::strcmp(argv[i], "--costume") && i + 1 < argc) o.live_options.costume = argv[++i];
        else if (!std::strcmp(argv[i], "--member") && i + 1 < argc) o.live_options.member = int(std::strtol(argv[++i], nullptr, 10));
        else if (!std::strcmp(argv[i], "--date") && i + 1 < argc) {  // the in-game date (M14: birthday events in tests)
            sif::event::Date d;
            if (sif::event::ParseDate(argv[++i], d)) sif::event::SetToday(d);
        }
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
                    const sif::save::SaveData& after, const sif::mission::Context* context) {
    for (const sif::mission::Mission& m : missions.rows) {
        if (size_t(m.Kind()) >= missions.conditions.size() || !sif::mission::ProgressOf(m, after, context).Done() ||
            sif::mission::ProgressOf(m, before, context).Done()) {
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

// Saves the photos of one View Live Show (M12 P4c): each goes to profile::NextPhotoSlot (a free slot, else the oldest
// that is not a favourite) as photos/NN.png in the data folder, with a `photo` line in the save. The photo's member
// card is the played member's card in the costume she wore ((member x 1000 + costume) x 10, the R picture); its live
// is song x 1000 + variant, as the game's records (340011). shortcut: `stage` is the port's stage model number
// (stNNN), not the game's stage id
int PhotoMemberCard(const sif::LiveResult& result, int member, const sif::LiveOptions& live) {
    std::string dress = result.costume;
    if (const auto it = live.wear.find(member); it != live.wear.end()) dress = it->second;  // her own costume wins
    const int costume = dress.size() == 4 && dress[0] == 'd' ? std::atoi(dress.c_str() + 1) : 0;
    return (member * 1000 + costume) * 10;
}

// A save from before M15 has no per-group counts: they start from its records, each song's to its group (Saint Snow
// with Aqours). shortcut: the GREAT/PERFECT totals were kept for both groups together, so they go to the group with
// more plays. False when there is nothing to seed (counts already there, or no records).
bool SeedGroupCounters(sif::save::SaveData& save, const std::vector<sif::live::SongInfo>& songs) {
    if (save.records.empty() || sif::save::Count(save, "g0.plays") || sif::save::Count(save, "g1.plays")) return false;
    for (const auto& [chart, r] : save.records) {
        int g = 0;
        for (const auto& s : songs) {
            if (s.id == chart.first) g = s.group == 0 ? 0 : 1;
        }
        sif::save::Add(save, sif::save::GroupKey(g, "plays"), r.plays);
        sif::save::Add(save, sif::save::GroupKey(g, "full_combos"), r.full_combos);
        sif::save::Best(save, sif::save::GroupKey(g, "best_score"), r.best_score);
        sif::save::Best(save, sif::save::GroupKey(g, "best_rank"), r.best_rank + 1);
    }
    const int g = sif::save::Count(save, "g1.plays") > sif::save::Count(save, "g0.plays") ? 1 : 0;
    sif::save::Add(save, sif::save::GroupKey(g, "perfects"), int(std::min<long long>(save.perfects, 2000000000)));
    sif::save::Add(save, sif::save::GroupKey(g, "greats"), int(std::min<long long>(save.greats, 2000000000)));
    return true;
}

// The mission counters of the photos taken (M15 Q3: kinds 13, 33, 45), saved or not
void CountPhotos(const sif::LiveResult& result, int member, const sif::LiveOptions& live, sif::save::SaveData& save) {
    const int dress = PhotoMemberCard(result, member, live) / 10 % 1000;
    for (const sif::LiveResult::Photo& p : result.photos) {
        sif::save::Add(save, "photos");
        sif::save::Add(save, "photo." + std::to_string(dress));
        if (p.camera_member) sif::save::Mark(save, "camera_photo", p.camera_member);
    }
}

void SavePhotos(const std::filesystem::path& data_dir, sif::LiveResult& result, int member,
                const sif::LiveOptions& live, const std::vector<bool>& favourites, sif::save::SaveData& save) {
    std::error_code ec;
    std::filesystem::create_directories(data_dir / "photos", ec);
    const int card = PhotoMemberCard(result, member, live);
    int stage = 0;
    std::sscanf(live.stage.c_str(), "st%d", &stage);
    const long long now = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch()).count();
    for (size_t k = 0; k < result.photos.size(); ++k) {
        const sif::LiveResult::Photo& p = result.photos[k];
        const int slot = sif::profile::NextPhotoSlot(save);
        if (slot < 0) {
            sif::LogWarn("photos: every slot holds a favourite; photo not saved");
            break;
        }
        const std::filesystem::path file = data_dir / sif::os::PathFromUtf8(sif::profile::PhotoFile(slot));
        std::string error;
        if (!sif::png::Write(file, p.rgba, p.width, p.height, &error)) {
            sif::LogWarn("photos: {}", error);
            continue;
        }
        const bool favourite = k < favourites.size() && favourites[k];
        save.photos[slot] = {now, stage, card, result.song * 1000 + result.variant, favourite};
        sif::LogInfo("photos: slot {} = {} ({}x{}, member card {}, live {}{})", slot, sif::profile::PhotoFile(slot), p.width,
                     p.height, card, save.photos[slot].live, favourite ? ", favourite" : "");
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
    options.live_options.note_speed = settings.play.note_speed;
    {
        sif::PadSlots slots = sif::DefaultPadSlots();
        if (!settings.controls.pad_lanes.empty() && !sif::PadSlotsFromText(settings.controls.pad_lanes, slots)) {
            sif::LogWarn("settings: controls.pad_lanes '{}' is not 10 button names; using the defaults",
                         settings.controls.pad_lanes);
        }
        sif::SetPadSlots(slots);
        for (const auto& [config, text] : {std::pair{sif::HomeConfig::Easy, settings.controls.pad_lanes_home_easy},
                                           std::pair{sif::HomeConfig::Hard, settings.controls.pad_lanes_home_hard}}) {
            sif::HomeSlots home = sif::DefaultHomeSlots();
            if (!text.empty() && !sif::HomeSlotsFromText(text, home)) {
                sif::LogWarn("settings: a 6-lane pad mapping '{}' is not 6 button names; using the defaults", text);
            }
            sif::SetHomeSlots(config, home);
        }
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
                                          options.frames, options.ui_hide, options.ui_hide_path, options.ui_centre);
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
            bool home_mode = false;  // the song select's mode: top-menu entry 0 "Play Live Show" (home), else arcade
            bool view_mode = false;  // entry 2 "View Live Show" (M12 P4): the live without notes, for the camera
            // the port's own save; one that cannot be read is left alone (played without saving), never overwritten
            const auto save_path = options.data_dir / "save.txt";
            sif::save::SaveData save;
            std::string save_error;
            const bool can_save = sif::save::Load(save_path, save, &save_error);
            if (!can_save) sif::LogWarn("save: {}: {}; playing without saving", sif::os::PathToUtf8(save_path), save_error);
            sif::LoadingScreen loading;
            loading.Load(renderer, vfs, lang);
            const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
            sif::MedleyPlace medley_place;  // Live Show Medley: the tab and list last shown (M16 R5)
            sif::mission::Missions missions;
            std::vector<sif::live::SongInfo> songs;
            if (!sif::mission::Load(vfs, missions, &why)) sif::LogWarn("missions: {}", why);
            std::vector<sif::mission::Award> accolades, badges;
            if (!sif::mission::LoadAwards(vfs, accolades, badges, &why)) sif::LogWarn("awards: {}", why);
            if (!sif::live::LoadSongList(vfs, language, songs, &why)) sif::LogWarn("missions: songs: {}", why);
            if (SeedGroupCounters(save, songs)) sif::LogInfo("missions: per-group counts seeded from the records (M15)");
            std::vector<sif::live::DressInfo> dresses;
            if (!sif::live::LoadDressList(vfs, language, dresses, &why)) sif::LogWarn("missions: costumes: {}", why);
            sif::costume::Data costume_data;
            const bool has_costumes = sif::costume::Load(vfs, costume_data, &why);
            if (!has_costumes) sif::LogWarn("costumes: {}", why);
            sif::card::Data card_data;  // skill and member cards (M11)
            const bool has_cards = sif::card::Load(vfs, card_data, &why);
            if (!has_cards) sif::LogWarn("cards: {}", why);
            sif::event::Data event_data;  // birthday events (M14)
            const bool has_events = sif::event::Load(vfs, event_data, &why);
            if (!has_events) sif::LogWarn("birthday events: {}", why);
            sif::friendship::Tables friendship_data;  // Friendship levels and gains (M14)
            const bool has_friendship = sif::friendship::Load(vfs, friendship_data, &why);
            if (!has_friendship) sif::LogWarn("friendship: {}", why);
            // the played member: --member, else the save's current member (the navigator; R2/L2 on the top menu, M13)
            sif::SkillSetup skills{has_cards ? &card_data : nullptr, &save,
                                   options.live_options.member > 0 ? options.live_options.member : sif::save::Current(save)};
            std::mt19937 drop_random{std::random_device{}()};
            // the offline support cards (M12 P7): the NPC presets, drawn with the eboot's generator, one per game run
            sif::profile::Data profile_data;
            if (!sif::profile::Load(vfs, profile_data, &why)) sif::LogWarn("profile cards: {}", why);
            sif::profile::XorShift128 support_random;
            sif::Messages messages;
            if (!messages.Load(vfs, language, &why)) sif::LogWarn("messages: {}", why);
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
            const sif::mission::Context mission_context{has_friendship ? &friendship_data : nullptr,
                                                        has_cards ? &card_data : nullptr,
                                                        has_costumes ? &costume_data : nullptr, &owned, &missions};
            for (bool menu = !options.select; open;) {
                if (menu) loading.Show(renderer);
                if (menu) {
                    int mode = -1;
                    sif::Nameplate plate = MakeNameplate(missions, accolades, badges, save);
                    // R2 / L2 on the menu (M13), the day's login bonus (M14)
                    const auto member_before = std::tuple(save.mode, save.navigator, save.login_date, save.birthdays);
                    const int entry = sif::RunTopMenu(window, renderer, vfs, lang, options.frames, &mode, &audio, &plate,
                                                      &save, has_events ? &event_data : nullptr);
                    if (std::tuple(save.mode, save.navigator, save.login_date, save.birthdays) != member_before && can_save &&
                        !sif::save::Write(save_path, save, &save_error)) {
                        sif::LogWarn("save: {}", save_error);
                    }
                    if (options.live_options.member <= 0) skills.member = sif::save::Current(save);
                    if (entry < 0) break;
                    home_mode = entry == 0;
                    view_mode = entry == 2;
                    if (mode == 0 && entry == 3) {  // Live Show Medley (M16 R4, R5): Set List Selection, the medley, its end
                        sif::medley::Data medley_data;
                        if (!sif::medley::Load(vfs, medley_data, &why)) {
                            sif::LogWarn("medley: {}", why);
                            continue;
                        }
                        for (int next = 1; open && next == 1;) {  // 1: back to the Set List screen
                            loading.Show(renderer);
                            const sif::MedleyPick pick = sif::RunMedleySelect(
                                window, renderer, vfs, options.live_options.lang, medley_data, save, medley_place, &owned,
                                has_costumes ? &costume_data : nullptr, options.frames);
                            // your lists, their names and the group (L2) are kept
                            if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                            open = !pick.closed;
                            if (!open || !pick.start) break;
                            for (next = 0; open && next == 0;) {  // 0: Watch the Same Medley List
                                sif::MedleyOutcome out = sif::RunMedley(window, renderer, audio, vfs, settings,
                                                                        options.live_options, pick.run, options.frames);
                                if (out.finished && !out.closed) {  // missions 14, 15, 37 and 47 (M16 R3 counters)
                                    sif::save::CountMedley(save, pick.group, int(out.seconds), out.watched);
                                    sif::LogInfo("missions: medley {} s, group {} total {} s", int(out.seconds), pick.group,
                                                 sif::save::Count(save, sif::save::GroupKey(pick.group, "medley_seconds")));
                                    if (can_save && !sif::save::Write(save_path, save, &save_error)) {
                                        sif::LogWarn("save: {}", save_error);
                                    }
                                }
                                loading.Show(renderer);
                                open = !out.closed && sif::RunMedleyEnd(window, renderer, vfs, options.live_options.lang,
                                                                        pick.run, options.frames, out);
                                next = out.choice;
                            }
                        }
                        continue;
                    }
                    if (mode == 4 && entry == 15) {  // Collection: Profile Card: its menu (M12 P6a), then the screens
                        sif::Messages card_messages;
                        if (!card_messages.Load(vfs, language, &why)) sif::LogWarn("profile: messages: {}", why);
                        for (int choice = 1; open && choice > 0;) {
                            loading.Show(renderer);
                            sif::ProfileMenuSetup menu{&save, options.data_dir, &plate,
                                                       {&plate, &songs, &dresses, has_cards ? &card_data : nullptr,
                                                        &card_messages, language, &save,
                                                        has_friendship ? &friendship_data : nullptr},
                                                       skills.member};
                            choice = sif::RunProfileMenu(window, renderer, vfs, lang, menu, options.frames);
                            open = choice >= 0;
                            if (choice == 1) {  // Change Accolade or Name Plate (M7)
                                loading.Show(renderer);
                                const size_t shown_accolade = plate.accolade, shown_badge = plate.badge,
                                             shown_plate = plate.name_plate;
                                open = sif::RunProfile(window, renderer, vfs, lang, plate, options.frames);
                                // only a real choice is saved
                                if (plate.accolade != shown_accolade || plate.badge != shown_badge ||
                                    plate.name_plate != shown_plate) {
                                    save.accolade = plate.accolades[plate.accolade].id, save.badge = plate.badges[plate.badge].id;
                                    save.name_plate = plate.name_plates[plate.name_plate].id;
                                    if (can_save && !sif::save::Write(save_path, save, &save_error)) {
                                        sif::LogWarn("save: {}", save_error);
                                    }
                                }
                            } else if (choice == 2) {  // Create a New Profile Card (M12 P6b)
                                // shortcut: the game asks "Create a new Profile Card?" first when one exists
                                loading.Show(renderer);
                                sif::profile::Data frames;
                                if (!sif::profile::Load(vfs, frames, &why)) sif::LogWarn("profile: {}", why);
                                const sif::ProfileEditSetup edit{&save, options.data_dir, &plate, menu.texts,
                                                                 frames.frames.empty() ? nullptr : &frames,
                                                                 skills.member, lang};
                                open = sif::RunProfileEdit(window, renderer, vfs, edit, options.frames);
                                if (can_save && !sif::save::Write(save_path, save, &save_error)) {
                                    sif::LogWarn("save: {}", save_error);
                                }
                            }
                        }
                        continue;
                    }
                    if (mode == 4 && entry == 18) {  // Collection: Photo Gallery (M12 P5)
                        loading.Show(renderer);
                        open = sif::RunPhotoGallery(window, renderer, vfs, lang, options.data_dir, save, options.frames);
                        if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                        continue;
                    }
                    if (mode == 4 && entry == 16 && has_cards) {  // Collection: Member Card (M12 P5)
                        loading.Show(renderer);
                        sif::MemberCardSetup cards{&card_data, has_costumes ? &costume_data : nullptr, &save, &owned, {},
                                                   skills.member};
                        for (const auto& d : dresses) cards.names.push_back({d.id, d.name});
                        open = sif::RunMemberCards(window, renderer, vfs, lang, cards, options.frames);
                        continue;
                    }
                    if (mode == 4 && entry == 17 && has_cards) {  // Collection: Skill Card (M11 S4)
                        loading.Show(renderer);
                        open = sif::RunSkillCollection(window, renderer, vfs, lang, skills, options.frames);
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
                        options.live_options.note_speed = settings.play.note_speed;
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
                                                {&missions, &accolades, &songs, &save, &owned, &mission_context,
                                                 &profile_data},
                                                options.frames);
                        if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                        continue;
                    }
                    if (mode == 5 && entry == 21 && has_events) {  // Event: Ongoing Events, the birthday events (M14 F6)
                        loading.Show(renderer);
                        open = sif::RunBirthdayEvents(window, renderer, vfs, lang,
                                                      {&event_data, &save, has_cards ? &card_data : nullptr,
                                                       has_costumes ? &costume_data : nullptr},
                                                      options.frames);
                        if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                        continue;
                    }
                }
                loading.Show(renderer);
                const sif::SongChoice choice = sif::RunSongSelect(window, renderer, vfs, lang, options.frames, last, &save, &owned,
                                                                  has_costumes ? &costume_data : nullptr, home_mode,
                                                                  has_cards && !view_mode ? &skills : nullptr, view_mode);
                if (has_cards && can_save && !sif::save::Write(save_path, save, &save_error)) {  // the skill slots (M11)
                    sif::LogWarn("save: {}", save_error);
                }
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
                    std::snprintf(folder, sizeof(folder), save.special.contains(member) ? "d%03d_v1" : "d%03d", dress);
                    options.live_options.wear[member] = folder;
                }
                options.live_options.skills = view_mode ? std::vector<sif::live::Skill>{} : sif::LiveSkills(skills);
                options.live_options.view = view_mode;  // a viewed live is not judged: no records, drops or result
                if (!view_mode) {  // the intro before a played live: the player's panel and the two NPC support cards
                    sif::LiveIntro intro;
                    for (const auto& s : songs) {
                        if (s.id == choice.song) intro.song = s.title;
                    }
                    const int member = skills.member;
                    std::string dress = options.live_options.costume;
                    if (const auto it = options.live_options.wear.find(member); it != options.live_options.wear.end()) {
                        dress = it->second;
                    }
                    const int costume = dress.size() == 4 && dress[0] == 'd' ? std::atoi(dress.c_str() + 1) : 0;
                    intro.member_card = (member * 1000 + costume) * 10;
                    if (has_cards) {
                        const auto slots = sif::card::Slots(card_data, save, member);
                        for (int k = 0; k < 3; ++k) {
                            intro.skills[size_t(k)] = slots[size_t(k)];
                            intro.levels[size_t(k)] = slots[size_t(k)] ? sif::card::Level(card_data, save, slots[size_t(k)]) : 0;
                        }
                    }
                    const sif::Nameplate intro_plate = MakeNameplate(missions, accolades, badges, save);
                    intro.plate = &intro_plate;
                    if (!profile_data.presets.empty()) {
                        const int group = choice.song >= 0 && [&] {
                            for (const auto& s : songs) {
                                if (s.id == choice.song) return s.group != 0;
                            }
                            return false;
                        }() ? 1 : 0;
                        const auto picks = sif::profile::PickSupport(profile_data, group, support_random);
                        for (int k = 0; k < 2; ++k) {
                            if (picks[size_t(k)] >= 0) intro.support[size_t(k)] = &profile_data.presets[size_t(picks[size_t(k)])];
                        }
                    }
                    intro.messages = &messages, intro.language = language, intro.lang = lang;
                    open = sif::RunLiveIntro(window, renderer, vfs, intro, options.frames);
                    if (!open) break;
                    loading.Show(renderer);
                }
                code = sif::RunLive(window, renderer, audio, vfs, settings, options.live_options, &result);
                if (view_mode && !result.photos.empty() && !result.closed) {
                    // View Live Show's photos (M12 P4c/P4d): End the Live Show (favourites), then they are saved.
                    // shortcut: the game's "What will you do next?" box is not built (not for Play Live Show either);
                    // the song select comes back, as after a played live
                    loading.Show(renderer);
                    std::vector<bool> favourites;
                    open = sif::RunLiveViewEnd(window, renderer, vfs, options.live_options.lang, result,
                                               options.live_options.stage, PhotoMemberCard(result, skills.member,
                                                                                           options.live_options),
                                               options.frames, favourites);
                    loading.Show(renderer);
                    CountPhotos(result, skills.member, options.live_options, save);
                    SavePhotos(options.data_dir, result, skills.member, options.live_options, favourites, save);
                    if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                    if (!open) break;
                }
                if (view_mode && code == 0 && !result.closed && result.finished) {  // mission counters (M15 Q3)
                    sif::save::Add(save, "views");
                    sif::save::Add(save, "view." + std::to_string(result.song));
                    if (result.costume.size() == 4) sif::save::Mark(save, "view_costumes", std::atoi(result.costume.c_str() + 1));
                    for (const auto& [m, seconds] : result.camera_seconds) {
                        sif::save::Add(save, "camera." + std::to_string(m), int(seconds));
                    }
                    if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                    sif::LogInfo("missions: View Live Show {} times", sif::save::Count(save, "views"));
                }
                sif::save::NewBests news;
                if (code == 0 && !result.closed && result.finished) {
                    loading.Show(renderer);
                    const bool full_combo = result.counts[2] + result.counts[3] + result.counts[4] == 0;
                    const sif::save::SaveData before = save;
                    const int costume = result.costume.size() == 4 && result.costume[0] == 'd'
                                            ? std::atoi(result.costume.c_str() + 1) : -1;
                    news = sif::save::Record(save, {result.song, result.variant, int(result.score), result.max_combo, result.rank,
                                                    result.clear_rate, full_combo, result.counts[0], result.counts[1], costume});
                    if (!view_mode) {  // the group missions count per group: the played member's (M15 Q2)
                        const int g = sif::member::Mode(skills.member);
                        sif::save::Add(save, sif::save::GroupKey(g, "plays"));
                        sif::save::Add(save, sif::save::GroupKey(g, "perfects"), result.counts[0]);
                        sif::save::Add(save, sif::save::GroupKey(g, "greats"), result.counts[1]);
                        if (full_combo) sif::save::Add(save, sif::save::GroupKey(g, "full_combos"));
                        sif::save::Best(save, sif::save::GroupKey(g, "best_score"), int(result.score));
                        sif::save::Best(save, sif::save::GroupKey(g, "best_rank"), result.rank + 1);
                        if (const auto w = save.wear.find(skills.member); w != save.wear.end() && save.special.contains(skills.member)) {
                            sif::save::Mark(save, "special", w->second);  // mission kind 48
                        }
                    }
                    if (has_cards) {  // skill cards (M11 S6): one draw, a second at rank S with Getting Skills Charge
                        for (const int id : sif::card::RollSkillDrops(card_data, save, skills.member, result.rank, drop_random)) {
                            const int before = sif::card::Level(card_data, save, id);
                            const int level = sif::card::AddSkill(card_data, save, id);
                            const sif::card::SkillCard* c = sif::card::FindSkill(card_data, id);
                            sif::LogInfo("skill card: {} {} (member {}), level {} -> {}", id, c ? c->name[size_t(language)] : "",
                                         c ? c->member : 0, before, level);
                            result.skills.push_back({id, before, level});
                        }
                    }
                    if (has_friendship && !view_mode) {  // Friendship (M14 F4; eboot FUN_000a68d0): the played member only
                        sif::friendship::GainSkills gs;
                        if (has_cards) {  // kinds 18-21
                            gs.more = sif::card::SkillSum(card_data, save, skills.member, 18);
                            gs.more_plus = sif::card::SkillSum(card_data, save, skills.member, 19);
                            gs.expanded = sif::card::SkillSum(card_data, save, skills.member, 20);
                            gs.perfect = sif::card::SkillSum(card_data, save, skills.member, 21);
                        }
                        const int difficulty = result.variant >= 10 ? result.variant - 5 : result.variant;
                        sif::save::Bond& bond = save.bonds[skills.member];
                        const int last = sif::friendship::PointsFor(friendship_data, 200);  // level 200: 2,200,000
                        const int gain = std::min(sif::friendship::Gain(friendship_data, difficulty, result.rank, gs,
                                                                        result.counts[0]),
                                                  std::max(0, last - bond.points));
                        result.friendship = {skills.member, bond.points, gain};
                        sif::LogInfo("friendship: member {} +{} ({} -> {}), level {} -> {}", skills.member, gain, bond.points,
                                     bond.points + gain, sif::friendship::LevelOf(friendship_data, bond.points),
                                     sif::friendship::LevelOf(friendship_data, bond.points + gain));
                        bond.points += gain;
                        ++bond.lives;
                    }
                    if (has_costumes) {  // the song's materials (costume::RollDrops, eboot FUN_000679a0; M11 S6b)
                        sif::costume::DropInput drop{result.song, result.rank,
                                                     result.variant >= 10 ? result.variant - 5 : result.variant,
                                                     costume > 0 ? costume : 0, {}};
                        for (int kind = 27; kind <= 35 && has_cards; ++kind) {  // the material skills
                            if (const int v = sif::card::SkillSum(card_data, save, skills.member, kind)) drop.skills[kind] = v;
                        }
                        // a Wai-Wai! Booster used in Prepare (M14 F5; eboot 0x7DA12): spent now, +10 units (and kind 22)
                        drop.booster = choice.booster && save.boosters > 0 && !view_mode;
                        if (drop.booster) {
                            --save.boosters, ++save.boosters_used;
                            if (has_cards) {
                                if (const int v = sif::card::SkillSum(card_data, save, skills.member, 22)) drop.skills[22] = v;
                            }
                            sif::LogInfo("boosters: one used, {} left", save.boosters);
                        }
                        std::string got;
                        for (const auto& [item, amount] : sif::costume::RollDrops(costume_data, drop, drop_random, save)) {
                            result.materials.push_back({item, amount});
                            const sif::costume::Item* i = sif::costume::FindItem(costume_data, item);
                            sif::save::CountCollected(save, i ? i->type : 0, item, amount);  // mission kinds 8-11
                            got += (got.empty() ? "" : ", ") + (i ? i->name[size_t(language)] : std::to_string(item)) + " x" +
                                   std::to_string(amount);
                        }
                        sif::LogInfo("materials: {}", got.empty() ? "none" : got);
                    }
                    // the played member's birthday event (M14 F6; eboot FUN_0000f850): points while it runs
                    if (has_events && !view_mode) {
                        if (const sif::event::Birthday* b = sif::event::Find(event_data, skills.member)) {
                            if (const int year = sif::event::EventYear(*b, sif::event::Today())) {
                                sif::save::BirthdayState& st = save.birthdays[skills.member];
                                if (st.year != year) st = {year, 0, false};  // a new year's event starts from 0
                                const int add = sif::event::LivePoints(event_data, result.variant, result.rank);
                                st.points = std::min(st.points + add, 9999);
                                sif::LogInfo("birthday: member {} +{} points ({} of {})", skills.member, add, st.points, b->goal);
                            }
                        }
                    }
                    // Drop Support (kind 23): its % chance of one Wai-Wai! Booster (M11 S6: pool 1; eboot 0x7D637, cap 9)
                    if (has_cards && !view_mode) {
                        if (const int v = sif::card::SkillSum(card_data, save, skills.member, 23);
                            v > 0 && std::uniform_real_distribution<float>(0.0f, 100.0f)(drop_random) <= float(v) &&
                            save.boosters < sif::event::kMaxBoosters) {
                            ++save.boosters;
                            sif::LogInfo("boosters: Drop Support gave one, {} held", save.boosters);
                        }
                    }
                    if (can_save && !sif::save::Write(save_path, save, &save_error)) sif::LogWarn("save: {}", save_error);
                    if (const auto it = save.records.find({result.song, result.variant}); it != save.records.end()) {
                        const auto& r = it->second;  // what the save now holds (checked by routes)
                        sif::LogInfo("save: {:04d}/{:02d} played {} times, best score {}, rank {}{}", result.song,
                                     result.variant, r.plays, r.best_score, r.best_rank, news.score ? ", new best score" : "");
                    }
                    LogNewMissions(missions, accolades, songs, dresses, language, before, save, &mission_context);
                }
                const sif::ResultItems result_items{has_cards ? &card_data : nullptr, has_costumes ? &costume_data : nullptr, &save,
                                                    has_friendship ? &friendship_data : nullptr};
                open = code == 0 && !result.closed &&
                       (!result.finished || sif::RunResult(window, renderer, vfs, options.live_options.lang, result,
                                                           options.frames, news, &result_items));
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
