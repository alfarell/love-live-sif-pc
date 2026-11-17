#include "app/result_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
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

constexpr const char* kRoot = "uil0001_ResultTop00";
constexpr const char* kLayout = "uil0001_ResultTop00/uil0002_ResultLayout00";
// The Items Obtained page (shadPS4, M11 S2 s22): drawn instead of uil0002_ResultLayout00
const std::string kReward = "uil0003_RewardLayout00";
// the list scrolls inside a stencil: _00 sits under the left arrow, so the visible icons are _01-_08 (as in s22)
constexpr int kIcons = 8, kKeySlots = 2, kStandardSlots = 16;

std::string Two(const char* format, int k) {
    char s[96];
    std::snprintf(s, sizeof(s), format, k);
    return s;
}
std::string IconPath(int k) { return kReward + "/uil0011_RewNomalList00" + Two("/uip0020_RewIcon00_%02d", k + 1); }

// Fills a message's "%s" places in order.
std::string Fill(std::string text, std::initializer_list<std::string> values) {
    for (const std::string& v : values) {
        const size_t at = text.find("%s");
        if (at == std::string::npos) break;
        text.replace(at, 2, v);
    }
    return text;
}

// "Lv. 3", "Lv.MAX" (messages 555, 556)
std::string LevelText(const Messages& m, int level, int max_level) {
    if (level >= max_level) return m.Get(556);
    char s[32];
    std::snprintf(s, sizeof(s), m.Get(555).c_str(), level);
    return s;
}

// Items Obtained: one icon per skill card (uil0011_RewNomalList00), the materials split into key materials (two
// slots) and standard ones (16) with "x amount" and the new total (uil0012_RewMaterialList00).
// shortcut: Friendship Points (uip0010_PlayerLevel00) are not built, so the bar is hidden; profile cards, boosters and
// badges are never dropped by the port
void FillRewards(UiScreen& ui, const LiveResult& r, const ResultItems& items, const Messages& m) {
    ui.HideInstance(kReward + "/uip0010_PlayerLevel00", true);
    const std::string list = kReward + "/uil0011_RewNomalList00";
    ui.HideInstance(list + "/common_uip0007_ArrowRight00", true);
    ui.HideInstance(list + "/common_uip0006_ArrowLeft00", true);
    ui.HideInstance(list + "/uip0020_RewIcon00_00", true);
    for (int k = 0; k < kIcons; ++k) {
        ui.HideInstance(IconPath(k), k >= int(r.skills.size()));
        ui.HideNode(IconPath(k), "icon_frame00_eff", true);
    }
    const std::string mats = kReward + "/uil0012_RewMaterialList00";
    ui.SetInstanceText(mats + "/uip0012_RewMaterialWin00", 0, m.Get(548));  // [Key Materials]
    ui.SetInstanceText(mats + "/uip0012_RewMaterialWin00", 1, m.Get(549));  // [Standard Materials]
    int keys = 0, standard = 0;
    for (const auto& [item, amount] : r.materials) {
        const costume::Item* i = items.costumes ? costume::FindItem(*items.costumes, item) : nullptr;
        const bool key = i && (i->type == 1 || i->type == 2);  // key_ms, key_aq
        if (key ? keys >= kKeySlots : standard >= kStandardSlots) continue;
        const std::string row = key ? mats + Two("/common_uip0018_MaterialChange00_%02d", keys++)
                                    : mats + Two("/common_uip0019_MaterialChange01_%02d", standard++);
        if (i) ui.SetMaterialTexture(row, "icon_materiai00_M", costume::IconPath(*i));
        ui.SetInstanceNumber(row, 0, "x" + std::to_string(amount));  // "x1 (17)": the gain and the new total
        if (items.save) ui.SetInstanceNumber(row, 1, "(" + std::to_string(costume::ItemCount(*items.save, item)) + ")");
    }
    for (int k = keys; k < kKeySlots; ++k) ui.HideInstance(mats + Two("/common_uip0018_MaterialChange00_%02d", k), true);
    for (int k = standard; k < kStandardSlots; ++k) ui.HideInstance(mats + Two("/common_uip0019_MaterialChange01_%02d", k), true);
}

