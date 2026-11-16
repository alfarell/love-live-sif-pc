#include "app/song_select.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/route.h"
#include "app/skill_screen.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"
#include "game/live/song.h"
#include "game/text/messages.h"

namespace sif {
namespace {

using live::CostumesFor;
using live::DressInfo;
using live::LoadDressList;
using live::LoadSongList;
using live::LoadStageList;
using live::SongInfo;
using live::StageInfo;

enum Step { kSongStep, kDifficultyStep, kStageStep, kCostumeStep, kPrepareStep };

constexpr const char* kRoot = "uil0001_StepLayout00";
constexpr int kPlates = 11;      // uil0013_MusicList00: SelectPlate00_00 (centre), then 01/02, 03/04 ... left/right
// The two modes (M10 K3): "Play Live Show (AC)" has the five arcade charts (variants 00-04, difficulty index 0-4) in
// the _AC layouts; "Play Live Show" the four home charts (variants 10-13, index 5-8) in the _OR layouts, which hold
// the same parts with four entries
struct Mode {
    bool home = false;
    int Count() const { return home ? 4 : 5; }
    int Variant(int d) const { return home ? 10 + d : d; }
    int Index(int d) const { return home ? 5 + d : d; }  // into SongInfo::stars (difficulty index)
    int Label(int d) const { return home ? d : 4 + d; }  // label atlas cell: 0-3 home, 4-8 arcade
    const char* Cards() const { return home ? "uil0011_MusicDiff_OR00" : "uil0012_MusicDiff_AC00"; }
    const char* Plates() const { return home ? "uil0015_DifficultyList00_OR" : "uil0016_DifficultyList00_AC"; }
    const char* OtherCards() const { return home ? "uil0012_MusicDiff_AC00" : "uil0011_MusicDiff_OR00"; }
    const char* OtherPlates() const { return home ? "uil0016_DifficultyList00_AC" : "uil0015_DifficultyList00_OR"; }
};
// The parts of the step layout that belong to other steps or popups (song step shows the song list only)
constexpr const char* kOtherSteps[] = {"uil0025_CosSelect00", "uil0070_LiveViewBanner00",
                                       "uil0071_LiveViewInfo00", "uil0072_LiveViewCaution00",
                                       "uip0019_MusicPreviewLock00",
                                       // save results in the song info panel
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
std::string DifficultyPlate(const Mode& mode, int k) {
    char s[160];
    std::snprintf(s, sizeof(s), "%s/%s/uil0031_SelectPlate01_%02d/uip0030_DifficultySelect00", kRoot, mode.Plates(), k);
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

// A chart's record, or null if it was never played to its end
const save::ChartRecord* Record(const save::SaveData* save, int song, int variant) {
    if (!save) return nullptr;
    const auto it = save->records.find({song, variant});
    return it != save->records.end() && it->second.best_rank >= 0 ? &it->second : nullptr;
}

// ADVENTURE (home, the last card) opens with rank A or better on the song's HYPER: the card's own text, "Unlock by
// getting A or higher in HYPER" (shadPS4, M10). shortcut: the arcade CHALLENGE lock ("... in EXTREME(AC)") is not
// applied, so the arcade mode stays as before; add it when the user wants the game's arcade lock too
// shortcut: the game also darkens a locked card and plate; the UI layer has no colour override for parts, so the port
// shows only the padlock and the hint. Add a tint when it is wanted
bool Locked(const Mode& mode, const save::SaveData* save, int song, int d) {
    if (!mode.home || d != 3) return false;
    const save::ChartRecord* hyper = Record(save, song, mode.Variant(2));
    return !hyper || hyper->best_rank < 3;  // 0 D, 1 C, 2 B, 3 A
}

// The played member's R member card for a costume (M11 S7): ui_member_card_thumbnail mbc_<card id>, the
// s_member_card_datas row of that member whose +0x14 is the costume and whose id ends in 0 (the R picture; 1 is the HR
// "special colour" one). "" when the member has no card for it (the uniform d000).
std::string MemberCardPath(const card::Data* cards, int member, int dress) {
    if (!cards) return {};
    for (const card::MemberCard& c : cards->members) {
        if (c.member == member && c.costume == dress && c.id % 10 == 0) {
            return "ui/ui_member_card_thumbnail.pac/texture.pac/mbc_" + std::to_string(c.id);
        }
    }
    return {};
}

// What the costume step shows besides the list: the played member's cards and the costume's description.
struct CostumeArt {
    const card::Data* cards = nullptr;
    int member = 1;
    const costume::Data* data = nullptr;
    int language = 1;
};

// The costume step (Select Costume): the game's uil0025_CosSelect00 over the step layout (shadPS4 S2 s12) with the
// costume's picture (ctg30 CosPreview), name, description (common SelectInfoWin) and the played member's large card
// (uip0047_CharaCard00); the song list's plates' uip0045_CosSelect00 show that member's card for each costume, or the
// costume thumbnail when there is no card (d000). shortcut: the 3D member behind the curtain, special colours (L2),
// the costume effect line and the "N/M Sets" counter are not built
constexpr const char* kCostumeLayout = "uil0025_CosSelect00";
void ApplyCostumes(UiScreen& ui, const std::vector<DressInfo>& costumes, int costume, bool show, const CostumeArt& art) {
    const int n = int(costumes.size());
    for (int k = 0; k < kPlates; ++k) {
        const std::string plate = Plate(k) + "/uip0045_CosSelect00";
        ui.HideInstance(plate, !show || n == 0);
        if (!show || n == 0) continue;
        const DressInfo& d = costumes[size_t(((costume + PlateOffset(k)) % n + n) % n)];
        if (d.id > 0) {
            char thumb[80];
            std::snprintf(thumb, sizeof(thumb), "ui/ui_dress_thumbnail.pac/texture.pac/dress_thumb_%04d", d.id);
            ui.SetMaterialTexture(plate, "cos_icon00_M", thumb);
        }
        ui.SetInstanceText(plate, 0, d.name);
        for (const char* node : {"card_side00", "card_side00_line", "cos_lock_ribbon00", "lock_icon00", "cos_select_bg00_eff00"}) {
            ui.HideNode(plate, node, true);
        }
        const std::string card = MemberCardPath(art.cards, art.member, d.id);
        if (!card.empty()) ui.SetMaterialTexture(plate, "card_center00_M", card);
        for (const char* node : {"card_center00", "card_center00_line"}) ui.HideNode(plate, node, card.empty());
        for (const char* node : {"cos_icon00", "cos_icon00_line"}) ui.HideNode(plate, node, !card.empty());
        ui.HideNode(plate, "cos_select_bg00_on", k != 0);
        ui.HideNode(plate, "cos_select_bg00_off", k == 0);
    }
    // the list's counter: "N/M Sets" (uip0049_CosNumber00) instead of the songs' count and the Detail View button
    // (Hypothesis: N is the focused costume's place in the list; s12 shows 13/29 on School Unity)
    const std::string list = std::string(kRoot) + "/uil0013_MusicList00";
    ui.HideInstance(list + "/uip0014_MusicNumber00", show);
    ui.HideInstance(list + "/common_uip0002_InfoButton00_00", show);
    ui.HideInstance(list + "/uip0049_CosNumber00", !show || n == 0);
    if (!show || n == 0) return;
    ui.SetInstanceNumber(list + "/uip0049_CosNumber00", 0, std::to_string(costume + 1) + "/" + std::to_string(n));
    const std::string root = kCostumeLayout;
    for (const char* p : {"/common_uip0014_OpeButton00", "/ctg30_uip0010_FittingMark00", "/uip0059_CosCurtain00"}) {
        ui.HideInstance(root + p, true);
    }
    const DressInfo& d = costumes[size_t(costume)];
    ui.SetInstanceText(root + "/ctg30_uip0003_CosName00", 0, d.name);
    const std::string preview = root + "/ctg30_uip0007_CosPreview00";
    if (d.id > 0) ui.SetMaterialTexture(preview, "costume_preview00_M", costume::ThumbPath(d.id));
    for (const char* node : {"preview00_card00_R", "preview00_card00_R_line", "preview00_card00_HR", "preview00_card00_HR_line"}) {
        ui.HideNode(preview, node, true);
    }
    // the costume's info picture (effect line, "Costumes for Nine", the members): ui_dress_info_popup cos_pop_info_NNNN_T
    const std::string info = root + "/ctg30_uip0006_CosInfo00";
    char pop[96];
    std::snprintf(pop, sizeof(pop), "ui/ui_dress_info_popup%s.pac/texture.pac/cos_pop_info_%04d_T",
                  art.language == 1 ? "_en" : art.language == 2 ? "_cn" : art.language == 3 ? "_tw" : "", d.id);
    if (!ui.SetMaterialTexture(info, "cos_info_member00_M", pop)) LogWarn("song select: no {}", pop);
    for (const char* node : {"status_label00", "status_label01", "status_label02", "status_label03", "cos_caution00"}) {
        ui.HideNode(info, node, true);
    }
    const costume::Recipe* recipe = art.data ? costume::FindRecipe(*art.data, d.id) : nullptr;
    ui.SetInstanceText(root + "/common_uip0009_SelectInfoWin00", 0, recipe ? recipe->text[size_t(art.language)] : "");
    ui.SetInstanceText(root + "/common_uip0009_SelectInfoWin00", 1, "");
    const std::string card = MemberCardPath(art.cards, art.member, d.id);
    LogInfo("song select: costume {} '{}', member {} card {}", d.id, d.name, art.member,
            card.empty() ? "none" : card.substr(card.rfind('/') + 1));
    ui.HideInstance(root + "/uip0047_CharaCard00", card.empty());
    if (!card.empty()) ui.SetMaterialTexture(root + "/uip0047_CharaCard00", "chara_card00_M", card);
}

// Prepare for the Live Show (uil0027_LiveStart00, drawn over the step layout; the game's screen in shadPS4,
// work/m11/shad/s13 and s18): Profile Card in Use, Skill Card in Use (the played member's three slots), the three
// sub-buttons (Game Settings, Profile Card Selection, Skill Card Selection) and LIVE START. `focus`: 0-2 a sub-button,
// 3 LIVE START. shortcut: Wai-Wai! Boosters, profile cards and the material panel are not built (hidden or greyed);
// Game Settings is shown but opens nothing here (Options -> Game Settings does)
constexpr const char* kPrepare = "uil0027_LiveStart00";
void ApplyPrepare(UiScreen& ui, const SkillSetup* skills, const Messages& messages, int language, int focus) {
    const std::string root = kPrepare;
    const auto msg = [&](int id, const char* fallback) {
        const std::string& m = messages.Get(id);
        return m.empty() ? std::string(fallback) : m;
    };
    ui.SetInstanceText(root + "/uip0063_ProfCard00", 0, msg(606, "Profile Card in Use"));
    for (const char* card : {"prof_card_00", "prof_card_01", "prof_card_02", "prof_card_03"}) {
        ui.HideNode(root + "/uip0063_ProfCard00", card, true);
    }
    ui.SetInstanceText(root + "/uip0063_ProfCard00", 1, ""), ui.SetInstanceText(root + "/uip0063_ProfCard00", 2, "");
    ui.SetInstanceText(root + "/uip0068_UserProcRec00", 0, msg(650, "Receive Everyone's Profile Cards"));
    ui.HideInstance(root + "/uip0065_FavoriteInfo00", true);
    ui.HideInstance(root + "/uip0066_DropMaterial00", true);
    ui.HideInstance(root + "/ctg80_uip0001_SkillList00", !skills || !skills->cards);
    if (skills && skills->cards) ApplySkillRows(ui, root + "/ctg80_uip0001_SkillList00", *skills, messages, language);
    // setup_select_icon00 cells of 0.5: the card (0, 0), the smile (0.5, 0), the gear (0, 0.5)
    static constexpr float kIcon[3][2] = {{0, 0.5f}, {0.5f, 0}, {0, 0}};
    static constexpr int kText[3] = {589, 588, 587};
    static constexpr const char* kFallback[3] = {"Game Settings", "Profile Card Selection", "Skill Card Selection"};
    for (int k = 0; k < 3; ++k) {
        char path[96];
        std::snprintf(path, sizeof(path), "%s/uip0062_SubButton00_%02d", kPrepare, k);
        ui.SetMaterialUv(path, "select_menu_icon00_M", kIcon[k][0], kIcon[k][1]);
        ui.SetInstanceText(path, 0, msg(kText[k], kFallback[k]));
        ui.HideNode(path, "select_menu_on", focus != k);
        ui.HideNode(path, "select_menu_eff00", focus != k);
        ui.HideNode(path, "select_menu_off", focus == k);
    }
    const std::string start = root + "/uip0061_StartButton00";
    ui.HideNode(start, "button_start00_on", focus != 3);
    ui.HideNode(start, "button_start00_eff", focus != 3);
    ui.HideNode(start, "button_start00_off", focus == 3);
}

void Apply(UiScreen& ui, const Mode& mode, const std::vector<SongInfo>& songs, int current, Step step, int difficulty,
           const std::vector<StageInfo>& stages, int stage, const std::vector<DressInfo>& costumes, int costume,
           const save::SaveData* save, const CostumeArt& art) {
    for (const char* other : kOtherSteps) ui.HideInstance(std::string(kRoot) + "/" + other, true);
    ui.HideInstance(std::string(kRoot) + "/uil0013_MusicList00", step == kDifficultyStep || step == kPrepareStep);
    // Select Costume and Prepare show their own panels where the song's preview, members and sort bar were (s12, s13)
    for (const char* p : {"/uil0010_MusicSelect00", "/uip0010_MusicPreview00", "/uil0017_MenuPage00"}) {
        ui.HideInstance(std::string(kRoot) + p, step == kPrepareStep || step == kCostumeStep);
    }
    ui.HideInstance(std::string(kRoot) + "/" + mode.Plates(), step != kDifficultyStep);
    ui.HideInstance(std::string(kRoot) + "/" + mode.OtherPlates(), true);
    ui.HideInstance(std::string(kRoot) + "/uil0010_MusicSelect00/" + mode.OtherCards(), true);
    // the header: the mode (setup_title_mode00_T rows of 1/4: Play Live Show, Play Live Show(AC), View Live Show, Live
    // Show Medley) and the page name (setup_title_page00_T, 2 columns, rows of 1/8: Select Song, Select Difficulty,
    // Stage Selection, Select Costume, Prepare for the Live Show; pink in the top half, green in the bottom half). The
    // game's home select is pink (shadPS4, M10); the AC mode's title is green, so its page names take the green half
    // (Hypothesis: not seen)
    static constexpr float kPageUv[5][2] = {{0, 0}, {0.5f, 0}, {0, 0.125f}, {0.5f, 0.125f}, {0, 0.25f}};
    const std::string top = std::string(kRoot) + "/uip0002_StepTop00";
    ui.SetMaterialUv(top, "title_mode00", 0, mode.home ? 0.0f : 0.25f);
    ui.SetMaterialUv(top, "title_page01", kPageUv[step][0], kPageUv[step][1] + (mode.home ? 0.0f : 0.5f));
    ApplyStages(ui, stages, stage, step == kStageStep);
    ApplyCostumes(ui, costumes, costume, step == kCostumeStep, art);
    const int n = int(songs.size());
    for (int k = 0; k < kPlates; ++k) {
        const std::string plate = Plate(k);
        for (const char* p : {"/common_uip0017_New00_00", "/common_uip0017_New00_01"}) ui.HideInstance(plate + p, true);
        const std::string music = plate + "/uip0013_MusicSelect00";
        const int index = n ? ((current + PlateOffset(k)) % n + n) % n : 0;
        ui.HideInstance(music, n == 0 || step >= kStageStep);
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
    // the group header (00_common parts): µ's or Aqours / Saint Snow, by the current song (the game toggles the group
    // with L2 and lists one group's songs; shortcut: the port lists every group's songs together)
    const bool aqours = song && song->group != 0;
    ui.HideInstance(std::string(kRoot) + "/common_uil0101_ComBGGroup_Member01/common_uip0400_ComTopBG_Ms00", aqours);
    ui.HideInstance(std::string(kRoot) + "/common_uil0101_ComBGGroup_Member01/common_uip0410_ComTopBG_Aqours00", !aqours);
    ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab01", "select_group00", aqours);
    ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab01", "select_group01", !aqours);
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
        // box 1: the stage (Hypothesis; box 2 is on the difficulty row, so 1 may not be the stage either)
        ui.SetInstanceText(panel, 1, stages.empty() ? "" : stages[size_t(stage)].name);
        ui.SetInstanceText(panel, 2, "");  // not the costume name: box 2 sits on the difficulty row
        if (!stages.empty()) ui.SetMaterialTexture(panel, "stage_000", StageThumbnail(stages[size_t(stage)]));
        ui.SetMaterialTexture(panel, "cd_cover00", cover);
        const auto group = UnitLogo(song->group, -1), unit = UnitLogo(song->group, song->unit);
        ui.SetMaterialUv(panel, "logo00_M", group[0], group[1]);
        ui.SetMaterialUv(panel, "logo01_M", unit[0], unit[1]);
        ApplyPanelDifficulty(ui, panel, mode.Label(difficulty), song->stars[size_t(mode.Index(difficulty))]);
        const std::string music = std::string(kRoot) + "/uil0010_MusicSelect00/";
        ApplyMembers(ui, music + "uip0018_SDMember00", panel, song->members);
        // the group signs (uip0012_GroupName00): group_logo00 twice, group_logo02 once; group and unit (Hypothesis)
        ui.SetMaterialUv(music + "uip0012_GroupName00", "group_logo00", group[0], group[1]);
        ui.SetMaterialUv(music + "uip0012_GroupName00", "group_logo02", unit[0], unit[1]);
    }
    // the song step's difficulty cards (five arcade or four home): label, level badge; no chart -> no card
    const std::string cards = std::string(kRoot) + "/uil0010_MusicSelect00/" + mode.Cards() + "/";
    for (int d = 0; d < mode.Count(); ++d) {
        char k[8];
        std::snprintf(k, sizeof(k), "_%02d", d);
        const int level = song ? song->stars[size_t(mode.Index(d))] : 0;
        const std::string card = cards + "uip0011_MusicDifficulty00" + k, badge = cards + "common_uip0010_DifficultyStar00" + k;
        ui.HideInstance(card, level == 0);
        ui.HideInstance(badge, level == 0);
        ui.HideInstance(cards + "uip0020_Clear_status00" + k, true);
        const auto label = Cell(mode.Label(d)), cell = Cell(std::clamp(level, 1, 15) - 1);
        ui.SetMaterialUv(card, "difficulty_level00", label[0], label[1]);
        // the best rank: setup_result_evaluation00 holds D C B A S SS SSS in cells of 0.125
        const save::ChartRecord* record = song ? Record(save, song->id, mode.Variant(d)) : nullptr;
        ui.HideNode(card, "lank_result00", !record);
        if (record) ui.SetMaterialUv(card, "result_evaluation00", 0.125f * float(record->best_rank), 0);
        ui.HideNode(card, "lock_icon00", !song || !Locked(mode, save, song->id, d));
        ui.SetMaterialUv(badge, "difficulty_star00", cell[0], cell[1]);
    }
    ui.HideInstance(std::string(kRoot) + "/uil0010_MusicSelect00/" + mode.Cards(), step >= kStageStep);  // under the preview
    for (int d = 0; d < mode.Count(); ++d) {
        const std::string plate = DifficultyPlate(mode, d);
        const int stars = song ? song->stars[size_t(mode.Index(d))] : 0;
        for (int s = 0; s < 10; ++s) {
            char node[24];
            std::snprintf(node, sizeof(node), "difficultyStar%02d", s);
            ui.HideNode(plate, node, s >= stars);
        }
        // a locked chart: the padlock and the unlock hint (setup_select_playstatus_text00_01 "Unlock by getting A or
        // higher in HYPER"; _02 is the AC one, "... in EXTREME(AC)", not used: the port keeps CHALLENGE open)
        const bool locked = song && Locked(mode, save, song->id, d);
        ui.HideNode(plate, "select_playstatus_tex00", !locked);
        ui.HideNode(plate, "setup_difficulty_lock00", !locked);
        for (const char* node : {"select_playstatus_tex01", "select_bg_eff00"}) ui.HideNode(plate, node, true);
        // played: best rank instead of "Unplayed", and FULL COMBO (setup_result_combo00 row 1 of 4) once achieved
        const save::ChartRecord* record = song ? Record(save, song->id, mode.Variant(d)) : nullptr;
        ui.HideNode(plate, "notplayed00", record != nullptr || locked);
        ui.HideNode(plate, "clearrank00", !record);
        ui.HideNode(plate, "combostatus00", !record || record->full_combos == 0);
        if (record) {
            ui.SetMaterialUv(plate, "M_clearrank00", 0.125f * float(record->best_rank), 0);
            ui.SetMaterialUv(plate, "M_combostatus00", 0, 0.25f);
        }
        const auto label = Cell(mode.Label(d));
        ui.SetMaterialUv(plate, "M_difficulty_level00_big", label[0], label[1]);
        ui.HideNode(plate, "select_bg00_on", d != difficulty);
        ui.HideNode(plate, "select_bg00_off", d == difficulty);
    }
}

}

SongChoice RunSongSelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, long max_frames,
                         const SongChoice& previous, const save::SaveData* save, const dlc::Ownership* owned,
                         const costume::Data* costume_data, bool home, const SkillSetup* skills) {
    route::SetScreen("select");
    const Mode mode{home};
    std::vector<SongInfo> songs;
    std::string error;
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    if (!LoadSongList(vfs, language, songs, &error) || songs.empty()) {
        LogError("song select: {}", songs.empty() ? "no songs in the dump" : error);
        return {};
    }
    if (owned) std::erase_if(songs, [&](const SongInfo& s) { return !owned->Song(s.id); });
    if (songs.empty()) {  // only when every song is an add-on the user does not have
        LogError("song select: no owned songs");
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
    LogInfo("song select: {} songs, {} stages, {}", songs.size(), stages.size(), home ? "Play Live Show" : "Play Live Show (AC)");
    // first focus: CHALLENGE in the arcade mode (the port's choice so far), STANDARD in the home mode (the game, M10)
    int current = 0, difficulty = home ? 1 : mode.Count() - 1, stage = 0;
    for (size_t i = 0; i < songs.size(); ++i) {
        if (songs[i].id != previous.song) continue;
        current = int(i);
        if (live::IsHomeVariant(previous.variant) == home) difficulty = home ? previous.variant - 10 : previous.variant;
    }
    const std::string last_stage = previous.stage.empty() ? "st017" : previous.stage;  // else the live default
    for (size_t i = 0; i < stages.size(); ++i) {
        char folder[16];
        std::snprintf(folder, sizeof(folder), "st%03d", stages[i].model);
        if (last_stage == folder) stage = int(i);
    }
    std::vector<DressInfo> dresses, english;  // names in the UI language, and English (for the default pick)
    if (!LoadDressList(vfs, language, dresses, &error) || !LoadDressList(vfs, 1, english, &error)) {
        LogWarn("song select: costumes: {}", error);
    }
    std::vector<DressInfo> costumes;  // the current song's, built when the costume step opens
    int costume = 0;
    const auto open_costumes = [&] {
        const SongInfo& s = songs[size_t(current)];
        costumes = CostumesFor(vfs, s, dresses);
        if (costume_data && save) {
            std::erase_if(costumes, [&](const DressInfo& d) { return !costume::Owns(*costume_data, *save, owned, d.id); });
        } else if (owned) {
            std::erase_if(costumes, [&](const DressInfo& d) { return !owned->Costume(d.id); });
        }
        const std::string want =
            previous.song == s.id && !previous.costume.empty() ? previous.costume : live::PickCostume(vfs, s, english);
        costume = 0;
        for (size_t i = 0; i < costumes.size(); ++i) {
            if (live::CostumeFolder(costumes[i].id) == want) costume = int(i);
        }
    };
    Messages messages;
    if (skills && !messages.Load(vfs, language, &error)) LogWarn("song select: messages: {}", error);
    const Step last_step = skills ? kPrepareStep : kCostumeStep;
    const CostumeArt art{skills ? skills->cards : nullptr, skills ? skills->member : 1, costume_data, language};
    int prepare = 3;  // the Prepare step's focus: 0-2 the sub-buttons, 3 LIVE START
    Step step = kSongStep;
    SongChoice choice;
    bool running = true;
    ScreenFade fade;
    Apply(ui, mode, songs, current, step, difficulty, stages, stage, costumes, costume, save, art);
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; running && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) running = false, choice.closed = true;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int n = int(songs.size()), ns = std::max(1, int(stages.size())), nc = std::max(1, int(costumes.size()));
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT:
                    if (step == kPrepareStep) prepare = prepare == 2 ? 0 : std::max(0, prepare - 1);  // Profile: greyed
                    else if (step == kDifficultyStep) difficulty = std::max(0, difficulty - 1);
                    else if (step == kStageStep) stage = (stage + ns - 1) % ns;
                    else if (step == kCostumeStep) costume = (costume + nc - 1) % nc;
                    else current = (current + n - 1) % n;
                    break;
                case SDL_SCANCODE_RIGHT:
                    if (step == kPrepareStep) prepare = prepare == 0 ? 2 : std::min(3, prepare + 1);
                    else if (step == kDifficultyStep) difficulty = std::min(mode.Count() - 1, difficulty + 1);
                    else if (step == kStageStep) stage = (stage + 1) % ns;
                    else if (step == kCostumeStep) costume = (costume + 1) % nc;
                    else current = (current + 1) % n;
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE:
                    if (step == kDifficultyStep && Locked(mode, save, songs[size_t(current)].id, difficulty)) {
                        LogInfo("song select: difficulty {} is locked", difficulty);
                        break;
                    }
                    if (step == kPrepareStep && e.key.scancode == SDL_SCANCODE_SPACE && prepare == 2) {
                        RunSkillSelect(window, renderer, vfs, lang, *skills, max_frames);
                        route::SetScreen("prepare");
                        SDL_SetWindowTitle(window, "SIF port - prepare for the live show");
                        break;
                    }
                    if (step == kPrepareStep && e.key.scancode == SDL_SCANCODE_SPACE && prepare != 3) {
                        LogInfo("song select: prepare button {} (not built)", prepare);
                        break;
                    }
                    // the next step with something to choose; after the last one the live starts (Enter = Options
                    // starts it from anywhere on Prepare)
                    step = Step(step + 1);
                    if (step == kStageStep && stages.empty()) step = kCostumeStep;
                    if (step == kCostumeStep) {
                        open_costumes();
                        if (costumes.empty()) step = Step(kCostumeStep + 1);
                    }
                    if (step == kPrepareStep) {
                        prepare = 3;
                        route::SetScreen("prepare");
                        LogInfo("song select: prepare for the live show");
                    }
                    if (step > last_step) {
                        step = last_step;
                        choice = {songs[size_t(current)].id, mode.Variant(difficulty)};
                        if (!stages.empty()) {
                            char folder[16];
                            std::snprintf(folder, sizeof(folder), "st%03d", stages[size_t(stage)].model);
                            choice.stage = folder;
                        }
                        if (!costumes.empty()) choice.costume = live::CostumeFolder(costumes[size_t(costume)].id);
                        fade.Leave(ui);
                    }
                    break;
                case SDL_SCANCODE_ESCAPE:
                case SDL_SCANCODE_BACKSPACE:
                    // one step back; from the song list back to the top menu (M9: the else used to leave on any Esc)
                    if (step == kSongStep) {
                        fade.Leave(ui);
                        break;
                    }
                    step = Step(step - 1);
                    if (step == kCostumeStep && costumes.empty()) step = kStageStep;
                    if (step == kStageStep && stages.empty()) step = kDifficultyStep;
                    route::SetScreen("select");
                    break;
                default: break;
            }
            Apply(ui, mode, songs, current, step, difficulty, stages, stage, costumes, costume, save, art);
            if (step == kPrepareStep) ApplyPrepare(ui, skills, messages, language, prepare);
            const SongInfo& s = songs[size_t(current)];
            char title[256];
            std::snprintf(title, sizeof(title), "SIF port - %04d %s%s", s.id, s.title.c_str(),
                          step == kDifficultyStep ? (" - difficulty " + std::to_string(difficulty)).c_str()
                          : step == kStageStep    ? (" - stage " + stages[size_t(stage)].name).c_str()
                          : step == kCostumeStep && !costumes.empty() ? (" - costume " + costumes[size_t(costume)].name).c_str()
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
        if (step == kPrepareStep) ui.DrawLayout(renderer, kPrepare, 1.0f);
        if (step == kCostumeStep && !costumes.empty()) ui.DrawLayout(renderer, kCostumeLayout, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    if (choice.song >= 0) LogInfo("song select: song {:04d}, variant {:02d}", choice.song, choice.variant);
    return choice;
}

}
