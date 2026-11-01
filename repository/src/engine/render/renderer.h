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
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;

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

    // A frame: BeginFrame clears the window; draw calls use window pixels (origin top-left); EndFrame presents.
    // BeginFrame returns false if there is nothing to draw into (e.g. minimized window); skip the frame then.
    bool BeginFrame();
    void EndFrame();
    int Width() const { return int(width_); }
    int Height() const { return int(height_); }
    void DrawTexture(const Texture& texture, float x, float y, float w, float h);
    void DrawTextureFitted(const Texture& texture);  // centered, aspect kept
    // Disc (inner = 0) or ring (inner = inner radius / radius); color is straight-alpha RGBA 0..1.
    void DrawCircle(float cx, float cy, float radius, float inner, const float color[4]);

    // Clears the window and draws `texture` (if any) fitted to the window. Returns false if nothing was presented.
    bool DrawFrame(const Texture* texture);

    const char* DriverName() const { return driver_; }

private:
    void PushRect(float x, float y, float w, float h);

    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUGraphicsPipeline* blit_ = nullptr;
    SDL_GPUGraphicsPipeline* shape_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    const char* driver_ = "";
    // current frame
    SDL_GPUCommandBuffer* cmd_ = nullptr;
    SDL_GPURenderPass* pass_ = nullptr;
    uint32_t width_ = 0, height_ = 0;
};

}
