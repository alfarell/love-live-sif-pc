#include "game/live/judge.h"

#include <algorithm>
#include <cmath>

namespace sif::live {
namespace {

int32_t S32(const uint8_t* p) { return int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24); }

// Perfect and Great keep the combo; Good, Bad and Miss break it (eboot 0xAA3B0; Good keeps it only under the
// "combo softening" skills, not implemented).
bool KeepsCombo(Grade g) { return g == Grade::Perfect || g == Grade::Great; }
// points = round(100 x weight[grade] x difficulty multiplier [x tick rate]) per judgement or hold tick, capped at
// 9,999,999 (eboot 0xAA520, 0xA9D80). Weights and base 100 are s_CommonParam +0xC0..+0xCC and +0xB4.
constexpr double kWeight[kGrades] = {1.0, 0.7, 0.2, 0.1, 0.0};
constexpr double kBasePoints = 100;
constexpr double kMaxScore = 9'999'999;

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

void Judge::Reset(const Chart& chart, const WindowSet& windows, const ScoreRules& rules) {
    chart_ = &chart;
    win_ = windows;
    rules_ = rules;
    state_.assign(chart.notes.size(), State::Pending);
    holds_.assign(chart.notes.size(), {});
    holding_.fill(-1);
    log_.clear();
    counts = {};
    combo = max_combo = 0;
    score = 0;
    icons = 0;
    finale = finale_checked_ = false;
}

void Judge::CheckFinale(double now) {
    if (finale_checked_ || chart_->finale_ms < 0 || now < chart_->finale_ms) return;
    finale_checked_ = true;
    finale = icons >= kFinaleIcons;
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
    if (!release && grade != Grade::Miss && chart_->notes[size_t(note)].icon) ++icons;
    AddPoints(size_t(note), 1.0, grade);
    combo = KeepsCombo(grade) ? combo + 1 : 0;
    if (combo > max_combo) max_combo = combo;
}

void Judge::AddPoints(size_t note, double note_fraction, Grade grade, int count) {
    const double finale_factor = finale && chart_->notes[note].time_ms >= chart_->finale_ms ? rules_.finale_multiplier : 1.0;
    // std::round is half away from zero, like the eboot's sign/floor(|x| + 0.5)
    const double each = std::round(rules_.multiplier * finale_factor * note_fraction * kWeight[int(grade)] * kBasePoints);
    score = std::min(kMaxScore, score + each * count);
}

void Judge::Ticks(size_t i, double now) {
    // eboot 0x9ECC0, held state: one tick each time (now - press) passes another tick length, until the note's end
    // (clamped here, so a late update still gets the ticks a per-frame update would have given)
    Hold& h = holds_[i];
    now = std::min(now, chart_->notes[i].end_ms);
    while (h.ticked < h.ticks && now - h.press_ms - (h.ticked + 1) * rules_.tick_ms > 0) {
        ++h.ticked;
        AddPoints(i, rules_.tick_rate, h.press);  // ticks score with the press grade; no combo or count change
    }
}

void Judge::Press(int lane, double t) {
    if (!chart_ || lane < 0 || lane >= kLanes || holding_[size_t(lane)] >= 0) return;
    CheckFinale(t);
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
            holds_[i] = {g, t, int(std::round((n.end_ms - n.time_ms) / rules_.tick_ms)), 0};
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
    CheckFinale(t);
    Ticks(size_t(i), t);
    holding_[size_t(lane)] = -1;
    state_[size_t(i)] = State::Done;
    const double dt = t - chart_->notes[size_t(i)].end_ms;
    // released before the release window opens: Bad (eboot 0xA0070, release path)
    const Grade g = dt < -win_.release.EarlyLimit() ? Grade::Bad : Classify(win_.release, dt);
    Record(i, true, g, dt);
    // a Perfect or Great release awards the ticks not yet given, at the release grade (eboot 0x9ECC0)
    Hold& h = holds_[size_t(i)];
    if (g <= Grade::Great) AddPoints(size_t(i), rules_.tick_rate, g, h.ticks - h.ticked);
}

void Judge::Update(double now) {
    if (!chart_) return;
    CheckFinale(now);
    for (size_t i = 0; i < chart_->notes.size(); ++i) {
        const Note& n = chart_->notes[i];
        if (state_[i] == State::Pending && now - n.time_ms > win_.press.LateLimit()) {
            state_[i] = State::Done;
            Record(int(i), false, Grade::Miss, 0);  // an unpressed hold is one Miss (eboot 0x9FDF0 -> 0x9ECC0)
        } else if (state_[i] == State::Holding) {
            Ticks(i, now);
            if (now - n.end_ms <= win_.release.LateLimit()) continue;
            state_[i] = State::Done;
            holding_[size_t(n.lane)] = -1;
            Record(int(i), true, Grade::Miss, 0);  // held past the release Bad bound (eboot 0x9FDF0, state 2)
        }
    }
}

double Judge::MaxScore() const {
    if (!chart_) return 0;
    const double note = std::round(rules_.multiplier * kBasePoints);
    const double tick = std::round(rules_.multiplier * rules_.tick_rate * kBasePoints);
    double total = 0;
    for (const Note& n : chart_->notes) {
        total += note;
        if (n.IsHold()) total += note + tick * std::round((n.end_ms - n.time_ms) / rules_.tick_ms);
    }
    return std::min(kMaxScore, total);
}

double Judge::ClearRate() const {
    const double max = MaxScore();
    return max > 0 ? std::min(score / max + 1e-5, rules_.rank_rates.back()) : 0;
}

int Judge::Rank() const {
    const double rate = ClearRate();
    int rank = 0;
    while (rank < int(rules_.rank_rates.size()) && rate >= rules_.rank_rates[size_t(rank)]) ++rank;
    return rank;
}

bool Judge::Finished() const {
    for (State s : state_) {
        if (s != State::Done) return false;
    }
    return true;
}

}
