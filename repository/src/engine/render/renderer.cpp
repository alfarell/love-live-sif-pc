#include "engine/render/renderer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "engine/core/log.h"

namespace sif {
namespace {

// SPIR-V words from glslc -mfmt=c (build step in CMakeLists.txt)
const uint32_t kBlitVert[] =
#include "blit.vert.inc"
    ;
const uint32_t kBlitFrag[] =
#include "blit.frag.inc"
    ;
const uint32_t kShapeFrag[] =
#include "shape.frag.inc"
    ;

bool Fail(std::string* error, const char* what) {
    if (error) *error = std::string(what) + ": " + SDL_GetError();
    return false;
}

SDL_GPUShader* MakeShader(SDL_GPUDevice* device, const uint32_t* code, size_t size, SDL_GPUShaderStage stage,
                          uint32_t samplers, uint32_t uniforms) {
    SDL_GPUShaderCreateInfo info{};
    info.code = reinterpret_cast<const Uint8*>(code);
    info.code_size = size;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_samplers = samplers;
    info.num_uniform_buffers = uniforms;
    return SDL_CreateGPUShader(device, &info);
}

}

bool Renderer::Init(SDL_Window* window, bool vsync, std::string* error) {
    window_ = window;
    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
    if (!device_) return Fail(error, "SDL_CreateGPUDevice");
    driver_ = SDL_GetGPUDeviceDriver(device_);
    if (!SDL_ClaimWindowForGPUDevice(device_, window_)) return Fail(error, "SDL_ClaimWindowForGPUDevice");
    // vsync off: prefer mailbox (no tearing, lowest latency), else immediate
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!vsync) {
        if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_MAILBOX)) mode = SDL_GPU_PRESENTMODE_MAILBOX;
        else if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_IMMEDIATE)) mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    }
    SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
    LogInfo("renderer: SDL GPU driver {}, present mode {}", driver_,
            mode == SDL_GPU_PRESENTMODE_VSYNC ? "vsync" : mode == SDL_GPU_PRESENTMODE_MAILBOX ? "mailbox" : "immediate");

    SDL_GPUShader* vs = MakeShader(device_, kBlitVert, sizeof(kBlitVert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader* fs = MakeShader(device_, kBlitFrag, sizeof(kBlitFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
    SDL_GPUShader* shape_fs = MakeShader(device_, kShapeFrag, sizeof(kShapeFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
    if (!vs || !fs || !shape_fs) return Fail(error, "SDL_CreateGPUShader");
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    SDL_GPUGraphicsPipelineCreateInfo pipe{};
    pipe.vertex_shader = vs;
    pipe.fragment_shader = fs;
    pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipe.target_info.color_target_descriptions = &target;
    pipe.target_info.num_color_targets = 1;
    blit_ = SDL_CreateGPUGraphicsPipeline(device_, &pipe);
    // shapes: straight-alpha blending over what is already drawn
    SDL_GPUColorTargetBlendState& blend = target.blend_state;
    blend.enable_blend = true;
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    pipe.fragment_shader = shape_fs;
    shape_ = SDL_CreateGPUGraphicsPipeline(device_, &pipe);
    SDL_ReleaseGPUShader(device_, vs);
    SDL_ReleaseGPUShader(device_, fs);
    SDL_ReleaseGPUShader(device_, shape_fs);
    if (!blit_ || !shape_) return Fail(error, "SDL_CreateGPUGraphicsPipeline");

    SDL_GPUSamplerCreateInfo s{};
    s.min_filter = s.mag_filter = SDL_GPU_FILTER_LINEAR;
    s.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    s.address_mode_u = s.address_mode_v = s.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    s.max_lod = 16.0f;
    sampler_ = SDL_CreateGPUSampler(device_, &s);
    if (!sampler_) return Fail(error, "SDL_CreateGPUSampler");
    return true;
}

void Renderer::Shutdown() {
    if (!device_) return;
    SDL_WaitForGPUIdle(device_);
    if (sampler_) SDL_ReleaseGPUSampler(device_, sampler_);
    if (blit_) SDL_ReleaseGPUGraphicsPipeline(device_, blit_);
    if (shape_) SDL_ReleaseGPUGraphicsPipeline(device_, shape_);
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
}

bool Renderer::Upload(const btx::Image& image, Texture& out, std::string* error) {
    // shortcut: face 0 only for cubemaps (no 3D yet); upload all 6 faces when M5 needs reflections
    std::vector<uint8_t> converted;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    uint32_t levels = uint32_t(image.levels.size());
    switch (image.format) {
        case btx::Format::BC1: format = SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM; break;
        case btx::Format::BC3: format = SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM; break;
        case btx::Format::BC7: format = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM; break;
        case btx::Format::BGRA8: break;
        case btx::Format::BGR8: case btx::Format::R8:
            converted = btx::ToRgba8(image);
            format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            levels = 1;
            break;
    }
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = image.width;
    info.height = image.height;
    info.layer_count_or_depth = 1;
    info.num_levels = levels;
    out.gpu = SDL_CreateGPUTexture(device_, &info);
    if (!out.gpu) return Fail(error, "SDL_CreateGPUTexture");
    out.width = image.width;
    out.height = image.height;

    const std::span<const uint8_t> src = converted.empty() ? image.data.first(size_t(image.chain_size)) : std::span<const uint8_t>(converted);
    SDL_GPUTransferBufferCreateInfo tb{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, uint32_t(src.size()), 0};
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device_, &tb);
    if (!transfer) return Fail(error, "SDL_CreateGPUTransferBuffer");
    std::memcpy(SDL_MapGPUTransferBuffer(device_, transfer, false), src.data(), src.size());
    SDL_UnmapGPUTransferBuffer(device_, transfer);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    for (uint32_t i = 0; i < levels; ++i) {
        const btx::Level& l = image.levels[i];
        SDL_GPUTextureTransferInfo from{};
        from.transfer_buffer = transfer;
        from.offset = converted.empty() ? uint32_t(l.offset) : 0;
        if (converted.empty()) {
            from.pixels_per_row = btx::IsBlockFormat(image.format) ? l.pitch / (image.format == btx::Format::BC1 ? 8 : 16) * 4 : l.pitch / 4;
        }
        SDL_GPUTextureRegion to{};
        to.texture = out.gpu;
        to.mip_level = i;
        to.w = l.width;
        to.h = l.height;
        to.d = 1;
        SDL_UploadToGPUTexture(copy, &from, &to, false);
    }
    SDL_EndGPUCopyPass(copy);
    const bool ok = SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device_, transfer);
    return ok || Fail(error, "SDL_SubmitGPUCommandBuffer");
}

void Renderer::Release(Texture& texture) {
    if (texture.gpu) SDL_ReleaseGPUTexture(device_, texture.gpu);
    texture = {};
}

bool Renderer::BeginFrame() {
    cmd_ = SDL_AcquireGPUCommandBuffer(device_);
    if (!cmd_) return false;
    SDL_GPUTexture* swap = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd_, window_, &swap, &width_, &height_) || !swap) {
        SDL_SubmitGPUCommandBuffer(cmd_);
        cmd_ = nullptr;
        return false;
    }
    SDL_GPUColorTargetInfo target{};
    target.texture = swap;
    target.clear_color = {0.05f, 0.05f, 0.07f, 1.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    pass_ = SDL_BeginGPURenderPass(cmd_, &target, 1, nullptr);
    return true;
}

void Renderer::EndFrame() {
    if (!cmd_) return;
    SDL_EndGPURenderPass(pass_);
    SDL_SubmitGPUCommandBuffer(cmd_);
    cmd_ = nullptr;
    pass_ = nullptr;
}

void Renderer::PushRect(float x, float y, float w, float h) {
    // window pixels -> NDC (+y up): top-left, bottom-right
    const float rect[4] = {2 * x / float(width_) - 1, 1 - 2 * y / float(height_), 2 * (x + w) / float(width_) - 1,
                           1 - 2 * (y + h) / float(height_)};
    SDL_PushGPUVertexUniformData(cmd_, 0, rect, sizeof(rect));
}

void Renderer::DrawTexture(const Texture& texture, float x, float y, float w, float h) {
    if (!pass_ || !texture.gpu) return;
    SDL_BindGPUGraphicsPipeline(pass_, blit_);
    PushRect(x, y, w, h);
    SDL_GPUTextureSamplerBinding binding{texture.gpu, sampler_};
    SDL_BindGPUFragmentSamplers(pass_, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass_, 4, 1, 0, 0);
}

void Renderer::DrawTextureFitted(const Texture& texture) {
    if (!texture.gpu) return;
    const float scale = std::min(float(width_) / float(texture.width), float(height_) / float(texture.height));
    const float w = float(texture.width) * scale, h = float(texture.height) * scale;
    DrawTexture(texture, (float(width_) - w) / 2, (float(height_) - h) / 2, w, h);
}

void Renderer::DrawCircle(float cx, float cy, float radius, float inner, const float color[4]) {
    if (!pass_ || radius <= 0) return;
    SDL_BindGPUGraphicsPipeline(pass_, shape_);
    PushRect(cx - radius, cy - radius, 2 * radius, 2 * radius);
    const float shape[8] = {color[0], color[1], color[2], color[3], inner, 1.5f / radius, 0, 0};
    SDL_PushGPUFragmentUniformData(cmd_, 0, shape, sizeof(shape));
    SDL_DrawGPUPrimitives(pass_, 4, 1, 0, 0);
}

bool Renderer::DrawFrame(const Texture* texture) {
    if (!BeginFrame()) return false;
    if (texture) DrawTextureFitted(*texture);
    EndFrame();
    return true;
}

}
