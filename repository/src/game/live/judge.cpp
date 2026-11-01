#include "game/live/judge.h"

#include <algorithm>
#include <cmath>

namespace sif::live {
namespace {

int32_t S32(const uint8_t* p) { return int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24); }

// Perfect and Great keep the combo; Good, Bad and Miss break it (eboot 0xAA3B0; Good keeps it only under the
// "combo softening" skills, not implemented).
bool KeepsCombo(Grade g) { return g == Grade::Perfect || g == Grade::Great; }
// points = round(100 x weight[grade] x combo multiplier), capped at 9,999,999 (eboot 0xAA520). Weights and base 100
// are s_CommonParam +0xC0..+0xCC and +0xB4 in live_params.bin.
constexpr double kWeight[kGrades] = {1.0, 0.7, 0.2, 0.1, 0.0};
constexpr double kBasePoints = 100;
constexpr double kMaxScore = 9'999'999;
// shortcut: combo multiplier fixed at s_CommonParam +0xD0[0] = 1.0; the level that indexes it (+0xD0..+0xF0) and
// hold-tick points (10 per tick) are not traced yet, upgrade when U5 continues
constexpr double kComboMultiplier = 1.0;

}

const char* GradeName(Grade g) {
    static const char* names[] = {"Perfect", "Great", "Good", "Bad", "Miss"};
    return names[int(g)];
}

bool WindowsFromTable(std::span<const uint8_t> t, WindowSet& out, std::string* error) {
    if (t.size() < 64) {
        if (error) *error = "s_EvaluateRangeDatas too short";
        return false;
    }
    // row 2g: early side, stored {Bad, Good, Great, Perfect}; row 2g+1: late side, stored {Perfect, Great, Good, Bad}
    const auto group = [&](int g, Windows& w) {
        for (int i = 0; i < kBounds; ++i) {
            w.early[size_t(i)] = S32(&t[size_t(g) * 32 + size_t(3 - i) * 4]);
            w.late[size_t(i)] = S32(&t[size_t(g) * 32 + 16 + size_t(i) * 4]);
        }
    };
    group(0, out.press);
    group(1, out.release);
    return true;
}

void Judge::Reset(const Chart& chart, const WindowSet& windows) {
    chart_ = &chart;
    win_ = windows;
    state_.assign(chart.notes.size(), State::Pending);
    holding_.fill(-1);
    log_.clear();
    counts = {};
    combo = max_combo = 0;
    score = 0;
}

Grade Judge::Classify(const Windows& w, double dt) const {
    // eboot 0xA0070: inner bounds are strict (|dt| == 40 is Great), the Bad bound is inclusive, beyond it a Miss
    const double a = std::abs(dt);
    const auto& b = dt < 0 ? w.early : w.late;
    for (int g = 0; g < kBounds - 1; ++g) {
        if (a < b[size_t(g)]) return Grade(g);
    }
    return a <= b[kBounds - 1] ? Grade::Bad : Grade::Miss;
}

void Judge::Record(int note, bool release, Grade grade, double offset) {
    log_.push_back({note, release, grade, offset});
    ++counts[size_t(grade)];
    score = std::min(kMaxScore, score + std::round(kBasePoints * kWeight[int(grade)] * kComboMultiplier));
    combo = KeepsCombo(grade) ? combo + 1 : 0;
    if (combo > max_combo) max_combo = combo;
}

void Judge::Press(int lane, double t) {
    if (!chart_ || lane < 0 || lane >= kLanes || holding_[size_t(lane)] >= 0) return;
    // the earliest pending note of the lane that is inside its press window
    for (size_t i = 0; i < chart_->notes.size(); ++i) {
        const Note& n = chart_->notes[i];
        if (n.lane != lane || state_[i] != State::Pending) continue;
        const double dt = t - n.time_ms;
        if (dt < -win_.press.EarlyLimit()) return;  // too early for this lane's next note: ignored
        if (dt > win_.press.LateLimit()) continue;  // already late (Update will miss it)
        const Grade g = Classify(win_.press, dt);
        Record(int(i), false, g, dt);
        // eboot 0x9ECC0: only a Good-or-better press starts holding; a Bad press ends the hold without a release
        if (n.IsHold() && g <= Grade::Good) {
            state_[i] = State::Holding;
            holding_[size_t(lane)] = int(i);
        } else {
            state_[i] = State::Done;
        }
        return;
    }
}

void Judge::Release(int lane, double t) {
    if (!chart_ || lane < 0 || lane >= kLanes) return;
    const int i = holding_[size_t(lane)];
    if (i < 0) return;
    holding_[size_t(lane)] = -1;
    state_[size_t(i)] = State::Done;
    const double dt = t - chart_->notes[size_t(i)].end_ms;
    // released before the release window opens: Bad (eboot 0xA0070, release path)
    Record(i, true, dt < -win_.release.EarlyLimit() ? Grade::Bad : Classify(win_.release, dt), dt);
}

void Judge::Update(double now) {
    if (!chart_) return;
    for (size_t i = 0; i < chart_->notes.size(); ++i) {
        const Note& n = chart_->notes[i];
        if (state_[i] == State::Pending && now - n.time_ms > win_.press.LateLimit()) {
            state_[i] = State::Done;
            Record(int(i), false, Grade::Miss, 0);  // an unpressed hold is one Miss (eboot 0x9FDF0 -> 0x9ECC0)
        } else if (state_[i] == State::Holding && now - n.end_ms > win_.release.LateLimit()) {
            state_[i] = State::Done;
            holding_[size_t(n.lane)] = -1;
            Record(int(i), true, Grade::Miss, 0);  // held past the release Bad bound (eboot 0x9FDF0, state 2)
        }
    }
}

bool Judge::Finished() const {
    for (State s : state_) {
        if (s != State::Done) return false;
    }
    return true;
}

}
