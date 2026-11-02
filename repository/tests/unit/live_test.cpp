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

static void Real(const char* root) {
    sif::Vfs vfs;
    Check(vfs.Mount(sif::os::PathFromUtf8(root)), "mount");
    Song song;
    std::string error;
    Check(LoadSong(vfs, 10, 4, song, &error), "load song 0010 variant 04: " + error);
    std::printf("chart %s: %zu notes, lead-in %.0f ms, %.0f BPM, %d skipped\n", song.chart_name.c_str(),
                song.chart.notes.size(), song.chart.lead_in_ms, song.chart.first_bpm, song.chart.skipped);
    Check(song.chart.notes.size() == 466 && song.chart.lead_in_ms == 3428, "song 0010/04: 466 notes, lead-in 3428 ms");
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
    Check(p.MaxScore() == p.score, "song 0010/04: all Perfect = maximum score");
    Check(song.group == 0, "song 0010: group 0 (mu's)");
    Check(song.dance_start_frame == 972, "song 0010: music starts at dance frame 972 (s_music_datas +0x104)");
    std::printf("song 0010/04 all Perfect: score %.0f\n", p.score);
    // song 0340 ac_easy: the chart measured on the game's result screens in shadPS4
    Song easy;
    Check(LoadSong(vfs, 340, 0, easy, &error), "load song 0340 variant 00: " + error);
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
    if (argc > 1) Real(argv[1]);
    std::printf("%s\n", failures ? "live_test FAILED" : "live_test ok");
    return failures ? 1 : 0;
}
