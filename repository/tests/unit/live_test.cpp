// Headless autoplay test for the live (song play) rules. Synthetic: window edges, combo, holds. With the CUSA24620
// dump folder as the first argument: song 0010 (ac_challenge chart) autoplayed at fixed input offsets.
#include <cmath>
#include <cstdio>
#include <string>

#include "game/live/chart.h"
#include "game/live/judge.h"
#include "game/live/song.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"

using namespace sif::live;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", what.c_str());
    }
}

// Presses every note `offset` ms late (negative = early) and releases holds `release_offset` ms late.
static Judge Autoplay(const Chart& chart, const WindowSet& w, double offset, double release_offset = 0,
                      const ScoreRules& rules = {}) {
    Judge j;
    j.Reset(chart, w, rules);
    struct Input { double t; int lane; bool press; };
    std::vector<Input> inputs;
    for (const Note& n : chart.notes) {
        inputs.push_back({n.time_ms + offset, n.lane, true});
        inputs.push_back({(n.IsHold() ? n.end_ms + release_offset : n.time_ms + offset + 30), n.lane, false});
    }
    std::stable_sort(inputs.begin(), inputs.end(), [](const Input& a, const Input& b) { return a.t < b.t; });
    for (const Input& in : inputs) {
        j.Update(in.t);
        in.press ? j.Press(in.lane, in.t) : j.Release(in.lane, in.t);
    }
    j.Update(chart.notes.empty() ? 0 : chart.notes.back().time_ms + 100000);
    return j;
}

static WindowSet GameWindows() {  // the values of s_EvaluateRangeDatas groups 0 and 1 (live_params.bin)
    WindowSet w;
    w.press.early = w.press.late = {40, 133, 200, 267};
    w.release.early = w.release.late = {133, 200, 267, 333};
    return w;
}

static void Synthetic() {
    Chart c;
    for (int i = 0; i < 10; ++i) c.notes.push_back({i % kLanes, 1000.0 + 500 * i, -1});
    c.notes.push_back({4, 7000, 8000});  // one hold
    const WindowSet w = GameWindows();
    const Judge perfect = Autoplay(c, w, 0);
    Check(perfect.counts[0] == 12 && perfect.max_combo == 12 && perfect.Finished(), "autoplay: 12 Perfect (11 notes + 1 release)");
    // the 1000 ms hold has round(1000 / 41) = 24 ticks of round(0.033 x 100) = 3 points (eboot 0x9ECC0, 0xA9D80)
    Check(perfect.score == 1200 + 24 * 3, "autoplay: 12 x 100 points + 24 ticks x 3");
    Check(Autoplay(c, w, 0, 0, {3.0}).score == 12 * 300 + 24 * 10, "difficulty multiplier 3.0 (ac_challenge)");
    Check(Autoplay(c, w, 0, -400).score == 11 * 100 + 10 + 14 * 3, "hold let go at 600 ms: 14 ticks, Bad release");
    // window edges are strict except the Bad bound (eboot 0xA0070)
    Check(Autoplay(c, w, 39).counts[0] == 12, "+39 ms is Perfect");
    Check(Autoplay(c, w, 40).counts[1] == 11, "+40 ms is Great (strict edge)");
    Check(Autoplay(c, w, -132).counts[1] == 11, "-132 ms is Great");
    Check(Autoplay(c, w, -133).counts[2] == 11, "-133 ms is Good (strict edge)");
    const Judge late = Autoplay(c, w, 250);
    Check(late.counts[3] == 11 && late.max_combo == 0, "+250 ms: 11 Bad, a Bad hold press ends the hold, combo broken");
    Check(late.score == 11 * 10, "Bad scores 10 points (weight 0.1), no ticks");
    Check(perfect.MaxScore() == perfect.score, "maximum score = an all-Perfect play (judgements + every tick)");
    Check(perfect.Rank() == 4 && late.Rank() == 0, "rank: clear rate 1.0 is the 4th threshold (0.925), 0.09 below the 1st");
    const Judge none = Autoplay(c, w, 400);  // outside every window: inputs ignored, all notes time out
    Check(none.counts[4] == 11, "no hit: 11 Misses (an unpressed hold is one Miss)");
    Check(Autoplay(c, w, 0, 150).counts[1] == 1, "hold released 150 ms late: Great release");
    Check(Autoplay(c, w, 0, -400).counts[3] == 1, "hold released before its release window: Bad");
}

