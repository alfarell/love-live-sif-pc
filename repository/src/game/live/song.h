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

struct StageInfo {
    int id = 0;        // s_stage_datas +0x00; thumbnail ui/ui_stage_thumbnail.pac stage_NNN
    int model = 0;     // +0x04: the stage folder stage/stNNN.pac
    std::string name;  // +0x08 + 8 x language
};
// The selectable stages in the game's order (stage_common.pac/stage_datas.bin), those whose folder is in the dump;
// the 3xx records (variants reusing the 1xx models; Hypothesis: the stage_gimmick_ thumbnails) are left out.
bool LoadStageList(const Vfs& vfs, int language, std::vector<StageInfo>& out, std::string* error = nullptr);

struct DressInfo {
    int id = 0;        // s_dress_datas +0x00: chr/cNN/dNNN/dNNN.pac, thumbnail ui/ui_dress_thumbnail.pac dress_thumb_NNNN
    std::string name;  // +0x08 + 8 x language
};
// The costumes in the game's order (common/dress.pac/dress_data.bin, 192-byte records after an empty first one).
bool LoadDressList(const Vfs& vfs, int language, std::vector<DressInfo>& out, std::string* error = nullptr);
// The costumes every member of `song` has a model for: d000 (the school uniform; not in the costume table, named
// "School Uniform" by the port) first, then the table's in its order.
std::vector<DressInfo> CostumesFor(const Vfs& vfs, const SongInfo& song, const std::vector<DressInfo>& dresses);
// The default costume ("dNNN", "" if none): the one named like the song (English names), else the first of
// CostumesFor. shortcut: the game lets the player dress each member (member cards, 30_costume preview); the port's
// costume step picks one costume for everyone, starting from this rule
std::string PickCostume(const Vfs& vfs, const SongInfo& song, const std::vector<DressInfo>& english_dresses);
std::string CostumeFolder(int id);  // 4 -> "d004"
// Music: the music.acb cue whose CueId equals the song id.
bool LoadSongMusic(const Vfs& vfs, Song& song, cri::Pcm& out, std::string* error = nullptr);

}
