// Unit test for the CRI audio path. Synthetic: an ADX frame decodes as specified. With the CUSA24620 dump folder as
// the first argument: every cue of MUSIC, VOICE and SYS resolves to a wave, and sample HCA/ADX waves decode to
// their declared length with audio-like statistics (values from analysis/202610312004-m2-formats/).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/audio/cue_sheet.h"
#include "engine/audio/decode.h"
#include "engine/audio/sound_bank.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", what.c_str());
    }
}

// RMS and zero crossings per second of channel 0 (white noise at 48 kHz gives ~24,000 crossings/s)
static void Stats(const sif::cri::Pcm& p, double& rms, double& zc) {
    double sum = 0;
    size_t crossings = 0, n = p.Frames();
    for (size_t i = 0; i < n; ++i) {
        const int s = p.samples[i * size_t(p.channels)];
        sum += double(s) * s;
        if (i && (s < 0) != (p.samples[(i - 1) * size_t(p.channels)] < 0)) ++crossings;
    }
    rms = n ? std::sqrt(sum / double(n)) : 0;
    zc = n ? double(crossings) * p.rate / double(n) : 0;
}

static void Synthetic() {
    // mono ADX, 32 samples: header (data at 0x24), one frame with scale 0x0FFF and nibbles 1,-1,...; end frame
    std::vector<uint8_t> adx(0x24 + 18 + 18, 0);
    const uint8_t head[20] = {0x80, 0x00, 0x00, 0x20, 3, 18, 4, 1, 0, 0, 0xBB, 0x80, 0, 0, 0, 32, 0x01, 0xF4, 4, 0};
    std::memcpy(adx.data(), head, sizeof(head));
    adx[0x24] = 0x0F;
    adx[0x25] = 0xFF;
    for (int i = 0; i < 16; ++i) adx[0x26 + i] = 0x1F;  // high nibble +1, low nibble -1
    adx[0x24 + 18] = 0x80;
    adx[0x24 + 19] = 0x01;
    sif::cri::Pcm pcm;
    std::string error;
    Check(sif::cri::DecodeAdx(adx, pcm, &error) && pcm.Frames() == 32 && pcm.rate == 48000, "synthetic ADX decodes: " + error);
    Check(!pcm.samples.empty() && pcm.samples[0] == 4096, "first ADX sample = 1 * (0x0FFF + 1)");
}

static void Real(const char* root) {
    sif::Vfs vfs;
    Check(vfs.Mount(sif::os::PathFromUtf8(root)), "mount");
    for (const char* name : {"music", "voice", "sys"}) {
        sif::cri::CueSheet sheet;
        std::string error;
        Check(sif::cri::LoadCueSheet(vfs, name, sheet, &error), std::string("load ") + name + ": " + error);
        size_t resolved = 0, total = 0;
        for (const std::string& cue : sheet.CueNames()) {
            sif::cri::Wave w;
            ++total;
            if (sheet.FindWave(cue, w)) ++resolved;
        }
        std::printf("%s: %zu/%zu cues resolve to a wave\n", name, resolved, total);
        Check(total > 0 && resolved == total, std::string(name) + ": every cue resolves");
    }

    sif::cri::CueSheet music;
    sif::cri::LoadCueSheet(vfs, "music", music);
    std::string song0010;
    for (const std::string& cue : music.CueNames()) {
        sif::cri::Wave w;
        if (music.FindWave(cue, w) && music.StreamBankName(w.port) == "MUSIC_0010") song0010 = cue;
    }
    std::printf("song 0010 cue: '%s'\n", song0010.c_str());
    sif::cri::Pcm pcm;
    std::string error;
    Check(!song0010.empty() && sif::cri::LoadCue(vfs, music, song0010, pcm, &error), "decode song 0010: " + error);
    Check(pcm.Frames() == 6068572 && pcm.channels == 2 && pcm.rate == 48000, "song 0010: 6,068,572 stereo frames at 48 kHz");

    sif::cri::CueSheet sys;
    sif::cri::LoadCueSheet(vfs, "sys", sys);
    int adx = 0, hca = 0;
    for (const std::string& cue : sys.CueNames()) {
        sif::cri::Wave w;
        if (!sys.FindWave(cue, w) || (w.encode == 0 ? adx : hca) >= 3) continue;
        sif::cri::Pcm p;
        const bool ok = sif::cri::LoadCue(vfs, sys, cue, p, &error);
        double rms = 0, zc = 0;
        if (ok) Stats(p, rms, zc);
        std::printf("sys %s '%s': %zu frames (declared %lld), %d Hz, rms %.0f, %.0f crossings/s\n",
                    w.encode == 0 ? "ADX" : "HCA", cue.c_str(), p.Frames(), (long long)w.samples, p.rate, rms, zc);
        Check(ok && int64_t(p.Frames()) == w.samples && rms > 10 && zc < 12000, "sys cue " + cue + ": " + error);
        (w.encode == 0 ? adx : hca)++;
    }
    Check(adx == 3 && hca == 3, "decoded 3 ADX and 3 HCA system sounds");

    // H5: judgement sounds are layered cues (polyphonic sequences); Perfect = waves 184 + 249 + the shared 46
    std::vector<sif::cri::Layer> layers;
    std::string ids;
    Check(sys.FindLayersById(20030, layers, &error), "Perfect cue 20030 resolves: " + error);
    for (const auto& l : layers) ids += std::to_string(l.wave.memory_id) + "@" + std::to_string(int(l.gain * 100 + 0.5f)) + " ";
    std::printf("Perfect layers (wave@gain%%): %s\n", ids.c_str());
    Check(ids == "184@100 249@100 46@41 ", "Perfect: three layers, the shared one at 41%: " + ids);
    Check(sys.FindLayersById(20034, layers) && layers.size() == 1 && layers[0].wave.memory_id == 204, "miss: one layer (wave 204)");
    Check(sys.FindLayersById(20039, layers) && layers.size() == 4, "Perfect_fl: four layers (adds the finale wave 195)");
    for (int id = 20030; id <= 20043; ++id) {
        if (id > 20034 && id < 20039) continue;
        sif::cri::Pcm mix;
        const bool ok = sif::cri::LoadCueMix(vfs, sys, id, 48000, 2, mix, &error);
        double rms = 0, zc = 0;
        if (ok) Stats(mix, rms, zc);
        std::printf("cue %d mixed: %zu frames, rms %.0f\n", id, mix.Frames(), rms);
        Check(ok && mix.rate == 48000 && mix.channels == 2 && mix.Frames() > 4000 && rms > 10,
              "cue " + std::to_string(id) + " mixes to 48 kHz stereo: " + error);
    }
    sif::cri::Pcm perfect;
    sif::cri::LoadCueMix(vfs, sys, 20030, 48000, 2, perfect);
    Check(perfect.Frames() == 73134, "Perfect mix is as long as its longest layer (wave 46, 73,134 frames at 48 kHz)");
}

int main(int argc, char** argv) {
    Synthetic();
    if (argc > 1) Real(argv[1]);
    std::printf("%s\n", failures ? "audio_test FAILED" : "audio_test ok");
    return failures ? 1 : 0;
}
