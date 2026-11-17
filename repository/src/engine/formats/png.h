#pragma once
// A minimal PNG writer (RGBA8, one zlib stream) for the photos of View Live Show (M12 P4c). zlib is already linked
// for the dump's cmp\0zlib archives.

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace sif::png {

// width x height RGBA8 pixels, rows top to bottom
std::vector<uint8_t> Encode(std::span<const uint8_t> rgba, uint32_t width, uint32_t height);
bool Write(const std::filesystem::path& path, std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
           std::string* error = nullptr);
// Reads a PNG this writer made (8-bit RGBA, not interlaced; filter types 0-4 are undone). Other PNGs fail.
bool Read(const std::filesystem::path& path, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height,
          std::string* error = nullptr);

}
