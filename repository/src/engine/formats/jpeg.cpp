#include "engine/formats/jpeg.h"

#include <climits>

#include "stb_image.h"

namespace sif::jpeg {

bool Decode(std::span<const uint8_t> file, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height,
            std::string* error) {
    int w = 0, h = 0, channels = 0;
    if (file.size() > size_t(INT_MAX)) {
        if (error) *error = "jpeg: file too large";
        return false;
    }
    stbi_uc* pixels = stbi_load_from_memory(file.data(), int(file.size()), &w, &h, &channels, 4);
    if (!pixels) {
        if (error) *error = std::string("jpeg: ") + stbi_failure_reason();
        return false;
    }
    rgba.assign(pixels, pixels + size_t(w) * size_t(h) * 4);
    stbi_image_free(pixels);
    width = uint32_t(w), height = uint32_t(h);
    return true;
}

}
