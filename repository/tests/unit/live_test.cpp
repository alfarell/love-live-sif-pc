// Headless autoplay test for the live (song play) rules. Synthetic: window edges, combo, holds. With the CUSA24620
// dump folder as the first argument: song 0010 (ac_challenge chart) autoplayed at fixed input offsets.
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
static Judge Autoplay(const Chart& chart, const WindowSet& w, double offset, double release_offset = 0) {
    Judge j;
    j.Reset(chart, w);
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
    Check(perfect.score > 999'999 && perfect.score < 1'000'001, "autoplay: provisional score 1,000,000");
    Check(Autoplay(c, w, 40).counts[0] == 12, "+40 ms is still Perfect (edge)");
    Check(Autoplay(c, w, 41).counts[1] == 11, "+41 ms is Great");
    Check(Autoplay(c, w, -133).counts[1] == 11, "-133 ms is Great (edge)");
    Check(Autoplay(c, w, -150).counts[2] == 11, "-150 ms is Good");
    const Judge late = Autoplay(c, w, 250);
    Check(late.counts[3] == 11 && late.max_combo <= 1, "+250 ms is Bad (inside the 267 ms bound) and breaks the combo");
    const Judge none = Autoplay(c, w, 400);  // outside every window: inputs ignored, all notes time out
    Check(none.counts[4] == 12, "no hit: 11 notes + 1 hold release missed");
    Check(Autoplay(c, w, 0, 150).counts[1] == 1, "hold released 150 ms late: Great release");
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
    const Judge p = Autoplay(song.chart, w, 0);
    Check(p.counts[1] + p.counts[2] + p.counts[3] + p.counts[4] == 0, "song 0010 autoplay: all Perfect");
}

int main(int argc, char** argv) {
    Synthetic();
    if (argc > 1) Real(argv[1]);
    std::printf("%s\n", failures ? "live_test FAILED" : "live_test ok");
    return failures ? 1 : 0;
}