// The focused skill card: its picture (uil0013_RewPreview00), "Skill Card", and member, skill and level (message
// 560; a duplicate shows "old → new", message 554).
void ShowCard(UiScreen& ui, const LiveResult& r, const ResultItems& items, const Messages& m, int language,
              const std::string& lang, int focus) {
    const std::string preview = kReward + "/uil0013_RewPreview00", window = kReward + "/uip0015_RewWindow00";
    const card::SkillCard* c =
        focus < int(r.skills.size()) && items.cards ? card::FindSkill(*items.cards, r.skills[size_t(focus)].card) : nullptr;
    ui.HideInstance(preview, !c);
    ui.HideInstance(window, !c);
    for (int k = 0; k < kIcons; ++k) {
        ui.HideNode(IconPath(k), "icon_frame00_on", k != focus);
        ui.HideNode(IconPath(k), "icon_frame00_off", k == focus);
    }
    if (!c) return;
    const LiveResult::Skill& s = r.skills[size_t(focus)];
    ui.SetMaterialTexture(preview + "/uip0013_Reward00", "rew_card00_M", SkillThumbPath(c->id, lang, false));
    for (const char* node : {"rew_card_frame01", "boost_item00"}) ui.HideNode(preview + "/uip0013_Reward00", node, true);
    for (const char* inst : {"/ctg03_uip0006_Badge00", "/common_uip0020_CheckMark00_00", "/common_uip0020_CheckMark00_01"}) {
        ui.HideInstance(preview + inst, true);
    }
    ui.SetInstanceText(preview + "/uip0014_RewName00", 0, m.Get(550));
    ui.SetInstanceText(preview + "/uip0013_Reward00", 0, "");  // a profile card's caption
    ui.SetInstanceText(window, 1, "");                         // the profile card note (message 563)
    std::string level = LevelText(m, s.level, c->max_level);
    if (s.level_before > 0 && s.level_before != s.level) level = Fill(m.Get(554), {LevelText(m, s.level_before, c->max_level), level});
    // member names: messages 197-216 in the drop table's member order (µ's 1-9, Aqours 11-19, Saint Snow 21-22)
    const int index = c->member <= 9 ? c->member - 1 : c->member <= 19 ? c->member - 2 : c->member - 3;
    ui.SetInstanceText(window, 0, Fill(m.Get(560), {m.Get(197 + index), c->name[size_t(language)], level}));
}

}

