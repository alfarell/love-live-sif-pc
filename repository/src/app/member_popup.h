#pragma once
// R2 "Switch Members" on the top menu (M13 N5): the game's member popup (ui2/00_common uil0010_PopMemberTop00): the
// group's members ("Choose a Member.", L1/R1 to the other group), then the chosen member's profile with "Continue
// with this member?" (shadPS4 work/m13/shad/r2a.png, r2c.png, r2aq.png).

#include <SDL3/SDL.h>

#include <string>

#include "engine/ui/ui_screen.h"
#include "game/text/messages.h"

namespace sif {

class MemberPopup {
public:
    enum class Result { Open, Closed, Chosen };
    // Opens on `mode`'s list (0 µ's, 1 Aqours) with `current` focused; loads the popup once.
    bool Open(Renderer& renderer, const Vfs& vfs, const std::string& lang, int mode, int current);
    // The first start's full-screen "Choose a Member" (M17 T3; ui2/01_title uil0051_selectMember00, shadPS4
    // work/m17/shad/f06-f09): the same popup with the group's header and background, "You will be able to change
    // members later." (697), the focused member's description and the help line (489). Starts in µ's mode on Honoka;
    // Esc does not close it. The caller draws the 3D member on the left (Focused()).
    bool OpenFirst(Renderer& renderer, const Vfs& vfs, const std::string& lang);
    bool IsOpen() const { return stage_ != Stage::Closed; }
    // One key press: arrows move, Page Up / Page Down (L1/R1) change the group, Space / Return choose, Esc goes back.
    Result Key(SDL_Scancode key);
    int Chosen() const { return chosen_; }
    int Focused() const;  // the member under the cursor
    void Update(double seconds) { ui_.Update(seconds); }
    void Draw(Renderer& renderer) const;
    // The first-start form: the help line again, over the 3D member
    void DrawTicker(Renderer& renderer) const;
    void Release(Renderer& renderer);

private:
    enum class Stage { Closed, List, Profile };
    bool Load(Renderer& renderer, const Vfs& vfs, const std::string& lang, const std::string& screen);
    void Apply();

    bool loaded_ = false;
    bool first_ = false;
    std::string root_;  // the layout drawn; the popup is its instance top_
    std::string top_;
    UiText text_;
    UiScreen ui_;
    Messages messages_;
    Stage stage_ = Stage::Closed;
    int mode_ = 0, focus_ = 0, chosen_ = 0;
};

}
