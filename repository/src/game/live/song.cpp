#include "game/live/song.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "engine/audio/sound_bank.h"
#include "engine/formats/blbn.h"

namespace sif::live {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

template <class T> T At(std::span<const uint8_t> d, size_t off) {
    T v;
    std::memcpy(&v, &d[off], sizeof(T));
    return v;
}

bool ReadTable(const Vfs& vfs, const char* path, const char* symbol, std::vector<uint8_t>& file,
               std::span<const uint8_t>& out, std::string* error) {
    std::vector<blbn::Symbol> symbols;
    if (!vfs.Read(path, file, error) || !blbn::Parse(file, symbols, error)) return false;
    const blbn::Symbol* s = blbn::Find(symbols, symbol);
    if (!s) return Fail(error, std::string(path) + " has no " + symbol);
    out = s->data;
    return true;
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

bool LoadScoreRules(const Vfs& vfs, int id, int variant, ScoreRules& out, std::string* error) {
    std::vector<uint8_t> params_file, datas_file;
    std::span<const uint8_t> param, datas;
    if (!ReadTable(vfs, "common/common.pac/live_common.pac/live_params.bin", "s_CommonParam", params_file, param, error) ||
        !ReadTable(vfs, "common/common.pac/live_common.pac/live_datas.bin", "s_live_datas", datas_file, datas, error)) {
        return false;
    }
    if (param.size() < 0x134) return Fail(error, "s_CommonParam too short");
    // s_live_datas: 904-byte records keyed by id = song x 1000 + variant; +0x14 = difficulty index 0-8 (eboot 0x945B0)
    constexpr size_t kRecord = 904;
    for (size_t r = 0; r + kRecord <= datas.size(); r += kRecord) {
        if (At<int32_t>(datas, r) != id * 1000 + variant) continue;
        const int level = std::clamp(At<int32_t>(datas, r + 0x14), 0, 8);  // clamped like eboot 0xCA910
        out.multiplier = At<float>(param, 0xD0 + size_t(level) * 4);
        out.tick_rate = At<float>(param, 0x128);
        out.tick_ms = int(At<float>(param, 0x130) * 1000.0f);
        return true;
    }
    return Fail(error, "no s_live_datas record for this chart");
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
    return LoadWindows(vfs, out.windows, error) && LoadScoreRules(vfs, id, variant, out.rules, error);
}

bool LoadSongMusic(const Vfs& vfs, Song& song, cri::Pcm& out, std::string* error) {
    cri::CueSheet music;
    cri::Wave wave;
    if (!cri::LoadCueSheet(vfs, "music", music, error) || !music.FindWaveById(song.id, wave, error)) return false;
    song.title = std::string(music.CueNameById(song.id));
    return cri::LoadWave(vfs, music, wave, out, error);
}

}
