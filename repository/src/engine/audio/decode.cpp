#include "engine/audio/decode.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

extern "C" {
#include "clhca.h"
}

namespace sif::cri {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

uint32_t Be(const uint8_t* p, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) v = v << 8 | p[i];
    return v;
}

}

bool DecodeHca(std::span<const uint8_t> wave, Pcm& out, std::string* error) {
    const int header = clHCA_isOurFile(wave.data(), unsigned(wave.size()));
    if (header < 0) return Fail(error, "not HCA");
    std::unique_ptr<clHCA, void (*)(clHCA*)> hca(clHCA_new(), clHCA_delete);
    clHCA_stInfo info{};
    if (clHCA_DecodeHeader(hca.get(), wave.data(), unsigned(header)) < 0 || clHCA_getInfo(hca.get(), &info) < 0) {
        return Fail(error, "bad HCA header");
    }
    if (info.encryptionEnabled) return Fail(error, "encrypted HCA (not supported)");
    out = {int(info.samplingRate), int(info.channelCount), {}};
    out.samples.reserve(size_t(info.blockCount) * info.samplesPerBlock * info.channelCount);
    std::vector<int16_t> block(size_t(info.samplesPerBlock) * info.channelCount);
    std::vector<uint8_t> buf(info.blockSize);
    for (unsigned b = 0; b < info.blockCount; ++b) {
        const size_t pos = size_t(header) + size_t(b) * info.blockSize;
        if (pos + info.blockSize > wave.size()) return Fail(error, "HCA truncated");
        std::memcpy(buf.data(), wave.data() + pos, info.blockSize);  // the decoder may modify its input
        if (clHCA_DecodeBlock(hca.get(), buf.data(), info.blockSize) < 0) return Fail(error, "HCA block " + std::to_string(b));
        clHCA_ReadSamples16(hca.get(), block.data());
        out.samples.insert(out.samples.end(), block.begin(), block.end());
    }
    // keep the samples the header declares: drop encoder delay and padding
    const size_t skip = size_t(info.encoderDelay) * info.channelCount, keep = size_t(info.sampleCount) * info.channelCount;
    if (skip + keep <= out.samples.size()) {
        out.samples.erase(out.samples.begin() + ptrdiff_t(skip + keep), out.samples.end());
        out.samples.erase(out.samples.begin(), out.samples.begin() + ptrdiff_t(skip));
    }
    return true;
}

bool DecodeAdx(std::span<const uint8_t> w, Pcm& out, std::string* error) {
    // header (big-endian): u16 0x8000, u16 copyright offset (data at offset + 4), u8 encoding (3), u8 frame size,
    // u8 bits per sample, u8 channels, u32 rate, u32 samples, u16 high-pass cutoff, u8 version, u8 flags
    if (w.size() < 20 || Be(&w[0], 2) != 0x8000) return Fail(error, "not ADX");
    const size_t data = Be(&w[2], 2) + 4;
    const int encoding = w[4], frame_size = w[5], bits = w[6], channels = w[7];
    const uint32_t rate = Be(&w[8], 4), total = Be(&w[12], 4), cutoff = Be(&w[16], 2);
    const int version = w[18], flags = w[19];
    if (encoding != 3 || bits != 4 || frame_size != 18 || channels < 1 || channels > 8) return Fail(error, "unsupported ADX type");
    if (flags != 0) return Fail(error, "encrypted ADX (not supported)");
    // prediction coefficients from the high-pass cutoff (same float math as CRI's library; vgmstream meta/adx.c)
    const float z = std::cos(2.0f * 3.14159265358979f * float(cutoff) / float(rate));
    const float a = 1.41421356f - z, b = 1.41421356f - 1.0f;
    const float c = (a - std::sqrt((a + b) * (a - b))) / b;
    const int coef1 = int16_t(c * 8192), coef2 = int16_t(c * c * -4096);

    out = {int(rate), channels, std::vector<int16_t>(size_t(total) * channels)};
    const size_t frame_bytes = size_t(frame_size) * channels;
    const int per_frame = (frame_size - 2) * 2;  // 32 samples
    std::vector<int> hist1(channels), hist2(channels);
    for (uint32_t s0 = 0, f = 0; s0 < total; s0 += per_frame, ++f) {
        const size_t frame = data + f * frame_bytes;
        if (frame + frame_bytes > w.size()) return Fail(error, "ADX truncated");
        for (int ch = 0; ch < channels; ++ch) {
            const uint8_t* p = &w[frame + size_t(ch) * frame_size];
            const int raw = int16_t(Be(p, 2));
            if (raw == int16_t(0x8001)) {  // end marker
                out.samples.resize(size_t(s0) * channels);
                return true;
            }
            const int scale = raw + 1;
            for (int i = 0; i < per_frame && s0 + i < total; ++i) {
                const int nibble = i & 1 ? p[2 + i / 2] & 0x0F : p[2 + i / 2] >> 4;  // high nibble first
                const int signed4 = nibble >= 8 ? nibble - 16 : nibble;
                int sample = version == 3 ? signed4 * scale + ((coef1 * hist1[ch]) >> 12) + ((coef2 * hist2[ch]) >> 12)
                                          : signed4 * scale + ((coef1 * hist1[ch] + coef2 * hist2[ch]) >> 12);
                sample = std::clamp(sample, -32768, 32767);
                out.samples[size_t(s0 + i) * channels + ch] = int16_t(sample);
                hist2[ch] = hist1[ch];
                hist1[ch] = sample;
            }
        }
    }
    return true;
}

bool DecodeWave(std::span<const uint8_t> wave, Pcm& out, std::string* error) {
    if (wave.size() >= 4 && std::memcmp(wave.data(), "HCA", 3) == 0) return DecodeHca(wave, out, error);
    if (wave.size() >= 2 && wave[0] == 0x80 && wave[1] == 0x00) return DecodeAdx(wave, out, error);
    return Fail(error, "unknown wave codec");
}

}
