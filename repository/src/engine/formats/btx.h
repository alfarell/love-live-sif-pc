#pragma once
// SIF `btx\0` textures (spec: docs/formats/btx.md). Data is linear (not GPU-tiled), mips largest first.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sif::btx {

enum class Format { BGRA8, BGR8, BC1, BC3, BC7, R8 };

struct Level {
    uint32_t width = 0, height = 0;
    uint32_t pitch = 0;      // bytes per row (per block row for BC formats)
    uint64_t offset = 0;     // into Image::data
    uint64_t size = 0;
};

struct Image {
    Format format = Format::BGRA8;
    uint32_t width = 0, height = 0;
    bool cube = false;               // 6 faces, each with its own mip chain (detected by size; no header flag known)
    std::vector<Level> levels;       // face 0 only; face f starts at f * ChainSize()
    std::span<const uint8_t> data;   // texel data (points into the parsed buffer)
    uint64_t chain_size = 0;
    std::string name;
};

bool IsBlockFormat(Format f);
bool Parse(std::span<const uint8_t> file, Image& out, std::string* error = nullptr);
// Level 0 of face 0 as tightly packed RGBA8 (for BGR8 and R8, which GPUs do not sample directly).
std::vector<uint8_t> ToRgba8(const Image& image);

}
