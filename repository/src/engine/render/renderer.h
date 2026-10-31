#pragma once
// Renderer core on SDL3's GPU API (decision M3: Vulkan backend via SPIR-V shaders; D3D12 later needs DXIL builds).

#include <cstdint>
#include <string>

#include "engine/formats/btx.h"

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUTexture;

namespace sif {

struct Texture {
    SDL_GPUTexture* gpu = nullptr;
    uint32_t width = 0, height = 0;
};

class Renderer {
public:
    bool Init(SDL_Window* window, bool vsync, std::string* error);
    void Shutdown();

    bool Upload(const btx::Image& image, Texture& out, std::string* error);
    void Release(Texture& texture);

    // Clears the window and draws `texture` (if any) fitted to the window with its aspect ratio kept.
    // Returns false if the frame could not be presented (e.g. minimized window).
    bool DrawFrame(const Texture* texture);

    const char* DriverName() const { return driver_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUGraphicsPipeline* blit_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    const char* driver_ = "";
};

}
