#include "engine/formats/png.h"

#include <zlib.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

namespace sif::png {
namespace {

void Be32(std::vector<uint8_t>& out, uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) out.push_back(uint8_t(v >> s));
}

void Chunk(std::vector<uint8_t>& out, const char type[4], std::span<const uint8_t> body) {
    Be32(out, uint32_t(body.size()));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), body.begin(), body.end());
    Be32(out, uint32_t(crc32(0, out.data() + start, uInt(out.size() - start))));
}

}

std::vector<uint8_t> Encode(std::span<const uint8_t> rgba, uint32_t width, uint32_t height) {
    if (rgba.size() < size_t(width) * height * 4) return {};
    // filter byte 0 (none) before each row
    std::vector<uint8_t> raw;
    raw.reserve(size_t(height) * (size_t(width) * 4 + 1));
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t* row = rgba.data() + size_t(y) * width * 4;
        raw.insert(raw.end(), row, row + size_t(width) * 4);
    }
    uLongf packed = compressBound(uLong(raw.size()));
    std::vector<uint8_t> z(packed);
    if (compress2(z.data(), &packed, raw.data(), uLong(raw.size()), 6) != Z_OK) return {};
    z.resize(packed);
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    Be32(ihdr, width), Be32(ihdr, height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8 bits, RGBA, deflate, adaptive filtering, no interlace
    Chunk(out, "IHDR", ihdr);
    Chunk(out, "IDAT", z);
    Chunk(out, "IEND", {});
    return out;
}

namespace {
uint32_t Read32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint8_t Paeth(int a, int b, int c) {
    const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return uint8_t(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}
}

bool Read(const std::filesystem::path& path, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height,
          std::string* error) {
    const auto fail = [&](const char* why) {
        if (error) *error = path.string() + ": " + why;
        return false;
    };
    std::ifstream f(path, std::ios::binary);
    const std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    static const uint8_t kSig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (file.size() < 33 || !std::equal(kSig, kSig + 8, file.begin())) return fail("not a PNG");
    std::vector<uint8_t> z;
    width = height = 0;
    for (size_t o = 8; o + 12 <= file.size();) {
        const uint32_t n = Read32(&file[o]);
        if (o + 12 + n > file.size()) return fail("truncated");
        const uint8_t* body = &file[o + 8];
        if (!std::memcmp(&file[o + 4], "IHDR", 4)) {
            width = Read32(body), height = Read32(body + 4);
            if (body[8] != 8 || body[9] != 6 || body[12] != 0) return fail("only 8-bit RGBA, not interlaced");
        } else if (!std::memcmp(&file[o + 4], "IDAT", 4)) {
            z.insert(z.end(), body, body + n);
        }
        o += 12 + n;
    }
    const size_t stride = size_t(width) * 4;
    if (!width || !height || width > 16384 || height > 16384) return fail("bad size");
    std::vector<uint8_t> raw(size_t(height) * (stride + 1));
    uLongf got = uLongf(raw.size());
    if (uncompress(raw.data(), &got, z.data(), uLong(z.size())) != Z_OK || got != raw.size()) return fail("zlib error");
    rgba.assign(size_t(height) * stride, 0);
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t type = raw[y * (stride + 1)];
        const uint8_t* in = &raw[y * (stride + 1) + 1];
        uint8_t* out = &rgba[y * stride];
        const uint8_t* up = y ? out - stride : nullptr;
        for (size_t x = 0; x < stride; ++x) {
            const int a = x >= 4 ? out[x - 4] : 0, b = up ? up[x] : 0, c = up && x >= 4 ? up[x - 4] : 0;
            const int pred = type == 1 ? a : type == 2 ? b : type == 3 ? (a + b) / 2 : type == 4 ? Paeth(a, b, c) : 0;
            if (type > 4) return fail("bad filter");
            out[x] = uint8_t(in[x] + pred);
        }
    }
    return true;
}

bool Write(const std::filesystem::path& path, std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
           std::string* error) {
    const std::vector<uint8_t> file = Encode(rgba, width, height);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (file.empty() || !f.write(reinterpret_cast<const char*>(file.data()), std::streamsize(file.size()))) {
        if (error) *error = "cannot write " + path.string();
        return false;
    }
    return true;
}

}
