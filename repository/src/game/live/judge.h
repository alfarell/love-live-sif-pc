#pragma once
// Judgement, combo and a provisional score for one chart. Windows come from the game's own table
// s_EvaluateRangeDatas (live_params.bin): 4 groups of (early, late) rows of {Perfect, Great, Good, Bad} bounds in ms;
// beyond the Bad bound a note is a Miss (five grades, as on the game's result screen).
// Group 0 = presses, group 1 = hold releases (Hypothesis), groups 2-3 = the same +20 ms (skill "extendEvaluation").
// Rules not yet recovered from the eboot (U5) are marked Hypothesis and kept in one place here.

#include <array>
#include <span>
#include <string>
#include <vector>

#include "game/live/chart.h"

namespace sif::live {

enum class Grade { Perfect = 0, Great, Good, Bad, Miss };
constexpr int kGrades = 5;
constexpr int kBounds = 4;  // Perfect, Great, Good, Bad; a hit outside them all is ignored, a note never hit is a Miss
const char* GradeName(Grade g);

struct Windows {
    std::array<double, kBounds> early{};  // |dt| bounds before the note, by grade
    std::array<double, kBounds> late{};   // bounds after the note
    double EarlyLimit() const { return early[kBounds - 1]; }
    double LateLimit() const { return late[kBounds - 1]; }
};

struct WindowSet {
    Windows press, release;
};

// Reads groups 0 (press) and 1 (release) from s_EvaluateRangeDatas (128 bytes = 8 rows x 4 x s32).
bool WindowsFromTable(std::span<const uint8_t> evaluate_range_datas, WindowSet& out, std::string* error = nullptr);

struct Judgement {
    int note = -1;
    bool release = false;
    Grade grade = Grade::Miss;
    double offset_ms = 0;  // input time - note time (negative = early); 0 for missed notes
};

class Judge {
public:
    void Reset(const Chart& chart, const WindowSet& windows);
    // Times are audio milliseconds (the master clock). Inputs outside every window are ignored.
    void Press(int lane, double t_ms);
    void Release(int lane, double t_ms);
    // Misses notes whose late Miss bound has passed and holds not released in time.
    void Update(double now_ms);
    bool Finished() const;
    bool Active(size_t note) const { return note < state_.size() && state_[note] != State::Done; }  // still drawn
    bool Held(size_t note) const { return note < state_.size() && state_[note] == State::Holding; }

    const std::vector<Judgement>& Log() const { return log_; }
    std::array<int, kGrades> counts{};
    int combo = 0, max_combo = 0;
    double score = 0;  // provisional (see judge.cpp)

private:
    enum class State { Pending, Holding, Done };
    void Record(int note, bool release, Grade grade, double offset);
    Grade Classify(const Windows& w, double dt) const;

    const Chart* chart_ = nullptr;
    WindowSet win_;
    std::vector<State> state_;
    std::array<int, kLanes> holding_{};  // note index held in each lane, or -1
    double note_value_ = 0;
    std::vector<Judgement> log_;
};

}
