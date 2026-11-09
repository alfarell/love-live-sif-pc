#pragma once
// Renderer core on SDL3's GPU API (decision M3: Vulkan backend via SPIR-V shaders; D3D12 later needs DXIL builds).

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "engine/core/math.h"
#include "engine/formats/btx.h"

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUTexture;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUBuffer;
struct SDL_GPUTransferBuffer;

namespace sif {

struct Texture {
    SDL_GPUTexture* gpu = nullptr;
    uint32_t width = 0, height = 0;
    SDL_GPUTransferBuffer* staging = nullptr;  // streamed textures (CreateTexture): upload buffer, reused every update
};

// One vertex of a 3D mesh (model space).
struct MeshVertex {
    float position[3];
    float normal[3];
    float uv[2];
};

struct GpuMesh {
    SDL_GPUBuffer* vertices = nullptr;
    SDL_GPUBuffer* indices = nullptr;            // u16 triangle list
    SDL_GPUTransferBuffer* staging = nullptr;    // dynamic meshes: vertex upload buffer, reused every update
    uint32_t vertex_count = 0, index_count = 0;
};

class Renderer {
public:
    bool Init(SDL_Window* window, bool vsync, std::string* error);
    void Shutdown();

    bool Upload(const btx::Image& image, Texture& out, std::string* error);
    // A streamed RGBA8 texture (movie frames): CreateTexture once, then UpdateTexture with width x height x 4 bytes
    // whenever the picture changes. Like UpdateMesh, update outside a frame; the copy runs before the frame that uses
    // it, and the texture and upload buffer are cycled so a frame still in flight keeps the old picture.
    bool CreateTexture(uint32_t width, uint32_t height, Texture& out, std::string* error);
    bool UpdateTexture(Texture& texture, std::span<const uint8_t> rgba);
    void Release(Texture& texture);

    // `dynamic` meshes keep an upload buffer for UpdateMesh (CPU skinning). Upload and update outside a frame or
    // between frames' draws; both submit their own copy, which runs before the frame that uses it.
    bool CreateMesh(std::span<const MeshVertex> vertices, std::span<const uint16_t> indices, bool dynamic, GpuMesh& out,
                    std::string* error);
    bool UpdateMesh(GpuMesh& mesh, std::span<const MeshVertex> vertices);
    void Release(GpuMesh& mesh);

    // A frame: BeginFrame clears the window and the depth buffer. 3D draws come first (DrawMesh); the first 2D draw
    // switches to an overlay pass. 2D calls use window pixels (origin top-left); EndFrame presents.
    // BeginFrame returns false if there is nothing to draw into (e.g. minimized window); skip the frame then.
    bool BeginFrame();
    void EndFrame();
    int Width() const { return int(width_); }
    int Height() const { return int(height_); }
    void DrawTexture(const Texture& texture, float x, float y, float w, float h);
    void DrawTextureFitted(const Texture& texture);  // centered, aspect kept
    // Disc (inner = 0) or ring (inner = inner radius / radius); color is straight-alpha RGBA 0..1.
    void DrawCircle(float cx, float cy, float radius, float inner, const float color[4]);
    // 3D: `view_proj` for the following DrawMesh calls; `texture` null = white. Ignored after the first 2D draw.
    void SetCamera(const Mat4& view_proj) { view_proj_ = view_proj; }
    void DrawMesh(const GpuMesh& mesh, const Texture* texture, const Mat4& model);
    // 2D overlay: a mesh in canvas space, alpha-blended (straight alpha), no depth; `to_clip` maps it to clip space.
    void DrawUiMesh(const GpuMesh& mesh, const Texture* texture, const Mat4& to_clip, float alpha);
    // `tint`: RGBA multiplier (0..1), e.g. a UI clip's material colour; `uv_offset` shifts the texture (label atlases);
    // nothing is drawn where the mesh's own U exceeds `u_max` (fill gauges); `add` is added after the tint (RGBA 0..1)
    void DrawUiMesh(const GpuMesh& mesh, const Texture* texture, const Mat4& to_clip, const std::array<float, 4>& tint,
                    const std::array<float, 2>& uv_offset = {0, 0}, float u_max = 1e9f,
                    const std::array<float, 4>& add = {0, 0, 0, 0});

    // Clears the window and draws `texture` (if any) fitted to the window. Returns false if nothing was presented.
    bool DrawFrame(const Texture* texture);

    const char* DriverName() const { return driver_; }

private:
    void PushRect(float x, float y, float w, float h);
    bool BeginPass(bool clear);
    void Use2D();
    bool UploadBuffer(SDL_GPUBuffer* buffer, SDL_GPUTransferBuffer* staging, const void* data, uint32_t size);

    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUGraphicsPipeline* blit_ = nullptr;
    SDL_GPUGraphicsPipeline* shape_ = nullptr;
    SDL_GPUGraphicsPipeline* mesh_ = nullptr;
    SDL_GPUGraphicsPipeline* ui_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    SDL_GPUSampler* repeat_sampler_ = nullptr;  // 3D textures tile
    SDL_GPUTexture* depth_ = nullptr;
    uint32_t depth_width_ = 0, depth_height_ = 0;
    int depth_format_ = 0;  // SDL_GPUTextureFormat
    Texture white_;
    Mat4 view_proj_;
    const char* driver_ = "";
    // current frame
    SDL_GPUCommandBuffer* cmd_ = nullptr;
    SDL_GPURenderPass* pass_ = nullptr;
    SDL_GPUTexture* swapchain_ = nullptr;
    bool pass_3d_ = false;  // the current pass has the depth buffer (3D); false = 2D overlay
    uint32_t width_ = 0, height_ = 0;
};

}
