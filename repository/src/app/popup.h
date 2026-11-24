#pragma once
// The game's common popups (ui2/00_common, M16 R5): a title from the pop_title_text00_00_T atlas over either a message
// with one or two buttons (uil0050_ComPopTop00 / uil0056_ComPopTop03) or three or four choice rows with an arrow
// (uil0052_ComPopTop01 / uil0054_ComPopTop02). The layouts' origin is the canvas's top-left corner; the popup is drawn
// at the centre. Seen in shadPS4: "Confirmation of medley start" (Back / OK), "Confirm" "Discard edits and return to
// list?" (Cancel / OK), "What will you do next?" (three rows) (analysis/202611231413-live-show-medley/).

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "engine/ui/ui_screen.h"
#include "engine/ui/ui_text.h"

namespace sif {

class Popup {
public:
    // Rows of pop_title_text00_00_T (16 rows of 64 px)
    enum Title { kConfirm = 0, kWhatNext = 1, kMedleyStart = 5, kNotice = 10 };
    // Cells of com_popup_button_text00_T (2 columns x 64 px rows)
    enum Button { kNo = 0, kYes = 1, kCancel = 2, kBack = 4, kOk = 6 };
    enum Result { kOpen = -1, kCancelled = -2 };

    bool Load(Renderer& renderer, const Vfs& vfs, const std::string& lang);
    // A message with one or two buttons; `focus` is the button focused first. Choose() then returns the button index.
    void OpenMessage(int title, const std::string& text, const std::vector<int>& buttons, int focus);
    // Three or four choice rows; Choose() then returns the row.
    void OpenChoices(int title, const std::vector<std::string>& rows, int focus = 0);
    bool IsOpen() const { return open_; }
    // One key: Left/Right (buttons) or Up/Down (rows) move, Space / Return choose, Esc cancels. Returns kOpen while the
    // popup stays open, the chosen index, or kCancelled; it closes on a choice or a cancel.
    int Key(SDL_Scancode key);
    void Update(double seconds) { ui_.Update(seconds); }
    void Draw(Renderer& renderer) const;
    void Release(Renderer& renderer) { ui_.Release(renderer); }

private:
    void Apply();
    std::string Top() const;

    bool loaded_ = false;
    UiText text_;
    UiScreen ui_;
    bool open_ = false;
    int kind_ = 0;  // 0 message + 2 buttons, 1 three rows, 2 four rows, 3 message + 1 button
    int title_ = 0;
    std::string message_;
    std::vector<int> buttons_;
    std::vector<std::string> rows_;
    int focus_ = 0;
};

}
