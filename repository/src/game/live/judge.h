#pragma once
// Judgement, combo and score for one chart. Windows come from the game's own table
// s_EvaluateRangeDatas (live_params.bin): 4 groups of (early, late) rows of {Perfect, Great, Good, Bad} bounds in ms;
// beyond the Bad bound a note is a Miss (five grades, as on the game's result screen).
// Group 0 = presses, group 1 = hold releases (eboot 0x9FCF0/0x9FF80), groups 2-3 = the same +20 ms (skill
// "extendEvaluation", Hypothesis). Rules follow the eboot (analysis/202611010903-u5-live-rules/); gaps are marked.

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

// Per-chart scoring values from live_params.bin s_CommonParam (eboot 0xAA520, 0xA9D80, 0x9ECC0). Defaults = the
// game's values for a difficulty-index-0 chart.
struct ScoreRules {
    double multiplier = 1.0;  // +0xD0[s_live_datas +0x14 difficulty index]: 1.0 (ac_easy) .. 3.0 (ac_challenge, cs_master)
    double tick_rate = 0.033; // +0x128: a hold tick is worth this fraction of a note
    double tick_ms = 41;      // (int)(+0x130 x 1000) = (int)41.67
    // +0x138..+0x14C: clear-rate thresholds (score / maximum score) of the ranks above the lowest (eboot 0xAAC50);
    // the letters (C, B, A, S, then two above S) are a Hypothesis
    std::array<double, 6> rank_rates{0.45, 0.65, 0.825, 0.925, 1.025, 1.2};
    // +0x120: notes of the finale section score this many times their points (measured about x3 in shadPS4, G0;
    // Hypothesis: eboot check 0x93D00 is "finale active")
    double finale_multiplier = 3.0;
};

// Finale Rhythm Icons (analysis/202611072212-gameplay-feedback/): nine notes per play carry an icon (Note::icon); each
// one pressed better than Miss lights the next LOVELIVE! letter, and all nine by Chart::finale_ms start the finale.
constexpr int kFinaleIcons = 9;

struct Judgement {
    int note = -1;
    bool release = false;
    Grade grade = Grade::Miss;
    double offset_ms = 0;  // input time - note time (negative = early); 0 for missed notes
};

class Judge {
public:
    void Reset(const Chart& chart, const WindowSet& windows, const ScoreRules& rules = {});
    // Times are audio milliseconds (the master clock). Inputs outside every window are ignored.
    void Press(int lane, double t_ms);
    void Release(int lane, double t_ms);
    // Misses notes whose late Miss bound has passed and holds not released in time.
    void Update(double now_ms);
    bool Finished() const;
    // The score of an all-Perfect play: every judgement (a hold's press and release) and every hold tick at Perfect,
    // capped like the score (eboot 0xA94F0)
    double MaxScore() const;
    // score / MaxScore() (+1e-5, as eboot 0xAAC50) and the rank index: 0 below rank_rates[0], k above rank_rates[k-1]
    // capped at the top rank threshold (the game shows 120.0% for a finale score 1.48x the maximum, G0)
    double ClearRate() const;
    int Rank() const;
    bool Active(size_t note) const { return note < state_.size() && state_[note] != State::Done; }  // still drawn
    bool Held(size_t note) const { return note < state_.size() && state_[note] == State::Holding; }

    const std::vector<Judgement>& Log() const { return log_; }
    std::array<int, kGrades> counts{};
    int combo = 0, max_combo = 0;
    double score = 0;  // game points: judgements plus hold ticks (see judge.cpp)
    int icons = 0;       // icon notes pressed better than Miss. shortcut: a missed icon only leaves its letter unlit;
                         // whether it alone cancels the finale is unknown (G0 Q5)
    // The finale section is running: decided once, at Chart::finale_ms, from `icons`. Notes from that time on score
    // x finale_multiplier. MaxScore() stays without it (Hypothesis), so the clear rate can pass 1.0.
    bool finale = false;

private:
    enum class State { Pending, Holding, Done };
    void Record(int note, bool release, Grade grade, double offset);
    void AddPoints(size_t note, double note_fraction, Grade grade, int count = 1);
    void CheckFinale(double now);
    bool finale_checked_ = false;
    void Ticks(size_t note, double now);

    struct Hold { Grade press = Grade::Miss; double press_ms = 0; int ticks = 0, ticked = 0; };
    Grade Classify(const Windows& w, double dt) const;

    const Chart* chart_ = nullptr;
    WindowSet win_;
    ScoreRules rules_;
    std::vector<Hold> holds_;  // by note index
    std::vector<State> state_;
    std::array<int, kLanes> holding_{};  // note index held in each lane, or -1
    std::vector<Judgement> log_;
};

}
