#include "app/song_select.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
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
using live::LoadStageList;
using live::SongInfo;
using live::StageInfo;

enum Step { kSongStep, kDifficultyStep, kStageStep };

constexpr const char* kRoot = "uil0001_StepLayout00";
constexpr int kPlates = 11;      // uil0013_MusicList00: SelectPlate00_00 (centre), then 01/02, 03/04 ... left/right
constexpr int kDifficulties = 5; // arcade: ac_easy .. ac_challenge (variants 00-04, difficulty index 0-4)
// The parts of the step layout that belong to other steps or popups (song step shows the song list only)
constexpr const char* kOtherSteps[] = {"uil0025_CosSelect00", "uil0070_LiveViewBanner00",
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

// Difficulty labels (setup_difficulty_plate00 / setup_difficulty_level00) and level badges (00_common
// setup_difficulty_star01) are 4 x 4 atlases of 0.25 x 0.25 cells. Labels: 0-3 home BEGINNER, STANDARD, HYPER,
// ADVENTURE (variants 10-13), 4-8 EASY(AC) .. CHALLENGE(AC) (variants 00-04); badges: cell n-1 shows level n (1-15).
std::array<float, 2> Cell(int k) { return {0.25f * float(k % 4), 0.25f * float(k / 4)}; }

// setup_unit_logo00 (5 x 2 cells of 0.2 x 0.5): mu's, Aqours, Saint Snow, Printemps, lily white / BiBi, CYaRon!,
// AZALEA, Guilty Kiss. The song's unit picks the logo; solos and mixed line-ups show their group's.
std::array<float, 2> UnitLogo(int group, int unit) {
    static constexpr int kLogoOfUnit[11] = {0, 3, 5, 4, 1, 0, 1, 6, 7, 8, 2};
    const int k = unit >= 0 && unit < 11 ? kLogoOfUnit[unit] : std::clamp(group, 0, 2);
    return {0.2f * float(k % 5), 0.5f * float(k / 5)};
}

std::string MemberTexture(int member) {
    char s[80];
    std::snprintf(s, sizeof(s), "ui/ui_sd_character.pac/texture.pac/sd_character_menu%02d", member);
    return s;
}

// The song's members as chibis (ui_sd_character.pac sd_character_menuNN, NN = member id). The music select row
// (uip0018_SDMember00) has 11 slots, filled from the left; the selection panel (uip0004_SelectList00) has groups of
// 11 (sd_member_all), 2 (sd_member_duo) and 8 (sd_member_even) slots. shortcut: the panel's group choice (2 -> duo,
// even -> even, odd -> all) and centring are guesses from the slot counts
void ApplyMembers(UiScreen& ui, const std::string& row, const std::string& panel, const std::vector<int>& members) {
    const int n = int(members.size());
    for (int k = 0; k < 11; ++k) {
        char node[16], material[24];
        std::snprintf(node, sizeof(node), k < 10 ? "sd_chara0%d" : "sd_chara0%02d", k);
        std::snprintf(material, sizeof(material), k == 9 ? "sd_characte09" : "sd_characte00_%02d", k);
        ui.HideNode(row, node, k >= n);
        if (k < n) ui.SetMaterialTexture(row, material, MemberTexture(members[size_t(k)]));
    }
    const int group = n == 2 ? 1 : n % 2 == 0 ? 2 : 0;
    static constexpr const char* kGroups[3] = {"sd_member_all", "sd_member_duo", "sd_member_even"};
    static constexpr int kSlots[3] = {11, 2, 8};
    for (int g = 0; g < 3; ++g) ui.HideNode(panel, kGroups[g], g != group);
    const int first = (kSlots[group] - std::min(n, kSlots[group])) / 2;
    for (int k = 0; k < kSlots[group]; ++k) {
        char node[24], material[24];
        std::snprintf(node, sizeof(node), "sd_chara%02d_%02d", group, k);
        std::snprintf(material, sizeof(material), "sd_characte%02d", k);
        const int m = k - first;
        ui.HideNode(panel, node, m < 0 || m >= n);
        if (m >= 0 && m < n) ui.SetMaterialTexture(panel, material, MemberTexture(members[size_t(m)]));
    }
}

// The selection panel's difficulty: small label (setup_difficulty_level_s00, same order as the labels), and ten stars
// from setup_difficulty_star00 (cells of 0.5: grey (0, 0), yellow (0.5, 0), pink (0, 0.5)); like the level badges,
// stars past 10 turn the first ones pink
void ApplyPanelDifficulty(UiScreen& ui, const std::string& panel, int label, int level) {
    const auto l = Cell(label);
    ui.SetMaterialUv(panel, "difficulty_level00", l[0], l[1]);
    for (int s = 0; s < 10; ++s) {
        char material[24];
        std::snprintf(material, sizeof(material), "difficulty_star%02d", s);
        const bool pink = s < level - 10, yellow = s < level;
        ui.SetMaterialUv(panel, material, pink ? 0.0f : yellow ? 0.5f : 0.0f, pink ? 0.5f : 0.0f);
    }
}

// shortcut: the step logic is the port's own (the eboot's setup screen, 0x163000-0x179000, is surveyed but not traced);
// labels and levels come from the charts, records (rank, clear status, combo) need a save and are hidden
std::string StageThumbnail(const StageInfo& s) {
    char t[80];
    std::snprintf(t, sizeof(t), "ui/ui_stage_thumbnail.pac/texture.pac/stage_%03d", s.id);
    return t;
}

// The stage step reuses the song list's plates (each also places uip0040_StageSelect00: thumbnail, name) plus the
// stage preview (uil0020_StageSelect00: uip0041 picture and name; uip0043 records, hidden without a save)
void ApplyStages(UiScreen& ui, const std::vector<StageInfo>& stages, int stage, bool show) {
    ui.HideInstance(std::string(kRoot) + "/uil0020_StageSelect00", !show);
    const int n = int(stages.size());
    for (int k = 0; k < kPlates; ++k) {
        const std::string plate = Plate(k) + "/uip0040_StageSelect00";
        ui.HideInstance(plate, !show || n == 0);
        if (!show || n == 0) continue;
        const StageInfo& s = stages[size_t(((stage + PlateOffset(k)) % n + n) % n)];
        ui.SetMaterialTexture(plate, "stage_thumbnail00_M", StageThumbnail(s));
        ui.SetInstanceText(plate, 0, s.name);
        for (const char* node : {"lock_ribbon00", "lock_icon00", "select_bg_eff00"}) ui.HideNode(plate, node, true);
        ui.HideNode(plate, "select_bg00_on", k != 0);
        ui.HideNode(plate, "select_bg00_off", k == 0);
        ui.HideNode(plate, "select_bg00_line_on", k != 0);
        ui.HideNode(plate, "select_bg00_line_off", k == 0);
    }
    if (!show || n == 0) return;
    const std::string preview = std::string(kRoot) + "/uil0020_StageSelect00/uip0041_StagePreview00";
    ui.SetMaterialTexture(preview, "stage_preview00_M", StageThumbnail(stages[size_t(stage)]));
    ui.SetInstanceText(preview, 0, stages[size_t(stage)].name);
    ui.HideNode(preview, "stage_preview_line00", true);
    ui.HideInstance(std::string(kRoot) + "/uil0020_StageSelect00/uip0043_StagePlay00", true);
}

void Apply(UiScreen& ui, const std::vector<SongInfo>& songs, int current, Step step, int difficulty,
           const std::vector<StageInfo>& stages, int stage) {
    for (const char* other : kOtherSteps) ui.HideInstance(std::string(kRoot) + "/" + other, true);
    ui.HideInstance(std::string(kRoot) + "/uil0013_MusicList00", step == kDifficultyStep);
    ui.HideInstance(std::string(kRoot) + "/uil0016_DifficultyList00_AC", step != kDifficultyStep);
    ApplyStages(ui, stages, stage, step == kStageStep);
    const int n = int(songs.size());
    for (int k = 0; k < kPlates; ++k) {
        const std::string plate = Plate(k);
        for (const char* p : {"/uip0045_CosSelect00", "/common_uip0017_New00_00", "/common_uip0017_New00_01"}) {
            ui.HideInstance(plate + p, true);
        }
        const std::string music = plate + "/uip0013_MusicSelect00";
        const int index = n ? ((current + PlateOffset(k)) % n + n) % n : 0;
        ui.HideInstance(music, n == 0 || step == kStageStep);
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
        // box 1: the stage (Hypothesis: boxes 1 and 2 are the stage and costume names)
        ui.SetInstanceText(panel, 1, stages.empty() ? "" : stages[size_t(stage)].name);
        ui.SetInstanceText(panel, 2, "");
        if (!stages.empty()) ui.SetMaterialTexture(panel, "stage_000", StageThumbnail(stages[size_t(stage)]));
        ui.SetMaterialTexture(panel, "cd_cover00", cover);
        const auto group = UnitLogo(song->group, -1), unit = UnitLogo(song->group, song->unit);
        ui.SetMaterialUv(panel, "logo00_M", group[0], group[1]);
        ui.SetMaterialUv(panel, "logo01_M", unit[0], unit[1]);
        ApplyPanelDifficulty(ui, panel, 4 + difficulty, song->stars[size_t(difficulty)]);
        const std::string music = std::string(kRoot) + "/uil0010_MusicSelect00/";
        ApplyMembers(ui, music + "uip0018_SDMember00", panel, song->members);
        // the group signs (uip0012_GroupName00): group_logo00 twice, group_logo02 once; group and unit (Hypothesis)
        ui.SetMaterialUv(music + "uip0012_GroupName00", "group_logo00", group[0], group[1]);
        ui.SetMaterialUv(music + "uip0012_GroupName00", "group_logo02", unit[0], unit[1]);
    }
    // the song step's five arcade difficulty cards: label, level badge; no chart -> no card
    const std::string cards = std::string(kRoot) + "/uil0010_MusicSelect00/uil0012_MusicDiff_AC00/";
    for (int d = 0; d < kDifficulties; ++d) {
        char k[8];
        std::snprintf(k, sizeof(k), "_%02d", d);
        const int level = song ? song->stars[size_t(d)] : 0;
        const std::string card = cards + "uip0011_MusicDifficulty00" + k, badge = cards + "common_uip0010_DifficultyStar00" + k;
        ui.HideInstance(card, level == 0);
        ui.HideInstance(badge, level == 0);
        ui.HideInstance(cards + "uip0020_Clear_status00" + k, true);
        const auto label = Cell(4 + d), cell = Cell(std::clamp(level, 1, 15) - 1);
        ui.SetMaterialUv(card, "difficulty_level00", label[0], label[1]);
        ui.HideNode(card, "lank_result00", true);
        ui.HideNode(card, "lock_icon00", true);
        ui.SetMaterialUv(badge, "difficulty_star00", cell[0], cell[1]);
    }
    ui.HideInstance(std::string(kRoot) + "/uil0010_MusicSelect00/uil0012_MusicDiff_AC00", step == kStageStep);  // under the preview
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
        const auto label = Cell(4 + d);
        ui.SetMaterialUv(plate, "M_difficulty_level00_big", label[0], label[1]);
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
    std::vector<StageInfo> stages;
    if (!LoadStageList(vfs, language, stages, &error)) LogWarn("song select: stages: {}", error);
    LogInfo("song select: {} songs, {} stages", songs.size(), stages.size());
    int current = 0, difficulty = kDifficulties - 1, stage = 0;
    for (size_t i = 0; i < songs.size(); ++i) {
        if (songs[i].id == previous.song) current = int(i), difficulty = previous.variant;
    }
    const std::string last_stage = previous.stage.empty() ? "st017" : previous.stage;  // else the live default
    for (size_t i = 0; i < stages.size(); ++i) {
        char folder[16];
        std::snprintf(folder, sizeof(folder), "st%03d", stages[i].model);
        if (last_stage == folder) stage = int(i);
    }
    Step step = kSongStep;
    SongChoice choice;
    bool running = true;
    ScreenFade fade;
    Apply(ui, songs, current, step, difficulty, stages, stage);
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; running && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) running = false, choice.closed = true;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int n = int(songs.size()), ns = std::max(1, int(stages.size()));
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT:
                    if (step == kDifficultyStep) difficulty = std::max(0, difficulty - 1);
                    else if (step == kStageStep) stage = (stage + ns - 1) % ns;
                    else current = (current + n - 1) % n;
                    break;
                case SDL_SCANCODE_RIGHT:
                    if (step == kDifficultyStep) difficulty = std::min(kDifficulties - 1, difficulty + 1);
                    else if (step == kStageStep) stage = (stage + 1) % ns;
                    else current = (current + 1) % n;
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    if (step == kStageStep || (step == kDifficultyStep && stages.empty())) {
                        choice = {songs[size_t(current)].id, difficulty};
                        if (!stages.empty()) {
                            char folder[16];
                            std::snprintf(folder, sizeof(folder), "st%03d", stages[size_t(stage)].model);
                            choice.stage = folder;
                        }
                        fade.Leave(ui);
                    } else {
                        step = Step(step + 1);
                    }
                    break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    if (step != kSongStep) step = Step(step - 1);
                    else fade.Leave(ui);
                    break;
                default: break;
            }
            Apply(ui, songs, current, step, difficulty, stages, stage);
            const SongInfo& s = songs[size_t(current)];
            char title[256];
            std::snprintf(title, sizeof(title), "SIF port - %04d %s%s", s.id, s.title.c_str(),
                          step == kDifficultyStep ? (" - difficulty " + std::to_string(difficulty)).c_str()
                          : step == kStageStep    ? (" - stage " + stages[size_t(stage)].name).c_str()
                                                  : "");
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
