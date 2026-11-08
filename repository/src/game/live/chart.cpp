#include "game/live/chart.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace sif::live {
namespace {

uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

struct Event {
    uint32_t kind, tick, a, b, c, d;
};

// Every note-on event as (note number, audio ms) goes to `raw` when given (channel 5 uses numbers 60-69).
bool ParseScor(std::span<const uint8_t> s, Chart& out, std::vector<std::pair<int, double>>* raw, std::string* error) {
    out = {};
    if (s.size() < 0x40 || std::memcmp(s.data(), "Scor", 4) != 0) {
        if (error) *error = "not a Scor chart";
        return false;
    }
    const uint32_t count = U32(&s[8]), first_tempo = U32(&s[0x0C]), ppq = U32(&s[0x10]);
    if (s.size() < 0x40 + uint64_t(count) * 32 || ppq == 0 || first_tempo == 0) {
        if (error) *error = "bad Scor header";
        return false;
    }
    out.lead_in_ms = U32(&s[0x14]);
    out.first_bpm = 60e6 / first_tempo;
    std::vector<Event> events;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* e = &s[0x40 + size_t(i) * 32];
        events.push_back({uint32_t(e[2] | e[3] << 8), U32(e + 4), U32(e + 8), U32(e + 12), U32(e + 16), U32(e + 20)});
    }
    std::stable_sort(events.begin(), events.end(), [](const Event& x, const Event& y) {
        return x.tick != y.tick ? x.tick < y.tick : (x.kind == 2) > (y.kind == 2);  // tempo first at equal ticks
    });
    // tempo map: ms at each tempo change, used for note starts and hold ends
    struct Tempo { uint32_t tick; double ms; double us_per_q; };
    std::vector<Tempo> map{{0, 0.0, double(first_tempo)}};
    for (const Event& e : events) {
        if (e.kind != 2 || e.a == 0) continue;
        const Tempo& last = map.back();
        map.push_back({e.tick, last.ms + (e.tick - last.tick) * last.us_per_q / ppq / 1000.0, double(e.a)});
    }
    const auto to_ms = [&](uint32_t tick) {
        const Tempo* t = &map.front();
        for (const Tempo& m : map) {
            if (m.tick <= tick) t = &m;
        }
        return t->ms + (tick - t->tick) * t->us_per_q / ppq / 1000.0;
    };
    for (const Event& e : events) {
        if (e.kind != 0) continue;
        if (raw) raw->push_back({int(e.a), to_ms(e.tick) + out.lead_in_ms});
        const int lane = int(e.a) - kFirstNoteNumber;
        if (lane < 0 || lane >= kLanes) {
            ++out.skipped;
            continue;
        }
        Note n;
        n.lane = lane;
        n.time_ms = to_ms(e.tick) + out.lead_in_ms;
        // shortcut: taps are 120 ticks (a 1/16 note) in all ac charts; anything longer is treated as a hold
        if (e.c > 120) n.end_ms = to_ms(e.tick + e.c) + out.lead_in_ms;
        out.notes.push_back(n);
    }
    GroupNotes(out);
    return true;
}

}

bool ParseChart(std::span<const uint8_t> s, Chart& out, std::string* error) { return ParseScor(s, out, nullptr, error); }

bool ParseTimeline(std::span<const uint8_t> s, Chart& chart, std::string* error) {
    Chart timeline;
    std::vector<std::pair<int, double>> raw;
    if (!ParseScor(s, timeline, &raw, error)) return false;
    for (const auto& [number, ms] : raw) {
        if (number >= 60 && number <= 68) {
            double& p = chart.phase_ms[size_t((number - 60) % 3)];
            if (p < 0 || ms < p) p = ms;
        } else if (number == 69) {
            chart.finale_ms = ms;
        }
    }
    return true;
}

void MarkIcons(Chart& chart) {
    std::stable_sort(chart.notes.begin(), chart.notes.end(), [](const Note& a, const Note& b) { return a.time_ms < b.time_ms; });
    // shortcut: the icon channel per phase (2, 4, 3) is what song 0340 showed in shadPS4 (G0); the game's rule
    // (eboot 0xABAC0) is not traced
    constexpr int kIconChannel[3] = {2, 4, 3};
    for (Note& n : chart.notes) {
        int phase = -1;
        for (int k = 0; k < 3; ++k) {
            if (chart.phase_ms[size_t(k)] >= 0 && n.time_ms >= chart.phase_ms[size_t(k)]) phase = k;
        }
        n.icon = phase >= 0 && n.channel == kIconChannel[phase];
    }
    GroupNotes(chart);
}

void GroupNotes(Chart& chart) {
    struct Event {
        double t;
        bool tail;
        size_t note;
    };
    std::vector<Event> heads, tails;
    for (size_t i = 0; i < chart.notes.size(); ++i) {
        Note& n = chart.notes[i];
        n.group = n.end_group = -1;
        heads.push_back({n.time_ms, false, i});
        if (n.IsHold()) tails.push_back({n.end_ms, true, i});
    }
    const auto by_time = [](const Event& a, const Event& b) { return a.t < b.t; };
    std::stable_sort(heads.begin(), heads.end(), by_time);
    std::stable_sort(tails.begin(), tails.end(), by_time);
    // chains of 2+ events of one kind, each 1 ms or less after the previous: [first, last) into its list
    struct Span {
        double t;
        bool tail;
        const std::vector<Event>* list;
        size_t first, last;
    };
    std::vector<Span> spans;
    for (const std::vector<Event>* list : {&heads, &tails}) {
        const auto& e = *list;
        for (size_t i = 0; i < e.size();) {
            size_t j = i + 1;
            while (j < e.size() && e[j].t - e[j - 1].t <= 1) ++j;
            if (j - i > 1) spans.push_back({e[i].t, e[i].tail, list, i, j});
            i = j;
        }
    }
    std::stable_sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) {
        return a.t != b.t ? a.t < b.t : !a.tail && b.tail;
    });
    for (size_t g = 0; g < spans.size(); ++g) {
        for (size_t k = spans[g].first; k < spans[g].last; ++k) {
            const Event& e = (*spans[g].list)[k];
            (e.tail ? chart.notes[e.note].end_group : chart.notes[e.note].group) = int(g);
        }
    }
}

std::string ChartPrefix(int song_id, int variant) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "score_%04d_%02d_", song_id, variant);
    return buf;
}

}
