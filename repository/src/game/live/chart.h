#pragma once
// A playable SIF chart: the `ch1` Scor file of a song (spec: docs/formats/scd.md). Times are in milliseconds of
// *audio* time: chart time + header 0x14 (the lead-in; verified against audio onsets of songs 0010, 0020, 0050).

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sif::live {

constexpr int kLanes = 9;
// note numbers 55..63 = lanes 0..8, left to right: L2 L1 Left Down Right/Square Cross Circle R1 R2 (confirmed in
// shadPS4 live frames, M4)
constexpr int kFirstNoteNumber = 55;

struct Note {
    int lane = 0;
    double time_ms = 0;     // audio time of the press
    double end_ms = -1;     // audio time of the release for holds, < 0 for taps
    bool IsHold() const { return end_ms >= 0; }
};

struct Chart {
    std::vector<Note> notes;  // sorted by time
    double lead_in_ms = 0;    // header 0x14
    double first_bpm = 0;
    int skipped = 0;          // notes outside lanes 55..63 (note number 0 in home-version charts; meaning unknown)
};

bool ParseChart(std::span<const uint8_t> scor, Chart& out, std::string* error = nullptr);

// Chart names carry the song's short title ("score_0010_04_bokurara_ch1.scd"), so callers list
// live/<id>/<id>.pac/score.pac and pick the entry that starts with this prefix and ends with "_ch1.scd".
std::string ChartPrefix(int song_id, int variant);

}