static void Groups() {
    Chart c;
    for (double t : {100.0, 100.0, 200.0, 300.0, 300.5, 301.0, 400.0, 500.0, 500.0}) c.notes.push_back({0, t, -1});
    GroupNotes(c);
    std::string got;
    for (const Note& n : c.notes) got += std::to_string(n.group) + " ";
    Check(got == "0 0 -1 1 1 1 -1 2 2 ", "simultaneous groups numbered in song order, lone notes -1: " + got);
    // heads and hold tails are numbered together in time order (H4, rule A)
    const auto groups = [](std::initializer_list<Note> notes) {
        Chart h;
        h.notes = notes;
        GroupNotes(h);
        std::string s;
        for (const Note& n : h.notes) s += std::to_string(n.group) + "/" + std::to_string(n.end_group) + " ";
        return s;
    };
    got = groups({{0, 100, 200}, {1, 100, 200}, {0, 300, 400}, {1, 300, 400}});
    Check(got == "0/1 0/1 2/3 2/3 ", "two hold pairs: heads orange (even), tails blue (odd): " + got);
    got = groups({{0, 100, 300}, {1, 100, 300}, {2, 200, -1}, {3, 200, -1}});
    Check(got == "0/2 0/2 1/-1 1/-1 ", "a tap pair between a hold pair's heads and tails takes the next number: " + got);
    got = groups({{0, 100, 200}, {1, 100, 250}});
    Check(got == "0/-1 0/-1 ", "holds that start together but end apart: no tail group: " + got);
    got = groups({{0, 100, 200}, {1, 200, -1}});
    Check(got == "-1/-1 -1/-1 ", "a tail and a head at the same time do not form a group: " + got);
}

