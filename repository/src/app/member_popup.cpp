#include "app/member_popup.h"

#include <cstdio>

#include "engine/core/log.h"
#include "game/member/member.h"

namespace sif {
namespace {

constexpr const char* kTop = "uil0010_PopMemberTop00";
const std::string kList = std::string(kTop) + "/uil0011_PopMemberSelect00";
const std::string kProf = std::string(kTop) + "/uil0012_PopMemberProf00";
const std::string kFrame = kList + "/uip0030_MemberChange00";
const std::string kCard = kProf + "/uip0031_MemberChange01";
// The icon faces (com_ch_chara_icon00_T) and the profile's member marks (com_ch_member_icon00) are 5 x 4 atlases in
// member index order; the parts have no clips, so the cell is set as a UV offset
void Cell(UiScreen& ui, const std::string& instance, const char* material, int index) {
    ui.SetMaterialUv(instance, material, float(index % 5) * 0.2f, float(index / 5) * 0.25f);
}

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

std::string Icon(int index) {
    char s[48];
    std::snprintf(s, sizeof(s), "/uip0032_MemberChangeIcon00_%02d", index);
    return kList + s;
}

// The members of a mode in list order: µ's 1-9; Aqours 11-19 then Saint Snow 21-22 (indexes 0-8 / 9-19)
int First(int mode) { return mode == 0 ? 0 : 9; }
int Count(int mode) { return mode == 0 ? 9 : 11; }

}

bool MemberPopup::Open(Renderer& renderer, const Vfs& vfs, const std::string& lang, int mode, int current) {
    if (!loaded_) {
        std::string error;
        if (!text_.Load(vfs, lang, &error)) LogWarn("member popup: text: {}", error);
        if (!ui_.Load(renderer, vfs, "ui2/00_common", lang, &error, &text_)) {
            LogError("member popup: {}", error);
            return false;
        }
        if (!messages_.Load(vfs, Language(lang), &error)) LogWarn("member popup: messages: {}", error);
        loaded_ = true;
    }
    mode_ = mode == 1 ? 1 : 0;
    const int i = member::Index(current);
    focus_ = i >= First(mode_) && i < First(mode_) + Count(mode_) ? i - First(mode_) : 0;
    stage_ = Stage::List;
    Apply();
    LogInfo("member popup: open, mode {}", mode_);
    return true;
}

void MemberPopup::Apply() {
    ui_.HideInstance(kProf, stage_ != Stage::Profile);
    for (int i = 0; i < 20; ++i) {
        const bool shown = i >= First(mode_) && i < First(mode_) + Count(mode_);
        const std::string icon = Icon(i);
        ui_.HideInstance(icon, !shown);
        Cell(ui_, icon, "character_icon01", i);
        const bool focused = shown && i - First(mode_) == focus_;
        ui_.HideNode(icon, "character_icon00_on", !focused);
        ui_.HideNode(icon, "character_icon00_eff", !focused);
        ui_.HideNode(icon, "character_icon00_off", focused);
    }
    ui_.HideNode(kFrame, "layout_ms00", mode_ != 0);
    ui_.HideNode(kFrame, "layout_aq00", mode_ != 1);
    ui_.SetInstanceText(kFrame, 0, messages_.Get(590));  // "Choose a Member."
    ui_.SetInstanceText(kFrame, 1, "");
    if (stage_ != Stage::Profile) return;
    const int m = member::kAll[size_t(First(mode_) + focus_)];
    const member::Profile p = member::ProfileOf(m);
    const int group = member::Group(m);
    ui_.HideNode(kCard, "prof_bg00_ms", group != 0);
    ui_.HideNode(kCard, "prof_bg00_aq", group != 1);
    ui_.HideNode(kCard, "prof_bg00_ss", group != 2);
    char sd[64];
    std::snprintf(sd, sizeof(sd), "ui/ui_sd_character.pac/texture.pac/sd_character_menu%02d", m);
    if (!ui_.SetMaterialTexture(kCard, "sd_character00_M", sd)) LogWarn("member popup: no {}", sd);
    Cell(ui_, kCard, "member_mark00_M", member::Index(m));
    // the boxes (probed): 0 the question, 1 the name, 2 its reading (empty: the English game shows none), 3-8 the
    // labels, 9-14 the values
    const int labels[6] = {member::kSchoolYear, member::kBirthday, member::kBloodType, member::kFavourite, member::kHated,
                           member::kSubUnit};
    const int values[6] = {p.year, p.birthday, p.blood, p.favourite, p.hated, p.unit};
    ui_.SetInstanceText(kCard, 0, messages_.Get(513));  // "Continue with this member?"
    ui_.SetInstanceText(kCard, 1, messages_.Get(p.name));
    ui_.SetInstanceText(kCard, 2, "");
    for (int k = 0; k < 6; ++k) {
        ui_.SetInstanceText(kCard, 3 + k, messages_.Get(labels[k]));
        ui_.SetInstanceText(kCard, 9 + k, messages_.Get(values[k]));
    }
    ui_.SetMaterialUv(kProf + "/uip0005_SelectButton00", "button_text00_M", 0, 0.375f);  // "OK" (as skill_screen.cpp)
}

MemberPopup::Result MemberPopup::Key(SDL_Scancode key) {
    if (stage_ == Stage::Closed) return Result::Closed;
    if (stage_ == Stage::Profile) {
        switch (key) {
            case SDL_SCANCODE_SPACE:
            case SDL_SCANCODE_RETURN:
                chosen_ = member::kAll[size_t(First(mode_) + focus_)];
                stage_ = Stage::Closed;
                LogInfo("member popup: chose member {}", chosen_);
                return Result::Chosen;
            case SDL_SCANCODE_ESCAPE: stage_ = Stage::List; break;
            default: return Result::Open;
        }
        Apply();
        return Result::Open;
    }
    const int n = Count(mode_);
    switch (key) {
        case SDL_SCANCODE_ESCAPE:
            stage_ = Stage::Closed;
            LogInfo("member popup: closed");
            return Result::Closed;
        case SDL_SCANCODE_LEFT: focus_ = (focus_ + n - 1) % n; break;
        case SDL_SCANCODE_RIGHT: focus_ = (focus_ + 1) % n; break;
        // shortcut: rows of 4, then 5 (and Saint Snow's 2); up/down move by 4, as the first row
        case SDL_SCANCODE_UP: focus_ = std::max(0, focus_ - 4); break;
        case SDL_SCANCODE_DOWN: focus_ = std::min(n - 1, focus_ + 4); break;
        case SDL_SCANCODE_PAGEDOWN:
        case SDL_SCANCODE_PAGEUP:
            if ((key == SDL_SCANCODE_PAGEDOWN) == (mode_ == 0)) mode_ = 1 - mode_, focus_ = 0;
            break;
        case SDL_SCANCODE_SPACE:
        case SDL_SCANCODE_RETURN: stage_ = Stage::Profile; break;
        default: return Result::Open;
    }
    Apply();
    LogInfo("member popup: mode {}, focus member {}{}", mode_, member::kAll[size_t(First(mode_) + focus_)],
            stage_ == Stage::Profile ? ", profile" : "");
    return Result::Open;
}

void MemberPopup::Draw(Renderer& renderer) const {
    if (stage_ != Stage::Closed) ui_.DrawLayout(renderer, kTop, 1.0f);
}

void MemberPopup::Release(Renderer& renderer) {
    if (loaded_) ui_.Release(renderer);
    loaded_ = false;
}

}
