#include "game/live/song.h"

#include <cstdio>
#include <vector>

#include "engine/audio/sound_bank.h"
#include "engine/formats/blbn.h"

namespace sif::live {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

}

bool LoadWindows(const Vfs& vfs, WindowSet& out, std::string* error) {
    std::vector<uint8_t> file;
    std::vector<blbn::Symbol> symbols;
    if (!vfs.Read("common/common.pac/live_common.pac/live_params.bin", file, error) || !blbn::Parse(file, symbols, error)) {
        return false;
    }
    const blbn::Symbol* range = blbn::Find(symbols, "s_EvaluateRangeDatas");
    if (!range) return Fail(error, "live_params.bin has no s_EvaluateRangeDatas");
    return WindowsFromTable(range->data, out, error);
}

bool LoadSong(const Vfs& vfs, int id, int variant, Song& out, std::string* error) {
    out = {};
    out.id = id;
    out.variant = variant;
    char folder[64];
    std::snprintf(folder, sizeof(folder), "live/%04d/%04d.pac/score.pac", id, id);
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List(folder, entries, error)) return false;
    const std::string prefix = ChartPrefix(id, variant);
    for (const pac::ArcEntry& e : entries) {
        if (e.name.starts_with(prefix) && e.name.ends_with("_ch1.scd")) out.chart_name = e.name;
    }
    if (out.chart_name.empty()) return Fail(error, "no chart " + prefix + "*_ch1.scd in " + folder);
    std::vector<uint8_t> scor;
    if (!vfs.Read(std::string(folder) + "/" + out.chart_name, scor, error) || !ParseChart(scor, out.chart, error)) return false;
    return LoadWindows(vfs, out.windows, error);
}

bool LoadSongMusic(const Vfs& vfs, Song& song, cri::Pcm& out, std::string* error) {
    cri::CueSheet music;
    cri::Wave wave;
    if (!cri::LoadCueSheet(vfs, "music", music, error) || !music.FindWaveById(song.id, wave, error)) return false;
    song.title = std::string(music.CueNameById(song.id));
    return cri::LoadWave(vfs, music, wave, out, error);
}

}
