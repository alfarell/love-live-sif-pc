#pragma once
// Screen changes: a screen fades in from black when it opens; when it is left it plays its parts' "out" clips and
// fades to black. shortcut: the game's own screen-change sequence (loading layouts uil0002/uil0003 in 00_common) is
// not traced and 0.3 s is a guess; replace once the eboot's screen manager is read.

#include <algorithm>
#include <cmath>

#include "engine/render/renderer.h"
#include "engine/ui/ui_screen.h"

namespace sif {

struct ScreenFade {
    static constexpr double kSeconds = 0.3;
    double t = 0;  // seconds since the current fade began
    bool leaving = false;

    void Leave(UiScreen& ui) {
        if (leaving) return;
        leaving = true;
        t = 0;
        ui.Play("*", UiScreen::ClipName(2), false);  // "out"
    }
    void Update(double seconds) { t += seconds; }
    // How much of the screen shows: 0 = black, 1 = fully visible
    float Alpha() const {
        const float k = float(std::min(1.0, t / kSeconds));
        return leaving ? 1 - k : k;
    }
    bool Left() const { return leaving && t >= kSeconds; }
    // Black over the whole finished frame (one layer, so overlapping parts do not show through each other); the
    // disc's radius is the window diagonal so it covers every corner
    void Draw(Renderer& r) const {
        const float black[4] = {0, 0, 0, 1 - Alpha()};
        const float w = float(r.Width()), h = float(r.Height());
        if (black[3] > 0) r.DrawCircle(w / 2, h / 2, std::hypot(w, h), 0, black);
    }
};

}
