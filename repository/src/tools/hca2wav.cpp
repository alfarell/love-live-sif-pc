// sif_hca2wav: decode CRI HCA audio (a .hca file, or every HCA wave in an AFS2 .awb bank) to 16-bit WAV.
// Usage: sif_hca2wav <in.hca|in.awb> <out_dir> [--index N]   (--index: only the AWB wave with that position)
// Encoder delay and padding are trimmed as the HCA header says. Encrypted HCA is rejected (SIF's are not encrypted).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include "clhca.h"
}

namespace fs = std::filesystem;

static std::vector<uint8_t> ReadFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static uint32_t Le(const uint8_t* p, int n) {
    uint32_t v = 0;
    for (int i = n - 1; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

static void PutWav(std::vector<uint8_t>& out, const std::vector<int16_t>& pcm, unsigned rate, unsigned channels) {
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(uint8_t(v >> (8 * i))); };
    auto u16 = [&](uint16_t v) { out.push_back(uint8_t(v)); out.push_back(uint8_t(v >> 8)); };
    const uint32_t bytes = uint32_t(pcm.size() * 2);
    out.insert(out.end(), {'R', 'I', 'F', 'F'}); u32(36 + bytes);
    out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '}); u32(16); u16(1); u16(uint16_t(channels));
    u32(rate); u32(rate * channels * 2); u16(uint16_t(channels * 2)); u16(16);
    out.insert(out.end(), {'d', 'a', 't', 'a'}); u32(bytes);
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(pcm.data());
    out.insert(out.end(), raw, raw + bytes);
}

// Decodes one HCA stream; returns an error message or "" on success.
static std::string Decode(const uint8_t* data, size_t size, const fs::path& out_path) {
    const int header = clHCA_isOurFile(data, unsigned(size));
    if (header < 0) return "not HCA";
    clHCA* hca = clHCA_new();
    clHCA_stInfo info{};
    std::string err;
    if (clHCA_DecodeHeader(hca, data, unsigned(header)) < 0 || clHCA_getInfo(hca, &info) < 0) err = "bad header";
    else if (info.encryptionEnabled) err = "encrypted HCA (not supported)";
    std::vector<int16_t> pcm;
    std::vector<int16_t> block(size_t(info.samplesPerBlock) * info.channelCount);
    std::vector<uint8_t> buf(info.blockSize);
    for (unsigned b = 0; err.empty() && b < info.blockCount; ++b) {
        const size_t pos = size_t(header) + size_t(b) * info.blockSize;
        if (pos + info.blockSize > size) { err = "truncated"; break; }
        std::memcpy(buf.data(), data + pos, info.blockSize);  // the decoder may modify its input
        if (clHCA_DecodeBlock(hca, buf.data(), info.blockSize) < 0) { err = "block " + std::to_string(b); break; }
        clHCA_ReadSamples16(hca, block.data());
        pcm.insert(pcm.end(), block.begin(), block.end());
    }
    clHCA_delete(hca);
    if (!err.empty()) return err;
    const size_t skip = size_t(info.encoderDelay) * info.channelCount;
    const size_t keep = size_t(info.sampleCount) * info.channelCount;
    if (skip + keep <= pcm.size()) pcm = std::vector<int16_t>(pcm.begin() + skip, pcm.begin() + skip + keep);
    std::vector<uint8_t> wav;
    PutWav(wav, pcm, info.samplingRate, info.channelCount);
    std::ofstream(out_path, std::ios::binary).write(reinterpret_cast<const char*>(wav.data()), std::streamsize(wav.size()));
    std::printf("%s: %u Hz, %u ch, %u samples\n", out_path.filename().string().c_str(), info.samplingRate,
                info.channelCount, info.sampleCount);
    return "";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: sif_hca2wav <in.hca|in.awb> <out_dir> [--index N]\n");
        return 2;
    }
    const fs::path in = argv[1], out = argv[2];
    long only = -1;
    if (argc >= 5 && std::strcmp(argv[3], "--index") == 0) only = std::strtol(argv[4], nullptr, 10);
    fs::create_directories(out);
    const std::vector<uint8_t> f = ReadFile(in);
    int failed = 0, done = 0;
    if (f.size() >= 16 && std::memcmp(f.data(), "AFS2", 4) == 0) {
        // AFS2: u8 version, u8 offset size, u16 id size, u32 count, u16 align, u16 subkey, ids[count], offsets[count+1]
        const unsigned off_size = f[5], id_size = f[6];
        const uint32_t count = Le(&f[8], 4), align = Le(&f[12], 2);
        const size_t ids = 16, offs = ids + size_t(count) * id_size;
        if (offs + size_t(count + 1) * off_size > f.size()) { std::fprintf(stderr, "AFS2 table truncated\n"); return 1; }
        for (uint32_t i = 0; i < count; ++i) {
            if (only >= 0 && long(i) != only) continue;
            const uint32_t id = Le(&f[ids + size_t(i) * id_size], int(id_size));
            uint32_t start = Le(&f[offs + size_t(i) * off_size], int(off_size));
            const uint32_t end = Le(&f[offs + size_t(i + 1) * off_size], int(off_size));
            start = (start + align - 1) / align * align;
            if (start > end || end > f.size()) { std::fprintf(stderr, "wave %u: bad range\n", i); ++failed; continue; }
            const fs::path dst = out / (in.stem().string() + "_" + std::to_string(id) + ".wav");
            const std::string err = Decode(&f[start], end - start, dst);
            if (!err.empty()) { std::fprintf(stderr, "wave %u (id %u): %s\n", i, id, err.c_str()); ++failed; } else ++done;
        }
    } else {
        const std::string err = Decode(f.data(), f.size(), out / (in.stem().string() + ".wav"));
        if (!err.empty()) { std::fprintf(stderr, "%s\n", err.c_str()); ++failed; } else ++done;
    }
    std::printf("%d decoded, %d failed\n", done, failed);
    return failed ? 1 : 0;
}
