#pragma once
// JPEG decoding through stb_image (third_party/stb, JPEG only; M12 P7: the NPC profile cards' pictures).

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sif::jpeg {

// RGBA8, rows top first; false (with `error`) if it is not a JPEG stb_image can read
bool Decode(std::span<const uint8_t> file, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height,
            std::string* error = nullptr);

}
