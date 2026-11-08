#include "engine/audio/sound_bank.h"

#include <algorithm>
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

bool LoadCueMix(const Vfs& vfs, const CueSheet& sheet, int64_t cue_id, int rate, int channels, Pcm& out,
                std::string* error) {
    if (rate <= 0 || channels <= 0) return Fail(error, "bad target format");
    std::vector<Layer> layers;
    if (!sheet.FindLayersById(cue_id, layers, error)) return false;
    std::vector<float> mix;
    for (const Layer& l : layers) {
        Pcm pcm;
        if (!LoadWave(vfs, sheet, l.wave, pcm, error)) return false;
        const size_t in_frames = pcm.Frames();
        if (in_frames == 0 || pcm.rate <= 0) continue;
        const double step = double(pcm.rate) / rate;
        const size_t frames = size_t(double(in_frames) / step);
        if (mix.size() < frames * size_t(channels)) mix.resize(frames * size_t(channels), 0.0f);
        const auto sample = [&](size_t f, int c) {  // input frame f, output channel c
            const size_t ic = size_t(pcm.channels);
            if (ic == size_t(channels)) return float(pcm.samples[f * ic + size_t(c)]);
            if (ic == 1) return float(pcm.samples[f]);
            float sum = 0;
            for (size_t k = 0; k < ic; ++k) sum += pcm.samples[f * ic + k];
            return sum / float(ic);
        };
        for (size_t f = 0; f < frames; ++f) {
            const double pos = double(f) * step;
            const size_t f0 = std::min(size_t(pos), in_frames - 1), f1 = std::min(f0 + 1, in_frames - 1);
            const float t = float(pos - double(f0));
            for (int c = 0; c < channels; ++c) {
                mix[f * size_t(channels) + size_t(c)] += l.gain * (sample(f0, c) * (1 - t) + sample(f1, c) * t);
            }
        }
    }
    out.rate = rate;
    out.channels = channels;
    out.samples.resize(mix.size());
    for (size_t i = 0; i < mix.size(); ++i) out.samples[i] = int16_t(std::clamp(mix[i], -32768.0f, 32767.0f));
    return !out.samples.empty() || Fail(error, "cue id " + std::to_string(cue_id) + " decoded empty");
}

}
