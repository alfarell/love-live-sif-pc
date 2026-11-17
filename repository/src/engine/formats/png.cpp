#include "engine/formats/png.h"

#include <zlib.h>

#include <fstream>

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
