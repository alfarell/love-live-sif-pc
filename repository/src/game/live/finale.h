#pragma once
// Finale Rhythm Icons (analysis/202611072212-gameplay-feedback/202611081816-analysis.md): nine notes per play carry
// an icon (Note::icon); each one hit lights the next letter of LOVELIVE! in the HUD, and all nine open the finale
// section at Chart::finale_ms (M7.5 G6).

#include "game/live/chart.h"
#include "game/live/judge.h"

namespace sif::live {

constexpr int kFinaleIcons = 9;

// Icon notes pressed with a grade better than Miss so far. shortcut: whether a missed icon cancels the finale is not
// known (G0 Q5); a miss only leaves its letter unlit here.
inline int CollectedIcons(const Chart& chart, const Judge& judge) {
    int n = 0;
    for (const Judgement& j : judge.Log()) {
        if (!j.release && j.note >= 0 && size_t(j.note) < chart.notes.size() && chart.notes[size_t(j.note)].icon &&
            j.grade != Grade::Miss) {
            ++n;
        }
    }
    return n;
}

}
