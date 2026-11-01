#include "engine/render/renderer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
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
const uint32_t kMeshVert[] =
#include "mesh.vert.inc"
    ;
const uint32_t kMeshFrag[] =
#include "mesh.frag.inc"
    ;
const uint32_t kUiFrag[] =
#include "ui.frag.inc"
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

    // 3D meshes: depth-tested, opaque with alpha test (mesh.frag)
    depth_format_ = SDL_GPUTextureSupportsFormat(device_, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D,
                                                 SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)
                        ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT
                        : SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    SDL_GPUShader* mesh_vs = MakeShader(device_, kMeshVert, sizeof(kMeshVert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader* mesh_fs = MakeShader(device_, kMeshFrag, sizeof(kMeshFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
    if (!mesh_vs || !mesh_fs) return Fail(error, "SDL_CreateGPUShader (mesh)");
    SDL_GPUVertexBufferDescription vb{0, sizeof(MeshVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
    const SDL_GPUVertexAttribute attrs[3] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, uint32_t(offsetof(MeshVertex, position))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, uint32_t(offsetof(MeshVertex, normal))},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, uint32_t(offsetof(MeshVertex, uv))}};
    SDL_GPUColorTargetDescription opaque{};
    opaque.format = target.format;
    SDL_GPUGraphicsPipelineCreateInfo mesh{};
    mesh.vertex_shader = mesh_vs;
    mesh.fragment_shader = mesh_fs;
    mesh.vertex_input_state.vertex_buffer_descriptions = &vb;
    mesh.vertex_input_state.num_vertex_buffers = 1;
    mesh.vertex_input_state.vertex_attributes = attrs;
    mesh.vertex_input_state.num_vertex_attributes = 3;
    mesh.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    mesh.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;  // shortcut: winding not confirmed (bmar.md), draw both sides
    mesh.depth_stencil_state.enable_depth_test = true;
    mesh.depth_stencil_state.enable_depth_write = true;
    mesh.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    mesh.target_info.color_target_descriptions = &opaque;
    mesh.target_info.num_color_targets = 1;
    mesh.target_info.has_depth_stencil_target = true;
    mesh.target_info.depth_stencil_format = SDL_GPUTextureFormat(depth_format_);
    mesh_ = SDL_CreateGPUGraphicsPipeline(device_, &mesh);
    // UI quads: same vertices, blended like the 2D shapes, in the overlay pass (no depth target)
    SDL_GPUShader* ui_fs = MakeShader(device_, kUiFrag, sizeof(kUiFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    mesh.fragment_shader = ui_fs;
    mesh.depth_stencil_state = {};
    mesh.target_info.color_target_descriptions = &target;  // blending set up for the shape pipeline above
    mesh.target_info.has_depth_stencil_target = false;
    ui_ = ui_fs ? SDL_CreateGPUGraphicsPipeline(device_, &mesh) : nullptr;
    SDL_ReleaseGPUShader(device_, mesh_vs);
    SDL_ReleaseGPUShader(device_, mesh_fs);
    if (ui_fs) SDL_ReleaseGPUShader(device_, ui_fs);
    if (!mesh_ || !ui_) return Fail(error, "SDL_CreateGPUGraphicsPipeline (mesh/ui)");

    SDL_GPUSamplerCreateInfo s{};
    s.min_filter = s.mag_filter = SDL_GPU_FILTER_LINEAR;
    s.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    s.address_mode_u = s.address_mode_v = s.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    s.max_lod = 16.0f;
    sampler_ = SDL_CreateGPUSampler(device_, &s);
    s.address_mode_u = s.address_mode_v = s.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    repeat_sampler_ = SDL_CreateGPUSampler(device_, &s);
    if (!sampler_ || !repeat_sampler_) return Fail(error, "SDL_CreateGPUSampler");

    static const uint8_t kWhite[4] = {255, 255, 255, 255};
    btx::Image white;
    white.width = white.height = 1;
    white.levels = {{1, 1, 4, 0, 4}};
    white.data = kWhite;
    white.chain_size = 4;
    return Upload(white, white_, error);
}

void Renderer::Shutdown() {
    if (!device_) return;
    SDL_WaitForGPUIdle(device_);
    Release(white_);
    if (depth_) SDL_ReleaseGPUTexture(device_, depth_);
    if (sampler_) SDL_ReleaseGPUSampler(device_, sampler_);
    if (repeat_sampler_) SDL_ReleaseGPUSampler(device_, repeat_sampler_);
    if (blit_) SDL_ReleaseGPUGraphicsPipeline(device_, blit_);
    if (shape_) SDL_ReleaseGPUGraphicsPipeline(device_, shape_);
    if (mesh_) SDL_ReleaseGPUGraphicsPipeline(device_, mesh_);
    if (ui_) SDL_ReleaseGPUGraphicsPipeline(device_, ui_);
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
    swapchain_ = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd_, window_, &swapchain_, &width_, &height_) || !swapchain_) {
        SDL_SubmitGPUCommandBuffer(cmd_);
        cmd_ = nullptr;
        return false;
    }
    if (!depth_ || depth_width_ != width_ || depth_height_ != height_) {
        if (depth_) SDL_ReleaseGPUTexture(device_, depth_);
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPUTextureFormat(depth_format_);
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        info.width = width_;
        info.height = height_;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        depth_ = SDL_CreateGPUTexture(device_, &info);
        depth_width_ = width_;
        depth_height_ = height_;
        if (!depth_) LogError("renderer: depth buffer {}x{}: {}", width_, height_, SDL_GetError());
    }
    return BeginPass(true);
}

bool Renderer::BeginPass(bool three_d) {
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain_;
    target.clear_color = {0.05f, 0.05f, 0.07f, 1.0f};
    target.load_op = three_d ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = depth_;
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    pass_3d_ = three_d && depth_;
    pass_ = SDL_BeginGPURenderPass(cmd_, &target, 1, pass_3d_ ? &depth : nullptr);
    return pass_ != nullptr;
}

void Renderer::Use2D() {
    if (!pass_ || !pass_3d_) return;
    SDL_EndGPURenderPass(pass_);
    BeginPass(false);
}

void Renderer::EndFrame() {
    if (!cmd_) return;
    if (pass_) SDL_EndGPURenderPass(pass_);
    SDL_SubmitGPUCommandBuffer(cmd_);
    cmd_ = nullptr;
    pass_ = nullptr;
}

bool Renderer::UploadBuffer(SDL_GPUBuffer* buffer, SDL_GPUTransferBuffer* staging, const void* data, uint32_t size) {
    void* dst = SDL_MapGPUTransferBuffer(device_, staging, true);  // cycle: the previous upload may still be in flight
    if (!dst) return false;
    std::memcpy(dst, data, size);
    SDL_UnmapGPUTransferBuffer(device_, staging);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
    if (!cmd) return false;
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation from{staging, 0};
    SDL_GPUBufferRegion to{buffer, 0, size};
    SDL_UploadToGPUBuffer(copy, &from, &to, true);
    SDL_EndGPUCopyPass(copy);
    return SDL_SubmitGPUCommandBuffer(cmd);
}

bool Renderer::CreateMesh(std::span<const MeshVertex> vertices, std::span<const uint16_t> indices, bool dynamic,
                          GpuMesh& out, std::string* error) {
    out = {};
    const uint32_t vsize = uint32_t(vertices.size_bytes()), isize = uint32_t(indices.size_bytes());
    if (vsize == 0 || isize == 0) {
        if (error) *error = "empty mesh";
        return false;
    }
    SDL_GPUBufferCreateInfo vb{SDL_GPU_BUFFERUSAGE_VERTEX, vsize, 0};
    SDL_GPUBufferCreateInfo ib{SDL_GPU_BUFFERUSAGE_INDEX, isize, 0};
    out.vertices = SDL_CreateGPUBuffer(device_, &vb);
    out.indices = SDL_CreateGPUBuffer(device_, &ib);
    SDL_GPUTransferBufferCreateInfo tb{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, std::max(vsize, isize), 0};
    SDL_GPUTransferBuffer* staging = SDL_CreateGPUTransferBuffer(device_, &tb);
    out.vertex_count = uint32_t(vertices.size());
    out.index_count = uint32_t(indices.size());
    const bool ok = out.vertices && out.indices && staging && UploadBuffer(out.vertices, staging, vertices.data(), vsize) &&
                    UploadBuffer(out.indices, staging, indices.data(), isize);
    if (dynamic && ok) {
        out.staging = staging;
    } else if (staging) {
        SDL_ReleaseGPUTransferBuffer(device_, staging);
    }
    if (!ok) {
        Release(out);
        return Fail(error, "CreateMesh");
    }
    return true;
}

bool Renderer::UpdateMesh(GpuMesh& mesh, std::span<const MeshVertex> vertices) {
    if (!mesh.staging || vertices.size() != mesh.vertex_count) return false;
    return UploadBuffer(mesh.vertices, mesh.staging, vertices.data(), uint32_t(vertices.size_bytes()));
}

void Renderer::Release(GpuMesh& mesh) {
    if (mesh.vertices) SDL_ReleaseGPUBuffer(device_, mesh.vertices);
    if (mesh.indices) SDL_ReleaseGPUBuffer(device_, mesh.indices);
    if (mesh.staging) SDL_ReleaseGPUTransferBuffer(device_, mesh.staging);
    mesh = {};
}

void Renderer::DrawMesh(const GpuMesh& mesh, const Texture* texture, const Mat4& model) {
    if (!pass_ || !pass_3d_ || !mesh.vertices) return;
    SDL_BindGPUGraphicsPipeline(pass_, mesh_);
    struct {
        Mat4 mvp, model;
    } transform{view_proj_ * model, model};
    SDL_PushGPUVertexUniformData(cmd_, 0, &transform, sizeof(transform));
    SDL_GPUTextureSamplerBinding binding{(texture && texture->gpu ? texture : &white_)->gpu, repeat_sampler_};
    SDL_BindGPUFragmentSamplers(pass_, 0, &binding, 1);
    SDL_GPUBufferBinding vb{mesh.vertices, 0}, ib{mesh.indices, 0};
    SDL_BindGPUVertexBuffers(pass_, 0, &vb, 1);
    SDL_BindGPUIndexBuffer(pass_, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(pass_, mesh.index_count, 1, 0, 0, 0);
}

void Renderer::PushRect(float x, float y, float w, float h) {
    // window pixels -> NDC (+y up): top-left, bottom-right
    const float rect[4] = {2 * x / float(width_) - 1, 1 - 2 * y / float(height_), 2 * (x + w) / float(width_) - 1,
                           1 - 2 * (y + h) / float(height_)};
    SDL_PushGPUVertexUniformData(cmd_, 0, rect, sizeof(rect));
}

void Renderer::DrawUiMesh(const GpuMesh& mesh, const Texture* texture, const Mat4& to_clip, float alpha) {
    if (!pass_ || !mesh.vertices || alpha <= 0) return;
    Use2D();
    SDL_BindGPUGraphicsPipeline(pass_, ui_);
    struct {
        Mat4 mvp, model;
    } transform{to_clip, Mat4{}};
    SDL_PushGPUVertexUniformData(cmd_, 0, &transform, sizeof(transform));
    const float tint[4] = {1, 1, 1, alpha};
    SDL_PushGPUFragmentUniformData(cmd_, 0, tint, sizeof(tint));
    SDL_GPUTextureSamplerBinding binding{(texture && texture->gpu ? texture : &white_)->gpu, sampler_};
    SDL_BindGPUFragmentSamplers(pass_, 0, &binding, 1);
    SDL_GPUBufferBinding vb{mesh.vertices, 0}, ib{mesh.indices, 0};
    SDL_BindGPUVertexBuffers(pass_, 0, &vb, 1);
    SDL_BindGPUIndexBuffer(pass_, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(pass_, mesh.index_count, 1, 0, 0, 0);
}

void Renderer::DrawTexture(const Texture& texture, float x, float y, float w, float h) {
    if (!pass_ || !texture.gpu) return;
    Use2D();
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
    Use2D();
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