static void Real(const char* root) {
    sif::Vfs vfs;
    Check(vfs.Mount(sif::os::PathFromUtf8(root)), "mount");
    Song song;
    std::string error;
    Check(LoadSong(vfs, 10, 4, song, &error), "load song 0010 variant 04: " + error);
    std::printf("chart %s: %zu notes, lead-in %.0f ms, %.0f BPM, %d skipped\n", song.chart_name.c_str(),
                song.chart.notes.size(), song.chart.lead_in_ms, song.chart.first_bpm, song.chart.skipped);
    Check(song.chart.notes.size() == 466 + 27 && song.chart.lead_in_ms == 3428, "song 0010/04: 466 + 27 extra notes, lead-in 3428 ms");
    // stage-screen movies (M8 V3; shadPS4: song 0340 opened monitor_006, 0120 monitor_005, on stage 1)
    Check(song.monitor == 5, "song 0010: stage screens play monitor 5 (s_music_datas +0x124)");
    for (const auto& [id, want] : {std::pair{340, 6}, {120, 5}, {1010, 9}}) {
        Song s;
        Check(LoadMusicRecord(vfs, id, s) && s.monitor == want, "song " + std::to_string(id) + ": monitor " + std::to_string(want));
    }
    Check(MonitorMoviePath(vfs, 6, 1) == "movie/monitor_006.usm", "stage 1: movie/monitor_006.usm");
    Check(MonitorMoviePath(vfs, 6, 6) == "movie/monitor_006_b.usm", "stage 6 (byte +0x2F = 1): the _b strip");
    Check(MonitorMoviePath(vfs, 0, 1).empty(), "no monitor number: no movie");
    for (int n : {1, 2, 3, 4, 5, 6, 7, 9, 51, 52, 53}) {
        Check(vfs.Exists(MonitorMoviePath(vfs, n, 17)), "movie for monitor " + std::to_string(n) + " is in the dump");
    }
    const WindowSet& w = song.windows;
    Check(w.press.early[0] == 40 && w.press.late[3] == 267 && w.release.late[3] == 333, "windows read from live_params.bin");
    for (double off : {0.0, 30.0, -100.0, 180.0}) {
        const Judge j = Autoplay(song.chart, w, off);
        std::printf("offset %+5.0f ms: P %d  Gr %d  Go %d  B %d  M %d  max combo %d  score %.0f\n", off, j.counts[0],
                    j.counts[1], j.counts[2], j.counts[3], j.counts[4], j.max_combo, j.score);
        Check(j.Finished(), "autoplay finishes");
    }
    const Judge p = Autoplay(song.chart, w, 0, 0, song.rules);
    Check(p.counts[1] + p.counts[2] + p.counts[3] + p.counts[4] == 0, "song 0010 autoplay: all Perfect");
    Check(song.rules.multiplier == 3.0f && song.rules.tick_ms == 41, "song 0010/04 (ac_challenge): multiplier 3.0");
    Check(song.rules.rank_rates[0] == 0.45f && song.rules.rank_rates[5] == 1.2f, "rank thresholds read (+0x138..+0x14C)");
    Check(p.finale && p.score > p.MaxScore(), "song 0010/04: all Perfect with the finale passes the maximum (finale points x3)");
    Check(song.group == 0, "song 0010: group 0 (mu's)");
    Check(song.dance_start_frame == 972, "song 0010: music starts at dance frame 972 (s_music_datas +0x104)");
    std::printf("song 0010/04 all Perfect: score %.0f\n", p.score);
    // song 0340 ac_easy: the chart measured on the game's result screens in shadPS4
    Song easy;
    Check(LoadSong(vfs, 340, 0, easy, &error), "load song 0340 variant 00: " + error);
    // channels 2-4 are played: the game's result counts 101 + 27 = 128 (Easy) and 167 + 27 = 194 (Normal) notes
    Check(easy.chart.notes.size() == 128, "song 0340/00: 101 + 27 extra notes = 128, got " + std::to_string(easy.chart.notes.size()));
    Song normal;
    Check(LoadSong(vfs, 340, 1, normal, &error) && normal.chart.notes.size() == 194, "song 0340/01: 167 + 27 = 194 notes");
    int icons = 0;
    for (const Note& n : normal.chart.notes) icons += n.icon;
    Check(icons == kFinaleIcons, "song 0340/01: nine icon notes, got " + std::to_string(icons));
    std::printf("song 0340/01 phases %.0f %.0f %.0f ms, finale %.0f ms\n", normal.chart.phase_ms[0], normal.chart.phase_ms[1],
                normal.chart.phase_ms[2], normal.chart.finale_ms);
    // G0: finale at audio 108.7 s; phases in order before it
    const auto& ph = normal.chart.phase_ms;
    Check(ph[0] > 0 && ph[0] < ph[1] && ph[1] < ph[2] && ph[2] < normal.chart.finale_ms &&
              std::abs(normal.chart.finale_ms - 108700) < 500,
          "song 0340: phase 1 and finale times from channel 5");
    const Judge all = Autoplay(normal.chart, w, 0, 0, normal.rules);
    Check(all.icons == kFinaleIcons && all.finale, "autoplay collects all nine icons and plays the finale");
    Check(Autoplay(normal.chart, w, 1000).icons == 0, "no hit, no icon");
    Check(normal.rules.finale_multiplier == 3.0f, "finale multiplier 3.0 (s_CommonParam +0x120)");
    // the same play without the finale (icons taken off): every point before the finale is equal, after it x3
    Chart plain = normal.chart;
    for (Note& n : plain.notes) n.icon = false;
    const Judge no_finale = Autoplay(plain, w, 0, 0, normal.rules);
    Check(!no_finale.finale && no_finale.score == no_finale.MaxScore(), "no icons: no finale, all Perfect = maximum");
    std::printf("song 0340/01 all Perfect: %.0f without the finale, %.0f with it (clear rate %.3f); game run 1: HUD 49865\n",
                no_finale.score, all.score, all.ClearRate());
    Check(all.score > no_finale.score && all.ClearRate() <= 1.2, "the finale raises the score; clear rate capped at 1.2");
    std::vector<SongInfo> list;
    Check(LoadSongList(vfs, 1, list), "song list");
    for (const SongInfo& i : list) {
        if (i.id == 10) Check(i.group == 0 && i.unit == 0 && i.members.size() == 9 && i.stars[4] == 11, "list: 0010 mu's, 9 members, CHALLENGE level 11");
        if (i.id == 1940) Check(i.group == 2 && i.unit == 10 && i.members == std::vector<int>{21, 22}, "list: 1940 Saint Snow, members 21 and 22");
    }
    std::vector<StageInfo> stages;
    Check(LoadStageList(vfs, 1, stages) && stages.size() == 30 && stages[0].id == 1 && stages[0].name == "Live Show Stage" &&
              stages.back().id == 112 && stages.back().name == "HAKODATE",
          "stage list: 30 stages, Live Show Stage .. HAKODATE");
    std::vector<DressInfo> dresses;
    Check(LoadDressList(vfs, 1, dresses) && dresses.size() == 70 && dresses[0].id == 31, "costume list: 70 costumes, first 31");
    for (const SongInfo& i : list) {
        if (i.id == 20) Check(PickCostume(vfs, i, dresses) == "d004", "costume: Snow Halation wears its own (d004)");
        if (i.id == 30) Check(PickCostume(vfs, i, dresses) == "d000", "costume: a song without its own costume -> d000");
        if (i.id == 30) Check(CostumesFor(vfs, i, dresses).front().id == 0, "costume list for 0030 starts with the uniform");
        if (i.id == 1940) {
            std::vector<int> ids;
            for (const DressInfo& d : CostumesFor(vfs, i, dresses)) ids.push_back(d.id);
            Check(ids.size() == 3 && ids[0] / 100 == 2 && ids[1] / 100 == 2 && ids[2] / 100 == 2, "costume list for 1940: the 3 Saint Snow costumes");
        }
        if (i.id == 1940) Check(PickCostume(vfs, i, dresses).starts_with("d2"), "costume: Saint Snow -> one of theirs (d2xx)");
    }
    Song aqours;
    Check(LoadSong(vfs, 1020, 4, aqours, &error) && aqours.group == 1, "song 1020 (Aozora Jumping Heart): group 1 (Aqours)");
    int holds = 0, ticks = 0;
    for (const Note& n : easy.chart.notes) {
        if (!n.IsHold()) continue;
        ++holds;
        ticks += int(std::round((n.end_ms - n.time_ms) / easy.rules.tick_ms));
    }
    const Judge e = Autoplay(easy.chart, w, 0, 0, easy.rules);
    std::printf("song 0340/00: %zu notes, %d holds, %d ticks, %d skipped, multiplier %.1f, all Perfect %.0f\n",
                easy.chart.notes.size(), holds, ticks, easy.chart.skipped, easy.rules.multiplier, e.score);
}

int main(int argc, char** argv) {
    Synthetic();
    Groups();
    if (argc > 1) Real(argv[1]);
    std::printf("%s\n", failures ? "live_test FAILED" : "live_test ok");
    return failures ? 1 : 0;
}
