#include "app/popup.h"

#include <algorithm>

#include "engine/core/log.h"

namespace sif {
namespace {

constexpr const char* kTops[4] = {"uil0050_ComPopTop00", "uil0052_ComPopTop01", "uil0054_ComPopTop02", "uil0056_ComPopTop03"};
constexpr const char* kPops[4] = {"uil0051_ComPop00", "uil0053_ComPop01", "uil0055_ComPop02", "uil0057_ComPop03"};
constexpr const char* kTypes[4] = {"uip1010_ComPopType00", "uip1011_ComPopType01", "uip1012_ComPopType02", "uip1010_ComPopType00"};

}

std::string Popup::Top() const { return kTops[kind_]; }

bool Popup::Load(Renderer& renderer, const Vfs& vfs, const std::string& lang) {
    if (loaded_) return true;
    std::string error;
    if (!text_.Load(vfs, lang, &error)) LogWarn("popup: text: {}", error);
    if (!ui_.Load(renderer, vfs, "ui2/00_common", lang, &error, &text_)) {
        LogError("popup: {}", error);
        return false;
    }
    loaded_ = true;
    return true;
}

void Popup::OpenMessage(int title, const std::string& text, const std::vector<int>& buttons, int focus) {
    kind_ = buttons.size() >= 2 ? 0 : 3;
    title_ = title, message_ = text, buttons_ = buttons, rows_.clear();
    focus_ = std::clamp(focus, 0, std::max(0, int(buttons.size()) - 1));
    open_ = true;
    Apply();
    ui_.Play("*", UiScreen::ClipName(1), false);  // "in"
}

void Popup::OpenChoices(int title, const std::vector<std::string>& rows, int focus) {
    kind_ = rows.size() >= 4 ? 2 : 1;
    title_ = title, message_.clear(), buttons_.clear(), rows_ = rows;
    focus_ = std::clamp(focus, 0, std::max(0, int(rows.size()) - 1));
    open_ = true;
    Apply();
    ui_.Play("*", UiScreen::ClipName(1), false);
}

void Popup::Apply() {
    const std::string pop = Top() + "/" + kPops[kind_];
    ui_.SetMaterialUv(pop + "/uip1001_ComPopBG00", "title_text00_M", 0, float(title_) / 16.0f);
    const std::string type = pop + "/" + kTypes[kind_];
    if (kind_ == 0 || kind_ == 3) {
        ui_.SetInstanceText(type, 0, message_);
        for (size_t b = 0; b < buttons_.size(); ++b) {
            const std::string button = pop + (kind_ == 3 ? std::string("/uip0005_SelectButton00")
                                                         : "/uip0005_SelectButton00_0" + std::to_string(b));
            ui_.SetMaterialUv(button, "button_text00_M", float(buttons_[b] % 2) * 0.5f, float(buttons_[b] / 2) * 0.125f);
            const bool on = int(b) == focus_;
            ui_.HideNode(button, "button_bg00_on", !on);
            ui_.HideNode(button, "button_bg00_off", on);
            ui_.HideNode(button, "button_effect00", !on);
        }
    } else {
        const int n = kind_ == 2 ? 4 : 3;
        for (int r = 0; r < n; ++r) {
            // the rows' ui_text style (60) is not in font_param.bin: the message's font (style 28), left-aligned to the
            // arrow, the focused row orange (shadPS4 work/m16/shad/e1; shortcut: the colour is read off that picture)
            TextStyle style = text_.Style(28) ? *text_.Style(28) : TextStyle{};
            style.halign = 0;
            if (r == focus_) style.color = 0xFFFF6A00;
            ui_.SetInstanceTextStyle(type, r, style);
            ui_.SetInstanceText(type, r, size_t(r) < rows_.size() ? rows_[size_t(r)] : "");
            ui_.HideNode(type, "select_arrow0" + std::to_string(r), r != focus_);
        }
    }
}

int Popup::Key(SDL_Scancode key) {
    if (!open_) return kCancelled;
    const bool rows = kind_ == 1 || kind_ == 2;
    const int n = rows ? int(rows_.size()) : int(buttons_.size());
    switch (key) {
        case SDL_SCANCODE_LEFT:
            if (!rows) focus_ = (focus_ + n - 1) % std::max(1, n);
            break;
        case SDL_SCANCODE_RIGHT:
            if (!rows) focus_ = (focus_ + 1) % std::max(1, n);
            break;
        case SDL_SCANCODE_UP:
            if (rows) focus_ = (focus_ + n - 1) % std::max(1, n);
            break;
        case SDL_SCANCODE_DOWN:
            if (rows) focus_ = (focus_ + 1) % std::max(1, n);
            break;
        case SDL_SCANCODE_SPACE:
        case SDL_SCANCODE_RETURN:
            open_ = false;
            return focus_;
        case SDL_SCANCODE_ESCAPE:
            open_ = false;
            return kCancelled;
        default: break;
    }
    Apply();
    return kOpen;
}

void Popup::Draw(Renderer& renderer) const {
    if (open_) ui_.DrawLayout(renderer, Top(), 1.0f, 960, 540);
}

}
