#include "engine/formats/btx.h"

#include <algorithm>
#include <cstring>

namespace sif::btx {
namespace {

uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

// bytes per 4x4 block, or per pixel for uncompressed formats
uint32_t Unit(Format f) {
    switch (f) {
        case Format::BC1: return 8;
        case Format::BC3: case Format::BC7: return 16;
        case Format::BGRA8: return 4;
        case Format::BGR8: return 3;
        case Format::R8: return 1;
    }
    return 4;
}

uint64_t LevelSize(uint32_t w, uint32_t h, Format f) {
    if (IsBlockFormat(f)) return uint64_t(std::max(1u, (w + 3) / 4)) * std::max(1u, (h + 3) / 4) * Unit(f);
    return uint64_t(w) * h * Unit(f);
}

}

bool IsBlockFormat(Format f) { return f == Format::BC1 || f == Format::BC3 || f == Format::BC7; }

bool Parse(std::span<const uint8_t> file, Image& out, std::string* error) {
    if (file.size() < 0x30 || std::memcmp(file.data(), "btx\0", 4) != 0) return Fail(error, "not a btx texture");
    out = {};
    out.width = uint32_t(file[8] | file[9] << 8);
    out.height = uint32_t(file[10] | file[11] << 8);
    const uint32_t pitch = U32(&file[0x0C]);
    switch (file[0x10]) {
        case 0: out.format = Format::BGRA8; break;
        case 1: out.format = Format::BGR8; break;
        case 7: out.format = Format::BC1; break;
        case 9: out.format = Format::BC3; break;
        case 13: out.format = Format::BC7; break;
        case 16: out.format = Format::R8; break;
        default: return Fail(error, "unknown btx format " + std::to_string(file[0x10]));
    }
    const uint32_t mips = uint32_t(file[0x12]) + 1;
    const uint32_t data_offset = U32(&file[0x18]);
    const uint32_t name_offset = U32(&file[0x24]);
    if (data_offset > file.size() || out.width == 0 || out.height == 0) return Fail(error, "bad btx header");
    if (name_offset < data_offset) {
        const char* name = reinterpret_cast<const char*>(&file[name_offset]);
        out.name.assign(name, strnlen(name, data_offset - name_offset));
    }
    out.data = file.subspan(data_offset);
    uint64_t pos = 0;
    for (uint32_t i = 0; i < mips; ++i) {
        Level l;
        l.width = std::max(1u, out.width >> i);
        l.height = std::max(1u, out.height >> i);
        l.size = LevelSize(l.width, l.height, out.format);
        l.pitch = IsBlockFormat(out.format) ? std::max(1u, (l.width + 3) / 4) * Unit(out.format) : l.width * Unit(out.format);
        if (i == 0 && !IsBlockFormat(out.format) && pitch > l.pitch) {
            l.pitch = pitch;  // padded rows (469-wide card text has a 470-pixel pitch)
            l.size = uint64_t(pitch) * l.height;
        }
        l.offset = pos;
        pos += l.size;
        out.levels.push_back(l);
    }
    out.chain_size = pos;
    if (pos > out.data.size()) return Fail(error, "btx data truncated");
    out.cube = out.data.size() >= 6 * pos;  // shortcut: size-based cube detection, as tools/tex2png.py does
    return true;
}

std::vector<uint8_t> ToRgba8(const Image& image) {
    const Level& l = image.levels.front();
    std::vector<uint8_t> out(size_t(l.width) * l.height * 4);
    for (uint32_t y = 0; y < l.height; ++y) {
        const uint8_t* row = image.data.data() + l.offset + uint64_t(y) * l.pitch;
        uint8_t* dst = &out[size_t(y) * l.width * 4];
        for (uint32_t x = 0; x < l.width; ++x, dst += 4) {
            switch (image.format) {
                case Format::BGRA8: dst[0] = row[x * 4 + 2]; dst[1] = row[x * 4 + 1]; dst[2] = row[x * 4]; dst[3] = row[x * 4 + 3]; break;
                case Format::BGR8: dst[0] = row[x * 3 + 2]; dst[1] = row[x * 3 + 1]; dst[2] = row[x * 3]; dst[3] = 255; break;
                case Format::R8: dst[0] = dst[1] = dst[2] = row[x]; dst[3] = 255; break;
                default: return {};  // block formats are uploaded as-is
            }
        }
    }
    return out;
}

}
