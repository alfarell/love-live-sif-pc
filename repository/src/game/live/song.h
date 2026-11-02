#pragma once
// Everything one live (song play) needs from the dump: the playable chart, the judgement windows and the music.

#include <array>
#include <string>
#include <vector>

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
    int group = 0;                // s_music_datas +0x10: 0 mu's, 1 Aqours, 2 Saint Snow
};

bool LoadSong(const Vfs& vfs, int id, int variant, Song& out, std::string* error = nullptr);
bool LoadWindows(const Vfs& vfs, WindowSet& out, std::string* error = nullptr);
// Score multiplier and hold-tick values for one chart (s_live_datas in live_datas.bin, s_CommonParam in live_params.bin).
bool LoadScoreRules(const Vfs& vfs, int id, int variant, ScoreRules& out, std::string* error = nullptr);
// The song's s_music_datas values: the dance/camera clock (frame = audio ms / 1000 x 60 + dance_start_frame) and
// its group.
bool LoadMusicRecord(const Vfs& vfs, int id, Song& out, std::string* error = nullptr);

// One playable song for the song select: s_music_datas order, only songs whose live/NNNN package is in the dump.
struct SongInfo {
    int id = 0;
    std::string title;           // s_music_datas title in the UI language (+0x48 + 8 x language)
    std::string credits;         // lyricist, composer, arranger lines (+0xD0, +0x90, +0xB0 + 8 x language)
    std::array<int, 9> stars{};  // star level per difficulty index (s_live_datas +0x18; 0 = no chart)
    int group = 0;               // s_music_datas +0x10: 0 mu's, 1 Aqours, 2 Saint Snow
    int unit = -1;               // +0x14: 0 mu's, 1 Printemps, 2 BiBi, 3 lily white, 4 Aqours, 5 mu's solo, 6 Aqours
                                 // solo, 7 CYaRon!, 8 AZALEA, 9 Guilty Kiss, 10 Saint Snow, -1 mixed (from the titles)
    std::vector<int> members;    // +0x18 count, +0x1C slots: member ids (1-9 mu's, 11-19 Aqours, 21-22 Saint Snow)
};
// `language`: 0 Japanese, 1 English, 2 simplified Chinese, 3 traditional Chinese (the order of the title pointers).
bool LoadSongList(const Vfs& vfs, int language, std::vector<SongInfo>& out, std::string* error = nullptr);
// Music: the music.acb cue whose CueId equals the song id.
bool LoadSongMusic(const Vfs& vfs, Song& song, cri::Pcm& out, std::string* error = nullptr);

}