bool RunLiveViewEnd(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& r,
                    const std::string& stage, int member_card, long max_frames, std::vector<bool>& favourites) {
    route::SetScreen("live_end");
    favourites.assign(r.photos.size(), false);
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("live end: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/95_result", lang, &error, &text)) {
        LogError("live end: {}", error);
        return true;
    }
    ui.HideNode("new00", true);
    ui.HideNode("complete00", true);
    // the header: "View Live Show:" (res_title_text00_T row 3 of 4) and "End the Live Show" (res_title_text01_T row 3)
    const std::string title = std::string(kRoot) + "/uip0001_ResuitTitle00";
    ui.SetMaterialUv(title, "title_mode00_M", 0, 0.5f);
    ui.SetMaterialUv(title, "title_page00_M", 0, 0.5f);
    ui.HideInstance(kLayout, true);
    const std::string end = "uil0020_LiveEndLayout00";
    // shortcut: the 3D member on the right (uip0042_ViewChara00, a render target) is not drawn; the menus have no 3D
    ui.HideInstance(end + "/uip0042_ViewChara00", true);
    // the song plate: cover, title (box 0), "Stage / Costume:" (box 1), the stage thumbnail and the member card
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    const std::string plate = end + "/uip0040_SondData01";
    std::vector<live::SongInfo> songs;
    if (live::LoadSongList(vfs, language, songs)) {
        for (const live::SongInfo& s : songs) {
            if (s.id != r.song) continue;
            ui.SetInstanceText(plate, 0, s.title);
            const bool aqours = s.group != 0;
            const std::string bg = std::string(kRoot) + "/common_uil0101_ComBGGroup_Member01";
            ui.HideInstance(bg + "/common_uip0400_ComTopBG_Ms00", aqours);
            ui.HideInstance(bg + "/common_uip0410_ComTopBG_Aqours00", !aqours);
            ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab00", "select_group00", aqours);
            ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab00", "select_group01", !aqours);
            char cover[96];
            std::snprintf(cover, sizeof(cover), "ui/ui_music_thumbnail.pac/texture.pac/cd_cover_%04d", s.id);
            ui.SetMaterialTexture(plate, "cd_jacket00_M", cover);
        }
    }
    std::vector<live::StageInfo> stages;
    int model = 0;
    std::sscanf(stage.c_str(), "st%d", &model);
    if (live::LoadStageList(vfs, language, stages)) {
        for (const live::StageInfo& s : stages) {
            if (s.model != model) continue;
            char thumb[80];
            std::snprintf(thumb, sizeof(thumb), "ui/ui_stage_thumbnail.pac/texture.pac/stage_%03d", s.id);
            ui.SetMaterialTexture(plate, "stage_preview00_M", thumb);
        }
    }
    if (member_card > 0) {
        ui.SetMaterialTexture(plate, "card_preview00_M",
                              "ui/ui_member_card_thumbnail.pac/texture.pac/mbc_" + std::to_string(member_card));
    }
    // the photos: textures made from the pictures (the large preview and the five small ones show the same picture)
    std::vector<Texture> textures(r.photos.size());
    for (size_t k = 0; k < r.photos.size(); ++k) {
        const LiveResult::Photo& p = r.photos[k];
        if (!renderer.CreateTexture(p.width, p.height, textures[k], &error) || !renderer.UpdateTexture(textures[k], p.rgba)) {
            LogWarn("live end: photo {}: {}", k + 1, error);
        }
    }
    const std::string list = end + "/uip0041_PhotoList00";
    // message 575 "Photos taken this time: " + n, 567 "Stage / Costume:" (the plate's box 1 is a JP placeholder)
    Messages messages;
    if (!messages.Load(vfs, language, &error)) LogWarn("live end: messages: {}", error);
    const auto msg = [&](int id, const char* fallback) {
        const std::string& m = messages.Get(id);
        return m.empty() ? std::string(fallback) : m;
    };
    ui.SetInstanceText(list, 0, msg(575, "Photos taken this time: ") + std::to_string(r.photos.size()));
    ui.SetInstanceText(plate, 1, msg(567, "Stage / Costume:"));
    int focus = 0;
    const auto apply = [&] {
        for (int k = 0; k < 5; ++k) {
            char node[40];
            std::snprintf(node, sizeof(node), "photo_preview_S%02d", k);
            const bool on = size_t(k) < r.photos.size();
            ui.HideNode(list, node, !on);
            ui.HideNode(list, std::string(node) + "_eff", k != focus);
            if (on) ui.SetMaterialTexture(list, std::string(node) + "_ss", &textures[size_t(k)]);
            const std::string mark = end + "/uil0021_LiveEndCheckMark/common_uip0020_CheckMark00_0" + std::to_string(k);
            ui.HideInstance(mark, !on);
            if (on) ui.SetMaterialUv(mark, "check00_M", favourites[size_t(k)] ? 0.5f : 0.0f, 0);
        }
        // shortcut: no photo shows the layout's "no image" text; the game skips this screen then (not seen)
        if (!r.photos.empty()) ui.SetMaterialTexture(list, "photo_preview_L00_ss", &textures[size_t(focus)]);
    };
    apply();
    ui.Play("*", UiScreen::ClipName(1), false);  // "in"
    SDL_SetWindowTitle(window, "SIF port - end the live show");
    LogInfo("live end: {} photos", r.photos.size());
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || fade.leaving) continue;
            const int n = std::max(1, int(r.photos.size()));
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT: focus = (focus + n - 1) % n; break;
                case SDL_SCANCODE_RIGHT: focus = (focus + 1) % n; break;
                case SDL_SCANCODE_Q:
                    if (size_t(focus) < favourites.size()) {
                        favourites[size_t(focus)] = !favourites[size_t(focus)];
                        LogInfo("live end: photo {} favourite {}", focus + 1, favourites[size_t(focus)] ? "on" : "off");
                    }
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_SPACE: fade.Leave(ui); break;
                default: break;
            }
            apply();
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
        ui.DrawLayout(renderer, end, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    for (Texture& t : textures) renderer.Release(t);
    return open;
}

