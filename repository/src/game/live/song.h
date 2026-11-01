#pragma once
// Everything one live (song play) needs from the dump: the playable chart, the judgement windows and the music.

#include <string>

#include "engine/audio/decode.h"
#include "engine/fs/vfs.h"
#include "game/live/chart.h"
#include "game/live/judge.h"

namespace sif::live {

// variant: 00-04 = ac_easy, ac_normal, ac_hard, ac_extreme, ac_challenge; 10-13 = cs_basic .. cs_master
// (difficulty names from the eboot strings; the order matches the difficulty index 0-8 in s_live_datas).
struct Song {
    int id = 0, variant = 0;
    std::string chart_name, title;
    Chart chart;
    WindowSet windows;
    ScoreRules rules;
    float dance_start_frame = 0;  // motion/camera frame (60 fps) at audio time 0 (s_music_datas +0x104)
};

bool LoadSong(const Vfs& vfs, int id, int variant, Song& out, std::string* error = nullptr);
bool LoadWindows(const Vfs& vfs, WindowSet& out, std::string* error = nullptr);
// Score multiplier and hold-tick values for one chart (s_live_datas in live_datas.bin, s_CommonParam in live_params.bin).
bool LoadScoreRules(const Vfs& vfs, int id, int variant, ScoreRules& out, std::string* error = nullptr);
// The song's dance/camera clock: frame = audio ms / 1000 x 60 + this value.
bool LoadDanceStart(const Vfs& vfs, int id, float& out, std::string* error = nullptr);
// Music: the music.acb cue whose CueId equals the song id.
bool LoadSongMusic(const Vfs& vfs, Song& song, cri::Pcm& out, std::string* error = nullptr);

}
