#include "app/song_select.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/live/song.h"

namespace sif {
namespace {

using live::LoadSongList;
using live::SongInfo;

constexpr const char* kRoot = "uil0001_StepLayout00";
constexpr int kPlates = 11;      // uil0013_MusicList00: SelectPlate00_00 (centre), then 01/02, 03/04 ... left/right
constexpr int kDifficulties = 5; // arcade: ac_easy .. ac_challenge (variants 00-04, difficulty index 0-4)
// The parts of the step layout that belong to other steps or popups (song step shows the song list only)
constexpr const char* kOtherSteps[] = {"uil0020_StageSelect00", "uil0025_CosSelect00", "uil0070_LiveViewBanner00",
                                       "uil0071_LiveViewInfo00", "uil0072_LiveViewCaution00",
                                       "uip0019_MusicPreviewLock00", "uil0015_DifficultyList00_OR",
                                       // home-version difficulty strip and save results in the song info panel
                                       "uil0010_MusicSelect00/uil0011_MusicDiff_OR00",
                                       "uil0010_MusicSelect00/uip0031_PlayResult00",
                                       // the list's second button/arrow set and the other steps' counters
                                       "uil0013_MusicList00/common_uip0002_InfoButton00_01",
                                       "uil0013_MusicList00/common_uip0006_ArrowLeft00_01",
                                       "uil0013_MusicList00/common_uip0007_ArrowRight00_01",
                                       "uil0013_MusicList00/uip0042_StageNumber00",
                                       "uil0013_MusicList00/uip0049_CosNumber00"};
// Song plate nodes that need save data (results, drops, locks) or another mode
constexpr const char* kPlateExtras[] = {"lock_ribbon00", "lock_icon00", "drop_item_text00", "drop_item_list00",
                                        "difficulty_original", "difficulty_ac", "cd_jacket_live00",
                                        "cd_jacket_live00_back", "cd_jacket_live00_line", "select_bg_eff00"};

std::string Plate(int k) {
    char s[160];
    std::snprintf(s, sizeof(s), "%s/uil0013_MusicList00/uil0030_SelectPlate00_%02d", kRoot, k);
    return s;
}
std::string DifficultyPlate(int k) {
    char s[160];
    std::snprintf(s, sizeof(s), "%s/uil0016_DifficultyList00_AC/uil0031_SelectPlate01_%02d/uip0030_DifficultySelect00",
                  kRoot, k);
    return s;
}
int PlateOffset(int k) { return k == 0 ? 0 : (k % 2 ? -(k + 1) / 2 : k / 2); }  // 1 -> -1, 2 -> +1, 3 -> -2 ...

// shortcut: the step logic is the port's own (the eboot's setup screen, 0x163000-0x179000, is surveyed but not traced);
// difficulty labels on the plates stay the part's default, the stars show each chart's level
void Apply(UiScreen& ui, const std::vector<SongInfo>& songs, int current, bool difficulty_step, int difficulty) {
    for (const char* other : kOtherSteps) ui.HideInstance(std::string(kRoot) + "/" + other, true);
    ui.HideInstance(std::string(kRoot) + "/uil0013_MusicList00", difficulty_step);
    ui.HideInstance(std::string(kRoot) + "/uil0016_DifficultyList00_AC", !difficulty_step);
    const int n = int(songs.size());
    for (int k = 0; k < kPlates; ++k) {
        const std::string plate = Plate(k);
        for (const char* p : {"/uip0040_StageSelect00", "/uip0045_CosSelect00", "/common_uip0017_New00_00",
                              "/common_uip0017_New00_01"}) {
            ui.HideInstance(plate + p, true);
        }
        const std::string music = plate + "/uip0013_MusicSelect00";
        const int index = n ? ((current + PlateOffset(k)) % n + n) % n : 0;
        ui.HideInstance(music, n == 0);
        if (n) {
            char cover[96];
            std::snprintf(cover, sizeof(cover), "ui/ui_music_thumbnail.pac/texture.pac/cd_cover_%04d", songs[size_t(index)].id);
            ui.SetMaterialTexture(music, "cd_cover00", cover);
            ui.SetInstanceText(music, 0, songs[size_t(index)].title);  // plate text box 0 (ui_text part 13)
        }
        for (const char* node : kPlateExtras) ui.HideNode(music, node, true);
        ui.HideNode(music, "select_bg00_on", k != 0);  // the centre plate is the focused one
        ui.HideNode(music, "select_bg00_off", k == 0);
    }
    const SongInfo* song = n ? &songs[size_t(current)] : nullptr;
    if (song) {  // the preview (part 10: box 0 title, box 3 credits) and the selection panel (part 4: box 0 song)
        const std::string preview = std::string(kRoot) + "/uip0010_MusicPreview00";
        char cover[96];
        std::snprintf(cover, sizeof(cover), "ui/ui_music_thumbnail.pac/texture.pac/cd_cover_%04d", song->id);
        ui.SetMaterialTexture(preview, "cd_cover00", cover);
        ui.HideNode(preview, "drop_materual00", true);
        ui.HideNode(preview, "drop_materual_text00", true);
        ui.SetInstanceText(preview, 0, song->title);
        ui.SetInstanceText(preview, 3, song->credits);
        const std::string panel = std::string(kRoot) + "/uip0004_SelectList00";
        ui.SetInstanceText(panel, 0, song->title);
        ui.SetInstanceText(panel, 1, "");
        ui.SetInstanceText(panel, 2, "");
    }
    for (int d = 0; d < kDifficulties; ++d) {
        const std::string plate = DifficultyPlate(d);
        const int stars = song ? song->stars[size_t(d)] : 0;
        for (int s = 0; s < 10; ++s) {
            char node[24];
            std::snprintf(node, sizeof(node), "difficultyStar%02d", s);
            ui.HideNode(plate, node, s >= stars);
        }
        for (const char* node : {"clearrank00", "combostatus00", "select_playstatus_tex00", "select_playstatus_tex01",
                                 "setup_difficulty_lock00", "select_bg_eff00"}) {
            ui.HideNode(plate, node, true);
        }
        ui.HideNode(plate, "select_bg00_on", d != difficulty);
        ui.HideNode(plate, "select_bg00_off", d == difficulty);
    }
}

}

SongChoice RunSongSelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
                         const SongChoice& previous) {
    std::vector<SongInfo> songs;
    std::string error;
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    if (!LoadSongList(vfs, language, songs, &error) || songs.empty()) {
        LogError("song select: {}", songs.empty() ? "no songs in the dump" : error);
        return {};
    }
    UiText text;
    if (!text.Load(vfs, lang, &error)) LogWarn("song select: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/20_setup", lang, &error, &text)) {
        LogError("song select: {}", error);
        return {};
    }
    ui.HideNode("new00", true);
    ui.HideNode("complete00", true);
    LogInfo("song select: {} songs", songs.size());
    int current = 0, difficulty = kDifficulties - 1;
    for (size_t i = 0; i < songs.size(); ++i) {
        if (songs[i].id == previous.song) current = int(i), difficulty = previous.variant;
    }
    bool difficulty_step = false;
    SongChoice choice;
    bool running = true;
    ScreenFade fade;
    Apply(ui, songs, current, difficulty_step, difficulty);
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; running && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) running = false, choice.closed = true;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int n = int(songs.size());
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT:
                    if (difficulty_step) difficulty = std::max(0, difficulty - 1);
                    else current = (current + n - 1) % n;
                    break;
                case SDL_SCANCODE_RIGHT:
                    if (difficulty_step) difficulty = std::min(kDifficulties - 1, difficulty + 1);
                    else current = (current + 1) % n;
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    if (difficulty_step) {
                        choice = {songs[size_t(current)].id, difficulty};
                        fade.Leave(ui);
                    } else {
                        difficulty_step = true;
                    }
                    break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    if (difficulty_step) difficulty_step = false;
                    else fade.Leave(ui);
                    break;
                default: break;
            }
            Apply(ui, songs, current, difficulty_step, difficulty);
            const SongInfo& s = songs[size_t(current)];
            char title[256];
            std::snprintf(title, sizeof(title), "SIF port - %04d %s%s", s.id, s.title.c_str(),
                          difficulty_step ? (" - difficulty " + std::to_string(difficulty)).c_str() : "");
            SDL_SetWindowTitle(window, title);
        }
        const uint64_t now = SDL_GetTicksNS();
        ui.Update(double(now - last) / 1e9);
        fade.Update(double(now - last) / 1e9);
        last = now;
        if (!renderer.BeginFrame()) {
            SDL_Delay(5);
            continue;
        }
        ui.DrawLayout(renderer, kRoot, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    if (choice.song >= 0) LogInfo("song select: song {:04d}, variant {:02d}", choice.song, choice.variant);
    return choice;
}

}
