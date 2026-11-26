#include "app/member_popup.h"

#include <algorithm>
#include <cstdio>

#include "engine/core/log.h"
#include "game/member/member.h"

namespace sif {
namespace {

constexpr const char* kTop = "uil0010_PopMemberTop00";
constexpr const char* kFirst = "uil0051_selectMember00";  // ui2/01_title
// The member loading splash (eboot FUN_00263570: the shared loader in mode 1, its picture _charaTex_splash00 =
// com_load_mem00_<member>; M18 U1, U2 shadPS4 work/m18/shad/splash/). The game keeps it until the member is loaded,
// 0.3-0.5 s in shadPS4. shortcut: a fixed time with a short fade out, and the port loads her before its first frame
constexpr const char* kSplash = "uip0072_ChrChangeSplash00";
constexpr double kSplashSeconds = 0.5, kSplashFade = 0.15;
// the popup's parts below its top instance
const std::string kList = "/uil0011_PopMemberSelect00";
const std::string kProf = "/uil0012_PopMemberProf00";
const std::string kFrame = kList + "/uip0030_MemberChange00";
const std::string kCard = kProf + "/uip0031_MemberChange01";
// The icon faces (com_ch_chara_icon00_T) and the profile's member marks (com_ch_member_icon00) are 5 x 4 atlases in
// member index order; the parts have no clips, so the cell is set as a UV offset
void Cell(UiScreen& ui, const std::string& instance, const char* material, int index) {
    ui.SetMaterialUv(instance, material, float(index % 5) * 0.2f, float(index / 5) * 0.25f);
}

int Language(const std::string& lang) { return lang == "en" ? 1 : lang == "cn" ? 2 : lang == "tw" ? 3 : 0; }

std::string Icon(const std::string& top, int index) {
    char s[48];
    std::snprintf(s, sizeof(s), "/uip0032_MemberChangeIcon00_%02d", index);
    return top + kList + s;
}

// The members of a mode in list order: µ's 1-9; Aqours 11-19 then Saint Snow 21-22 (indexes 0-8 / 9-19)
int First(int mode) { return mode == 0 ? 0 : 9; }
int Count(int mode) { return mode == 0 ? 9 : 11; }

}

bool MemberPopup::Load(Renderer& renderer, const Vfs& vfs, const std::string& lang, const std::string& screen) {
    if (loaded_) return true;
    std::string error;
    if (!text_.Load(vfs, lang, &error)) LogWarn("member popup: text: {}", error);
    if (!ui_.Load(renderer, vfs, screen, lang, &error, &text_)) {
        LogError("member popup: {}", error);
        return false;
    }
    if (!messages_.Load(vfs, Language(lang), &error)) LogWarn("member popup: messages: {}", error);
    loaded_ = true;
    return true;
}

bool MemberPopup::OpenFirst(Renderer& renderer, const Vfs& vfs, const std::string& lang) {
    first_ = true;
    root_ = kFirst, top_ = root_ + "/common_uil0010_PopMemberTop00";
    if (!Load(renderer, vfs, lang, "ui2/01_title")) return false;
    ui_.HideInstance(root_ + "/uip0053_nameset_Chara00", true);  // a placeholder where the game draws her in 3D
    ui_.SetInstanceText(root_ + "/uip0057_MemberSelectText00", 0, messages_.Get(697));
    ui_.SetInstanceText(root_ + "/common_uip0001_Ticker00", 0, messages_.Get(489));
    mode_ = 0, focus_ = 0;
    stage_ = Stage::List;
    Apply();
    LogInfo("member popup: first member, mode 0");
    return true;
}

bool MemberPopup::Choosable(int index) const {
    const int m = member::kAll[size_t(First(mode_) + index)];
    return singers_.empty() || std::find(singers_.begin(), singers_.end(), m) != singers_.end();
}

bool MemberPopup::OpenSingers(Renderer& renderer, const Vfs& vfs, const std::string& lang, int mode,
                              const std::vector<int>& singers) {
    int first = 0;
    for (int m : singers) {
        const int i = member::Index(m) - First(mode == 1 ? 1 : 0);
        if (i >= 0 && i < Count(mode == 1 ? 1 : 0)) {
            first = m;
            break;
        }
    }
    singers_ = singers;
    return Open(renderer, vfs, lang, mode, first);
}

int MemberPopup::Focused() const { return member::kAll[size_t(First(mode_) + focus_)]; }

void MemberPopup::DrawTicker(Renderer& renderer) const {
    if (first_ && stage_ != Stage::Closed) ui_.DrawInstance(renderer, root_ + "/common_uip0001_Ticker00", 1.0f);
}

bool MemberPopup::Open(Renderer& renderer, const Vfs& vfs, const std::string& lang, int mode, int current) {
    root_ = top_ = kTop;
    if (!Load(renderer, vfs, lang, "ui2/00_common")) return false;
    mode_ = mode == 1 ? 1 : 0;
    const int i = member::Index(current);
    focus_ = i >= First(mode_) && i < First(mode_) + Count(mode_) ? i - First(mode_) : 0;
    stage_ = Stage::List;
    Apply();
    LogInfo("member popup: open, mode {}", mode_);
    return true;
}

void MemberPopup::Apply() {
    const std::string prof = top_ + kProf, frame = top_ + kFrame, card = top_ + kCard;
    ui_.HideInstance(prof, stage_ != Stage::Profile);
    if (first_) {  // the group's background and header tab; the focused member's description
        const bool aqours = mode_ == 1;
        ui_.HideInstance(root_ + "/common_uil0101_ComBGGroup_Member01/common_uip0400_ComTopBG_Ms00", aqours);
        ui_.HideInstance(root_ + "/common_uil0101_ComBGGroup_Member01/common_uip0410_ComTopBG_Aqours00", !aqours);
        ui_.HideNode(root_ + "/common_uip0008_GroupTab00", "select_group00", aqours);
        ui_.HideNode(root_ + "/common_uip0008_GroupTab00", "select_group01", !aqours);
        ui_.SetInstanceText(root_ + "/common_uip0034_MemberInfo00", 0, messages_.Get(member::ProfileOf(Focused()).bio));
    }
    for (int i = 0; i < 20; ++i) {
        const bool shown = i >= First(mode_) && i < First(mode_) + Count(mode_);
        const std::string icon = Icon(top_, i);
        ui_.HideInstance(icon, !shown);
        Cell(ui_, icon, "character_icon01", i);
        const bool focused = shown && i - First(mode_) == focus_;
        ui_.HideNode(icon, "character_icon00_on", !focused);
        ui_.HideNode(icon, "character_icon00_eff", !focused);
        ui_.HideNode(icon, "character_icon00_off", focused);
    }
    ui_.HideNode(frame, "layout_ms00", mode_ != 0);
    ui_.HideNode(frame, "layout_aq00", mode_ != 1);
    // 591's box has no group switch (u08)
    for (const char* node : {"change_arrow00", "change_arrow01", "button_L100", "button_R100", "change_group_name00",
                             "change_group_name01"}) {
        ui_.HideNode(frame, node, !singers_.empty());
    }
    ui_.SetInstanceText(frame, 0, messages_.Get(590));  // "Choose a Member."
    ui_.SetInstanceText(frame, 1, "");
    if (stage_ != Stage::Profile) return;
    const int m = member::kAll[size_t(First(mode_) + focus_)];
    const member::Profile p = member::ProfileOf(m);
    const int group = member::Group(m);
    ui_.HideNode(card, "prof_bg00_ms", group != 0);
    ui_.HideNode(card, "prof_bg00_aq", group != 1);
    ui_.HideNode(card, "prof_bg00_ss", group != 2);
    char sd[64];
    std::snprintf(sd, sizeof(sd), "ui/ui_sd_character.pac/texture.pac/sd_character_menu%02d", m);
    if (!ui_.SetMaterialTexture(card, "sd_character00_M", sd)) LogWarn("member popup: no {}", sd);
    Cell(ui_, card, "member_mark00_M", member::Index(m));
    // the boxes (probed): 0 the question, 1 the name, 2 its reading (empty: the English game shows none), 3-8 the
    // labels, 9-14 the values
    const int labels[6] = {member::kSchoolYear, member::kBirthday, member::kBloodType, member::kFavourite, member::kHated,
                           member::kSubUnit};
    const int values[6] = {p.year, p.birthday, p.blood, p.favourite, p.hated, p.unit};
    ui_.SetInstanceText(card, 0, messages_.Get(513));  // "Continue with this member?"
    ui_.SetInstanceText(card, 1, messages_.Get(p.name));
    ui_.SetInstanceText(card, 2, "");
    for (int k = 0; k < 6; ++k) {
        ui_.SetInstanceText(card, 3 + k, messages_.Get(labels[k]));
        ui_.SetInstanceText(card, 9 + k, messages_.Get(values[k]));
    }
    ui_.SetMaterialUv(prof + "/uip0005_SelectButton00", "button_text00_M", 0, 0.375f);  // "OK" (as skill_screen.cpp)
}

void MemberPopup::StartSplash(int member) {
    if (!ui_.HasPart(kSplash)) return;
    char path[80];
    std::snprintf(path, sizeof(path), "ui/ui_transition_character.pac/texture.pac/com_load_mem00_%02d", member);
    if (!ui_.SetMaterialTexture(kSplash, "_charaTex_splash00", path)) {
        LogWarn("member popup: no splash {}", path);
        return;
    }
    ui_.Play(kSplash, UiScreen::ClipName(1), false);
    splash_ = kSplashSeconds;
    LogInfo("member popup: loading splash, member {}", member);
}

MemberPopup::Result MemberPopup::Key(SDL_Scancode key) {
    if (splash_ > 0) return Result::Open;  // the splash takes no keys
    if (stage_ == Stage::Closed) return Result::Closed;
    if (stage_ == Stage::Profile) {
        switch (key) {
            case SDL_SCANCODE_SPACE:
            case SDL_SCANCODE_RETURN:
                chosen_ = member::kAll[size_t(First(mode_) + focus_)];
                stage_ = Stage::Closed;
                LogInfo("member popup: chose member {}", chosen_);
                if (!first_) StartSplash(chosen_);  // shortcut: the first start's choice shows none (not seen in U2)
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
            if (first_) return Result::Open;  // the first member must be chosen
            stage_ = Stage::Closed;
            singers_.clear();
            LogInfo("member popup: closed");
            return Result::Closed;
        case SDL_SCANCODE_LEFT:
            do focus_ = (focus_ + n - 1) % n;
            while (!Choosable(focus_));
            break;
        case SDL_SCANCODE_RIGHT:
            do focus_ = (focus_ + 1) % n;
            while (!Choosable(focus_));
            break;
        // shortcut: rows of 4, then 5 (and Saint Snow's 2); up/down move by 4, as the first row
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_DOWN: {
            const int to = key == SDL_SCANCODE_UP ? std::max(0, focus_ - 4) : std::min(n - 1, focus_ + 4);
            if (Choosable(to)) focus_ = to;  // shortcut: a row move onto a member who cannot be chosen is ignored
            break;
        }
        case SDL_SCANCODE_PAGEDOWN:
        case SDL_SCANCODE_PAGEUP:
            if (singers_.empty() && (key == SDL_SCANCODE_PAGEDOWN) == (mode_ == 0)) mode_ = 1 - mode_, focus_ = 0;
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

void MemberPopup::Draw(Renderer& renderer) {
    if (splash_ > 0) {
        ui_.DrawPart(renderer, kSplash, 0, 0, float(std::min(1.0, splash_ / kSplashFade)));
        return;
    }
    if (stage_ == Stage::Closed) return;
    // the members who cannot be chosen: hidden in the layout, then drawn alone faded (the list only; the profile page
    // is over them)
    std::vector<std::string> faded;
    for (int k = 0; k < Count(mode_) && !singers_.empty(); ++k) {
        if (!Choosable(k)) faded.push_back(Icon(top_, First(mode_) + k));
    }
    for (const std::string& f : faded) ui_.HideInstance(f, true);
    ui_.DrawLayout(renderer, root_, 1.0f);
    for (const std::string& f : faded) {
        ui_.HideInstance(f, false);
        if (stage_ == Stage::List) ui_.DrawInstance(renderer, f, 0.35f);
    }
}

void MemberPopup::Release(Renderer& renderer) {
    if (loaded_) ui_.Release(renderer);
    loaded_ = false;
}

}
