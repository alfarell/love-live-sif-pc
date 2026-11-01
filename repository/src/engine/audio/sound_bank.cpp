#include "engine/audio/sound_bank.h"

#include <cstring>
#include <vector>

namespace sif::cri {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

// Reads one wave of a stream bank without loading the whole .awb: header, id/offset tables, then the wave.
bool ReadStreamWave(const Vfs& vfs, const std::string& path, uint32_t id, std::vector<uint8_t>& out, std::string* error) {
    std::vector<uint8_t> head;
    if (!vfs.ReadRange(path, 0, 16, head, error)) return false;
    const size_t table = Afs2TableSize(head);
    if (table == 0) return Fail(error, path + " is not an AFS2 bank");
    std::vector<Afs2Wave> waves;
    if (!vfs.ReadRange(path, 0, table, head, error) || !ParseAfs2(head, waves, error, true)) return false;
    for (const Afs2Wave& w : waves) {
        if (w.id == id) return vfs.ReadRange(path, w.offset, size_t(w.size), out, error);
    }
    return Fail(error, "wave " + std::to_string(id) + " not in " + path);
}

}

bool LoadCueSheet(const Vfs& vfs, std::string_view name, CueSheet& out, std::string* error) {
    std::vector<uint8_t> acb;
    return vfs.Read("sound/sound.pac/" + std::string(name) + ".acb", acb, error) && out.Load(std::move(acb), error);
}

bool LoadWave(const Vfs& vfs, const CueSheet& sheet, const Wave& wave, Pcm& out, std::string* error) {
    std::vector<uint8_t> data;
    if (wave.streaming) {
        const std::string bank(sheet.StreamBankName(wave.port));
        if (bank.empty()) return Fail(error, "no stream bank for port " + std::to_string(wave.port));
        std::string path = "sound/" + bank + ".awb";
        if (!vfs.Exists(path)) path = "sound/dlc/" + bank + ".awb";
        if (!vfs.Exists(path)) return Fail(error, "song add-on not installed: " + bank + ".awb");
        if (!ReadStreamWave(vfs, path, uint32_t(wave.stream_id), data, error)) return false;
    } else {
        const auto w = FindAfs2Wave(sheet.MemoryBank(), uint32_t(wave.memory_id));
        if (w.empty()) return Fail(error, "wave not in the embedded bank");
        data.assign(w.begin(), w.end());
    }
    return DecodeWave(data, out, error);
}

bool LoadCue(const Vfs& vfs, const CueSheet& sheet, std::string_view cue, Pcm& out, std::string* error) {
    Wave wave;
    return sheet.FindWave(cue, wave, error) && LoadWave(vfs, sheet, wave, out, error);
}

}
