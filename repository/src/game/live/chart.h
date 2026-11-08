#pragma once
// A playable SIF chart: the `ch1` Scor file of a song (spec: docs/formats/scd.md). Times are in milliseconds of
// *audio* time: chart time + header 0x14 (the lead-in; verified against audio onsets of songs 0010, 0020, 0050).

#include <array>
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
    int group = -1;         // simultaneous group, numbered 0, 1, 2 ... in song order; -1 for a note alone
    int channel = 1;        // chart file _chN: 1 the main notes, 2-4 the extra notes of the three phases
    bool icon = false;      // carries the Finale Rhythm Icon (one of the nine LOVELIVE! letters)
    bool IsHold() const { return end_ms >= 0; }
};

struct Chart {
    std::vector<Note> notes;  // sorted by time
    double lead_in_ms = 0;    // header 0x14
    double first_bpm = 0;
    int skipped = 0;          // notes outside lanes 55..63 (note number 0 in home-version charts; meaning unknown)
    // from channel 5 (ParseTimeline): the start of phases 1-3 and of the finale section; < 0 when absent
    std::array<double, 3> phase_ms{-1, -1, -1};
    double finale_ms = -1;
};

bool ParseChart(std::span<const uint8_t> scor, Chart& out, std::string* error = nullptr);

// Sets Note::group: notes (sorted by time) within 1 ms of the previous one share a group. ParseChart calls it; call it
// again after adding notes.
void GroupNotes(Chart& chart);

// Channel 5, the timeline: notes 60-68 start phases 1-3 (60/63/66, 61/64/67, 62/65/68), note 69 the finale. Sets
// chart.phase_ms and chart.finale_ms; the notes are left alone.
bool ParseTimeline(std::span<const uint8_t> scor, Chart& chart, std::string* error = nullptr);
// After channels 2-4 are appended (Note::channel set): sorts the notes by time, sets Note::icon for the icon channel
// of each phase, and regroups.
void MarkIcons(Chart& chart);

// Chart names carry the song's short title ("score_0010_04_bokurara_ch1.scd"), so callers list
// live/<id>/<id>.pac/score.pac and pick the entry that starts with this prefix and ends with "_ch1.scd".
std::string ChartPrefix(int song_id, int variant);

}