// shortcut: the result screen's own logic (eboot) is not traced: the combo result (FULL COMBO), records and rewards
// show their parts' defaults; the values the port knows are filled in (score, counts, max combo, clear rate, rank,
// title, cover)
bool RunResult(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& r,
               long max_frames, const save::NewBests& news, const ResultItems* items) {
    route::SetScreen("result");
    UiText text;
    std::string error;
    if (!text.Load(vfs, lang, &error)) LogWarn("result: text: {}", error);
    UiScreen ui;
    if (!ui.Load(renderer, vfs, "ui2/95_result", lang, &error, &text)) {
        LogError("result: {}", error);
        return true;
    }
    ui.HideNode("new00", true);
    ui.HideNode("complete00", true);
    // the play-score window: number boxes 0 score, 1 max combo, 2 clear rate (its digit row has '.' and '%'; the
    // two-decimal format is a guess)
    const std::string score = std::string(kLayout) + "/uil0005_PlayScoreWin00/uip0003_PlayScore00";
    ui.SetInstanceNumber(score, 0, std::to_string(long(r.score)));
    ui.SetInstanceNumber(score, 1, std::to_string(r.max_combo));
    char rate[16];
    std::snprintf(rate, sizeof(rate), "%.2f%%", std::floor(r.clear_rate * 10000) / 100);
    ui.SetInstanceNumber(score, 2, rate);
    // the rank letter: res_result_evaluation00 holds D C B A over S SS SSS in cells of 1/4 x 1/2
    const std::string rank = std::string(kLayout) + "/uil0007_ScoreRateGroup00/uip0005_ScoreRate00";
    for (const char* material : {"score_rate_M", "score_rate_eff_M"}) {
        ui.SetMaterialUv(rank, material, 0.25f * float(r.rank % 4), 0.5f * float(r.rank / 4));
    }
    // the FULL COMBO banner: only when nothing broke the combo (Good, Bad and Miss do)
    if (r.counts[2] + r.counts[3] + r.counts[4] > 0) ui.HideInstance(std::string(kLayout) + "/uip0004_ComboResult00", true);
    // "NEW" by the score, max combo and clear rate rows (Hypothesis: New_00..02 follow the rows top to bottom)
    const bool is_new[3] = {news.score, news.combo, news.rate};
    for (int k = 0; k < 3; ++k) {
        ui.HideInstance(std::string(kLayout) + "/uil0005_PlayScoreWin00/common_uip0017_New_0" + std::to_string(k), !is_new[k]);
    }
    // the judgement list, top to bottom (Hypothesis: Perfect .. Miss, as on the result screens seen in shadPS4)
    for (int k = 0; k < 5; ++k) {
        char path[160];
        std::snprintf(path, sizeof(path), "%s/uil0006_JudgNumberList00/uip0006_JudgNumber00_%02d", kLayout, k);
        ui.SetInstanceNumber(path, 0, std::to_string(r.counts[size_t(k)]));
        // res_judge_text00 stacks the five labels (PERFECT .. MISS) in rows of 1/5 of the texture (code-set, Hypothesis)
        ui.SetMaterialUv(path, "Judg_text00_M", 0, 0.2f * float(k));
    }
    // song data: cover and title; the "records so far" lines need a save
    std::vector<live::SongInfo> songs;
    const int language = lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0;
    const std::string song_data = std::string(kLayout) + "/uip0002_SongData00";
    if (live::LoadSongList(vfs, language, songs)) {
        for (const live::SongInfo& s : songs) {
            if (s.id != r.song) continue;
            ui.SetInstanceText(song_data, 0, s.title);
            // the group header (00_common parts) follows the song, as in the song select (it showed Aqours always)
            const bool aqours = s.group != 0;
            const std::string bg = std::string(kRoot) + "/common_uil0101_ComBGGroup_Member01";
            ui.HideInstance(bg + "/common_uip0400_ComTopBG_Ms00", aqours);
            ui.HideInstance(bg + "/common_uip0410_ComTopBG_Aqours00", !aqours);
            ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab00", "select_group00", aqours);
            ui.HideNode(std::string(kRoot) + "/common_uip0008_GroupTab00", "select_group01", !aqours);
            char cover[96];
            std::snprintf(cover, sizeof(cover), "ui/ui_music_thumbnail.pac/texture.pac/cd_cover_%04d", s.id);
            ui.SetMaterialTexture(song_data, "cd_jacket00_M", cover);
            // difficulty: res_difficulty_level00 has the song select's label order (4 x 4: home BEGINNER .. ADVENTURE,
            // then EASY(AC) .. CHALLENGE(AC)); res_difficulty_star_plate00 one colour per arcade difficulty (5 x 1);
            // the level badge is 00_common's (cell = level - 1). Variants 00-04 are difficulty indexes 0-4, 10-13 are 5-8
            const bool home = r.variant >= 10;
            const int label = home ? r.variant - 10 : 4 + r.variant, index = home ? r.variant - 5 : r.variant;
            const int level = index >= 0 && index < 9 ? s.stars[size_t(index)] : 0;
            const int cell = std::clamp(level, 1, 15) - 1;
            const std::string window = std::string(kLayout) + "/uil0005_PlayScoreWin00";
            for (const std::string& part : {song_data, score}) {
                ui.SetMaterialUv(part, "diff_plate00_M", 0.25f * float(label % 4), 0.25f * float(label / 4));
            }
            ui.SetMaterialUv(score, "star_bg00_M", 0.2f * float(std::clamp(home ? r.variant - 10 : r.variant, 0, 4)), 0);
            ui.SetMaterialUv(window + "/common_uip0010_DifficultyStar00", "difficulty_star00", 0.25f * float(cell % 4),
                             0.25f * float(cell / 4));
        }
    }
    for (int box = 1; box < 5; ++box) ui.SetInstanceText(song_data, box, "");
    ui.HideNode(song_data, "old_rate00", true);
    ui.Play("*", UiScreen::ClipName(1), false);  // "in"
    SDL_SetWindowTitle(window, "SIF port - result");
    LogInfo("result: song {:04d}/{:02d}, score {:.0f}, max combo {}, clear rate {}, rank {}", r.song, r.variant, r.score,
            r.max_combo, rate, r.rank);
    Messages messages;
    if (items && !messages.Load(vfs, language, &error)) LogWarn("result: messages: {}", error);
    bool rewards = false;  // the Items Obtained page is showing
    int focus = 0;
    const auto turn_page = [&] {
        rewards = true;
        route::SetScreen("result_items");
        ui.HideInstance(kLayout, true);
        ui.SetMaterialUv(std::string(kRoot) + "/uip0001_ResuitTitle00", "title_page00_M", 0.5f, 0);  // "Items Obtained"
        FillRewards(ui, r, *items, messages);
        ShowCard(ui, r, *items, messages, language, lang, focus);
        ui.Play("*", UiScreen::ClipName(1), false);
        std::string cards;
        for (const LiveResult::Skill& s : r.skills) cards += " " + std::to_string(s.card);
        LogInfo("result: items obtained: {} skill cards{}, {} materials", r.skills.size(), cards, r.materials.size());
    };
    bool open = true;
    ScreenFade fade;
    uint64_t last = SDL_GetTicksNS();
    for (long frame = 0; open && !fade.Left() && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) open = false;
            if (!((e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)) continue;
            const bool left = e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_LEFT;
            const bool right = e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_RIGHT;
            if (items && !rewards) {
                turn_page();
            } else if (rewards && (left || right)) {
                const int n = std::max(1, int(r.skills.size()));
                focus = (focus + (right ? 1 : n - 1)) % n;
                ShowCard(ui, r, *items, messages, language, lang, focus);
                if (focus < int(r.skills.size())) LogInfo("result: skill card {}", r.skills[size_t(focus)].card);
            } else {
                fade.Leave(ui);
            }
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
        if (rewards) ui.DrawLayout(renderer, kReward, 1.0f);
        fade.Draw(renderer);
        renderer.EndFrame();
    }
    ui.Release(renderer);
    return open;
}

}
