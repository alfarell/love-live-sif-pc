#include "game/live/judge.h"

#include <cmath>

namespace sif::live {
namespace {

int32_t S32(const uint8_t* p) { return int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24); }

// Hypothesis (U5): Perfect and Great keep the combo; Good, Bad and Miss break it (as in the mobile game).
bool KeepsCombo(Grade g) { return g == Grade::Perfect || g == Grade::Great; }
// Hypothesis (U5): provisional score weights; the real formula (s_CommonParam floats, combo bonus) needs the eboot.
constexpr double kWeight[kGrades] = {1.0, 0.7, 0.2, 0.1, 0.0};
constexpr double kMaxScore = 1'000'000;

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
    size_t judgements = 0;
    for (const Note& n : chart.notes) judgements += n.IsHold() ? 2 : 1;
    note_value_ = judgements ? kMaxScore / double(judgements) : 0;
}

Grade Judge::Classify(const Windows& w, double dt) const {
    for (int g = 0; g < kBounds; ++g) {
        if (dt < 0 ? -dt <= w.early[size_t(g)] : dt <= w.late[size_t(g)]) return Grade(g);
    }
    return Grade::Miss;
}

void Judge::Record(int note, bool release, Grade grade, double offset) {
    log_.push_back({note, release, grade, offset});
    ++counts[size_t(grade)];
    score += note_value_ * kWeight[int(grade)];
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
        if (n.IsHold() && g != Grade::Miss) {
            state_[i] = State::Holding;
            holding_[size_t(lane)] = int(i);
        } else {
            state_[i] = State::Done;
            if (n.IsHold()) Record(int(i), true, Grade::Miss, 0);  // a missed hold start loses its release too
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
    // released before the release window opens: counts as a Miss (Hypothesis)
    Record(i, true, dt < -win_.release.EarlyLimit() ? Grade::Miss : Classify(win_.release, dt), dt);
}

void Judge::Update(double now) {
    if (!chart_) return;
    for (size_t i = 0; i < chart_->notes.size(); ++i) {
        const Note& n = chart_->notes[i];
        if (state_[i] == State::Pending && now - n.time_ms > win_.press.LateLimit()) {
            state_[i] = State::Done;
            Record(int(i), false, Grade::Miss, 0);
            if (n.IsHold()) Record(int(i), true, Grade::Miss, 0);
        } else if (state_[i] == State::Holding && now - n.end_ms > win_.release.LateLimit()) {
            state_[i] = State::Done;
            holding_[size_t(n.lane)] = -1;
            Record(int(i), true, Grade::Miss, 0);  // held too long (Hypothesis)
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
