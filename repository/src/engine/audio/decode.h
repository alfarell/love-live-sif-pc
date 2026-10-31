#pragma once
// Whole-wave decoders for CRI HCA (via vgmstream clHCA) and ADX (standard type 3, unencrypted). Output: interleaved
// 16-bit PCM. Neither handles encryption (SIF's waves are not encrypted).

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sif::cri {

struct Pcm {
    int rate = 0;
    int channels = 0;
    std::vector<int16_t> samples;  // interleaved
    size_t Frames() const { return channels ? samples.size() / size_t(channels) : 0; }
};

bool DecodeHca(std::span<const uint8_t> wave, Pcm& out, std::string* error = nullptr);
bool DecodeAdx(std::span<const uint8_t> wave, Pcm& out, std::string* error = nullptr);
// Picks the decoder by magic ("HCA\0" or 0x8000).
bool DecodeWave(std::span<const uint8_t> wave, Pcm& out, std::string* error = nullptr);

}
