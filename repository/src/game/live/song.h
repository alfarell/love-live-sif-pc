#pragma once
// Everything one live (song play) needs from the dump: the playable chart, the judgement windows and the music.

#include <string>

#include "engine/audio/decode.h"
#include "engine/fs/vfs.h"
#include "game/live/chart.h"
#include "game/live/judge.h"

namespace sif::live {

// variant: 00-04 = ac_easy, ac_normal, ac_hard, ac_extreme, ac_challenge; 10-13 = cs_basic .. cs_master
// (difficulty names from the eboot strings; the order is a Hypothesis based on note counts).
struct Song {
    int id = 0, variant = 0;
    std::string chart_name, title;
    Chart chart;
    WindowSet windows;
};

bool LoadSong(const Vfs& vfs, int id, int variant, Song& out, std::string* error = nullptr);
bool LoadWindows(const Vfs& vfs, WindowSet& out, std::string* error = nullptr);
// Music: the music.acb cue whose CueId equals the song id.
bool LoadSongMusic(const Vfs& vfs, Song& song, cri::Pcm& out, std::string* error = nullptr);

}
