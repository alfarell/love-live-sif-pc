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

}

bool ParseChart(std::span<const uint8_t> s, Chart& out, std::string* error) {
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
    return true;
}

std::string ChartPrefix(int song_id, int variant) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "score_%04d_%02d_", song_id, variant);
    return buf;
}

}
