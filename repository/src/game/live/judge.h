#pragma once
// Judgement, combo and score for one chart. Windows come from the game's own table
// s_EvaluateRangeDatas (live_params.bin): 4 groups of (early, late) rows of {Perfect, Great, Good, Bad} bounds in ms;
// beyond the Bad bound a note is a Miss (five grades, as on the game's result screen).
// Group 0 = presses, group 1 = hold releases (eboot 0x9FCF0/0x9FF80), groups 2-3 = the same +20 ms, used while a
// Judge Support skill is active (eboot 0xA0200, M11 S1). Rules follow the eboot (analysis/202611010903-u5-live-rules/,
// skills: analysis/202611151530-skills-and-cards/); gaps are marked.

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
    Windows press_wide, release_wide;  // groups 2-3: Judge Support, Judge Support +, Starting Combo Support
};

// Reads groups 0-1 (press, release) and 2-3 (the wide pair) from s_EvaluateRangeDatas (128 bytes = 8 rows x 4 x s32);
// a 64-byte table has no wide pair, which then equals groups 0-1.
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
    int difficulty = 0;       // s_live_datas +0x14: difficulty index 0-8
    // +0x00: note travel time in ms (mic to ring) at Rhythm Icon Speed Slow .. Fast, the row of `difficulty` in a
    // 9 x 5 table (analysis/202611101907-checklist-feedback/)
    std::array<double, 5> approach_ms{2300, 2100, 1900, 1650, 1300};
};

// Finale Rhythm Icons (analysis/202611072212-gameplay-feedback/): nine notes per play carry an icon (Note::icon); each
// one pressed better than Miss lights the next LOVELIVE! letter, and all nine by Chart::finale_ms start the finale.
constexpr int kFinaleIcons = 9;

// A live skill: one of the played member's three set skill cards (M11 S5; the eboot's rules are in analysis
// 202611151530-skills-and-cards/202611151548). A timed skill fires when its skill note (Note::skill == slot) is hit
// and lasts duration_ms from the hit; an untimed one is armed at the live start with `amount` uses.
struct Skill {
    int slot = 0;            // 0-2: the phase whose skill note fires it
    int card = 0;            // skill card id (logs, HUD)
    int kind = -1;           // card::Kind: 0 Judge Plus, 1 Combo Support, 13 Judge Support, 14 Finale Support, ...
    double duration_ms = 0;  // level row seconds x 1000; 0 = untimed
    int amount = 0;          // untimed: Finale Support icons, Starting Combo Support combo breaks
};
// A skill taking effect: a timed one fired by its note (until = hit + duration), or one use of an untimed one.
struct SkillEvent {
    int skill = 0;  // index in Judge::Skills()
    double t_ms = 0, until_ms = 0;
};

struct Judgement {
    int note = -1;
    bool release = false;
    Grade grade = Grade::Miss;
    double offset_ms = 0;  // input time - note time (negative = early); 0 for missed notes
};

class Judge {
public:
    void Reset(const Chart& chart, const WindowSet& windows, const ScoreRules& rules = {}, std::vector<Skill> skills = {});
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
    const std::vector<Skill>& Skills() const { return skills_; }
    const std::vector<SkillEvent>& SkillLog() const { return skill_log_; }
    // A skill of `kind` is in effect at t: a timed one from its hit to its end, an untimed one while uses remain
    bool SkillActive(int kind, double t) const;
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
    void Record(int note, bool release, Grade grade, double offset, double t);
    Grade Judged(const Windows& normal, const Windows& wide, double dt, double t) const;
    void EndHold(size_t note, Grade grade, double offset, double t);
    void Use(int kind, double t);  // one use of an untimed skill
    void AddPoints(size_t note, double note_fraction, Grade grade, int count = 1);
    void CheckFinale(double now);
    bool finale_checked_ = false;
    void Ticks(size_t note, double now);

    // auto_end: pressed under Long Notes Support, the end is judged Perfect without a release (eboot 0x9ECC0 +0x42)
    struct Hold { Grade press = Grade::Miss; double press_ms = 0; int ticks = 0, ticked = 0; bool auto_end = false; };
    Grade Classify(const Windows& w, double dt) const;

    const Chart* chart_ = nullptr;
    WindowSet win_;
    ScoreRules rules_;
    std::vector<Hold> holds_;  // by note index
    std::vector<State> state_;
    std::array<int, kLanes> holding_{};  // note index held in each lane, or -1
    std::vector<Judgement> log_;
    std::vector<Skill> skills_;
    std::vector<double> until_;  // per skill: the end of a fired timed skill (ms), -1 before it fires
    std::vector<int> uses_;      // per skill: untimed uses left
    std::vector<SkillEvent> skill_log_;
};

}
