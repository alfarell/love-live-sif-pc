#include "engine/render/scene_model.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>

#include "engine/core/log.h"
#include "engine/fs/vfs.h"

namespace sif {
namespace {

std::string Lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// "stage_01.tga" / "honoka_body_01" -> "stage_01" / "honoka_body_01", lower case; texture entries are named either way
std::string TextureKey(const std::string& name) {
    std::string s = Lower(name);
    for (const char* ext : {".tga", ".btx", ".dds", ".png"}) {
        if (s.ends_with(ext)) s.resize(s.size() - std::strlen(ext));
    }
    return s;
}

template <class T> T At(std::span<const uint8_t> d, size_t off) {
    T v;
    std::memcpy(&v, &d[off], sizeof(T));
    return v;
}

}

bool SceneModel::Load(Renderer& renderer, const Vfs& vfs, const std::string& path, std::string* error,
                      const std::string& more_textures) {
    if (!vfs.Read(path, file_, error) || !bmar::Parse(file_, model_, error)) return false;

    // textures by key: embedded BMT entries first, then a texture archive next to the model
    // (chr: model.pac/texture.texs) or next to its model.pac (stage: stNNN.pac/texture.pac)
    std::unordered_map<std::string, std::string> sources;  // key -> vfs path, or "#<n>" for BMT entry n
    for (size_t i = 0; i < model_.textures.size(); ++i) sources.emplace(TextureKey(model_.textures[i].name), "#" + std::to_string(i));
    const std::string folder = path.substr(0, path.rfind('/'));
    for (const std::string& dir : {folder, folder.substr(0, folder.rfind('/')), more_textures}) {
        if (dir.empty()) continue;
        for (const char* archive : {"texture.texs", "texture.pac"}) {
            std::vector<pac::ArcEntry> entries;
            if (!vfs.List(dir + "/" + archive, entries)) continue;
            for (const pac::ArcEntry& e : entries) sources.emplace(TextureKey(e.name), dir + "/" + archive + "/" + e.name);
        }
    }
    std::unordered_map<std::string, int> loaded;
    std::vector<int> material_texture(model_.materials.size(), -1);
    for (size_t m = 0; m < model_.materials.size(); ++m) {
        const std::string key = TextureKey(model_.materials[m].texture);
        if (key.empty()) continue;  // no texture (UI text boxes, helpers)
        if (const auto it = loaded.find(key); it != loaded.end()) {
            material_texture[m] = it->second;
            continue;
        }
        const auto src = sources.find(key);
        if (src == sources.end()) {
            LogWarn("model {}: texture '{}' not found", path, model_.materials[m].texture);
            loaded[key] = -1;
            continue;
        }
        std::vector<uint8_t> tex_file;
        std::span<const uint8_t> tex;
        std::string why;
        if (src->second[0] == '#') {
            const pac::ArcEntry& e = model_.textures[std::stoul(src->second.substr(1))];
            tex = model_.texture_archive.subspan(size_t(e.offset), e.size);
        } else if (vfs.Read(src->second, tex_file, &why)) {
            tex = tex_file;
        }
        btx::Image image;
        Texture gpu;
        if (tex.empty() || !btx::Parse(tex, image, &why) || !renderer.Upload(image, gpu, &why)) {
            LogWarn("model {}: texture '{}': {}", path, model_.materials[m].texture, why);
            loaded[key] = -1;
            continue;
        }
        textures_.push_back(gpu);
        material_texture[m] = loaded[key] = int(textures_.size() - 1);
    }

    for (const bmar::Draw& d : model_.draws) {
        const bmar::Mesh& mesh = model_.meshes[d.mesh];
        const bmar::VertexLayout& l = mesh.layout;
        Part part;
        part.node = d.node;
        part.texture = material_texture[d.material];
        part.material = d.material;
        part.skinned = l.skin >= 0 && !model_.palette.empty();
        std::vector<MeshVertex> verts(mesh.vertex_count);
        if (part.skinned) {
            part.bones.resize(mesh.vertex_count);
            part.weights.resize(mesh.vertex_count);
        }
        for (uint32_t v = 0; v < mesh.vertex_count; ++v) {
            const std::span<const uint8_t> src = mesh.vertices.subspan(size_t(v) * mesh.stride, mesh.stride);
            MeshVertex& out = verts[v];
            for (int i = 0; i < 3; ++i) out.position[i] = At<float>(src, size_t(l.position) + 4 * size_t(i));
            if (l.normal >= 0) {
                for (int i = 0; i < 3; ++i) {
                    out.normal[i] = l.normal_f16 ? bmar::HalfToFloat(At<uint16_t>(src, size_t(l.normal) + 2 * size_t(i)))
                                                 : At<float>(src, size_t(l.normal) + 4 * size_t(i));
                }
            }
            if (l.uv0 >= 0) {
                for (int i = 0; i < 2; ++i) {
                    out.uv[i] = l.uv0_f16 ? bmar::HalfToFloat(At<uint16_t>(src, size_t(l.uv0) + 2 * size_t(i)))
                                          : At<float>(src, size_t(l.uv0) + 4 * size_t(i));
                }
            }
            if (part.skinned) {  // (f32x4 weights, u8x4 palette indices) twice
                for (int set = 0; set < 2; ++set) {
                    for (int i = 0; i < 4; ++i) {
                        part.weights[v][size_t(set * 4 + i)] = At<float>(src, size_t(l.skin) + 20 * size_t(set) + 4 * size_t(i));
                        uint8_t bone = src[size_t(l.skin) + 20 * size_t(set) + 16 + size_t(i)];
                        part.bones[v][size_t(set * 4 + i)] = bone < model_.palette.size() ? bone : 0;
                    }
                }
            }
        }
        if (!verts.empty()) {
            part.bounds[0] = part.bounds[2] = verts[0].position[0];
            part.bounds[1] = part.bounds[3] = verts[0].position[1];
            for (const MeshVertex& v : verts) {
                part.bounds[0] = std::min(part.bounds[0], v.position[0]);
                part.bounds[1] = std::min(part.bounds[1], v.position[1]);
                part.bounds[2] = std::max(part.bounds[2], v.position[0]);
                part.bounds[3] = std::max(part.bounds[3], v.position[1]);
            }
        }
        std::vector<uint16_t> indices(mesh.index_count);
        std::memcpy(indices.data(), mesh.indices.data(), mesh.indices.size());
        for (uint16_t& i : indices) i = std::min<uint16_t>(i, uint16_t(mesh.vertex_count - 1));
        if (!renderer.CreateMesh(verts, indices, part.skinned, part.gpu, error)) return false;
        if (part.skinned) {
            part.posed = verts;
            part.bind = std::move(verts);
        }
        parts_.push_back(std::move(part));
    }
    bmar::Pose(model_, nullptr, 0, world_);
    return true;
}

void SceneModel::Pose(Renderer& renderer, const bmar::Motion* motion, float frame) {
    const bmar::Layer layer{motion, frame};
    Pose(renderer, motion ? std::span<const bmar::Layer>(&layer, 1) : std::span<const bmar::Layer>());
}

void SceneModel::Pose(Renderer& renderer, std::span<const bmar::Layer> layers) {
    bmar::Pose(model_, layers, world_);
    skin_.resize(model_.palette.size());
    for (size_t k = 0; k < skin_.size(); ++k) skin_[k] = world_[model_.palette[k].node] * model_.palette[k].inverse_bind;
    for (Part& part : parts_) {
        if (!part.skinned) continue;
        for (size_t v = 0; v < part.bind.size(); ++v) {
            Mat4 m;
            std::fill(std::begin(m.m), std::end(m.m), 0.0f);
            for (int i = 0; i < 8; ++i) {
                const float w = part.weights[v][size_t(i)];
                if (w == 0) continue;
                const Mat4& s = skin_[part.bones[v][size_t(i)]];
                for (int e = 0; e < 16; ++e) m.m[e] += w * s.m[e];
            }
            const MeshVertex& in = part.bind[v];
            MeshVertex& out = part.posed[v];
            const Vec3 p = TransformPoint(m, {in.position[0], in.position[1], in.position[2]});
            const Vec3 n = TransformDir(m, {in.normal[0], in.normal[1], in.normal[2]});
            out.position[0] = p.x, out.position[1] = p.y, out.position[2] = p.z;
            out.normal[0] = n.x, out.normal[1] = n.y, out.normal[2] = n.z;
        }
        renderer.UpdateMesh(part.gpu, part.posed);
    }
}

void SceneModel::Draw(Renderer& renderer, const Mat4& placement) const {
    for (const Part& part : parts_) {
        // skinned vertices are already in model space; rigid meshes are stored relative to their node
        const Mat4 model = part.skinned ? placement : placement * world_[part.node];
        renderer.DrawMesh(part.gpu, part.texture >= 0 ? &textures_[size_t(part.texture)] : nullptr, model);
    }
}

void SceneModel::DrawUi(Renderer& renderer, const Mat4& to_clip, float alpha, std::span<const bmar::Layer> clips,
                        const std::set<std::string>* hidden,
                        const std::map<std::string, const Texture*>* textures,
                        const std::map<std::string, std::array<float, 3>>* uv_offsets) const {
    if (clips.empty() && (!hidden || hidden->empty()) && (!textures || textures->empty()) &&
        (!uv_offsets || uv_offsets->empty())) {
        for (const Part& part : parts_) {
            if (part.texture < 0) continue;  // text boxes and untextured helpers
            renderer.DrawUiMesh(part.gpu, &textures_[size_t(part.texture)], to_clip * world_[part.node], alpha);
        }
        return;
    }
    std::vector<Mat4> world;
    bmar::Pose(model_, clips, world);
    // a node is hidden when its own ANVS track, the caller's hidden set or an ancestor says so
    std::vector<bool> visible(model_.nodes.size(), true);
    for (size_t i = 0; i < model_.nodes.size(); ++i) {
        if (hidden && hidden->contains(model_.nodes[i].name)) visible[i] = false;
        for (const bmar::Layer& l : clips) {
            for (const bmar::PropertyTrack& t : l.motion->visibility) {
                if (t.name == model_.nodes[i].name && bmar::SampleKeys(t.keys, l.frame)[0] < 0.5f) visible[i] = false;
            }
        }
        const int parent = model_.nodes[i].parent;
        if (parent >= 0 && !visible[size_t(parent)]) visible[i] = false;
    }
    for (const Part& part : parts_) {
        // text boxes (no texture named) are not drawn; a named texture missing from the dump (e.g. one the game
        // fills at run time, like the stage preview) is drawn only when the caller supplies one
        if (model_.materials[part.material].texture.empty() || !visible[part.node]) continue;
        std::array<float, 4> tint{1, 1, 1, alpha};
        std::array<float, 2> uv{0, 0};
        float u_max = 1e9f;
        const std::string& material = model_.materials[part.material].name;
        if (uv_offsets) {
            if (const auto o = uv_offsets->find(material); o != uv_offsets->end()) {
                uv = {o->second[0], o->second[1]};
                u_max = o->second[2];
            }
        }
        for (const bmar::Layer& l : clips) {
            for (const bmar::PropertyTrack& t : l.motion->materials) {
                if (t.name != material) continue;
                const std::array<float, 4> v = bmar::SampleKeys(t.keys, l.frame);
                if (t.type == 6) {  // colour, 0-255
                    for (int c = 0; c < 3; ++c) tint[size_t(c)] = v[size_t(c)] / 255.0f;
                } else if (t.type == 4) {  // U offset (label atlas column), first component
                    uv[0] = v[0];
                } else if (t.type == 5) {  // V offset (label atlas row)
                    uv[1] = v[0];
                }
            }
        }
        const Texture* texture = part.texture >= 0 ? &textures_[size_t(part.texture)] : nullptr;
        if (textures) {
            if (const auto t = textures->find(material); t != textures->end() && t->second) texture = t->second;
        }
        if (!texture) continue;
        renderer.DrawUiMesh(part.gpu, texture, to_clip * world[part.node], tint, uv, u_max);
    }
}

std::vector<std::array<float, 4>> SceneModel::TextBoxes() const {
    std::vector<std::array<float, 4>> out;
    for (const Part& part : parts_) {
        if (!model_.materials[part.material].texture.empty()) continue;  // only meshes without a texture name
        const Vec3 a = TransformPoint(world_[part.node], {part.bounds[0], part.bounds[1], 0});
        const Vec3 b = TransformPoint(world_[part.node], {part.bounds[2], part.bounds[3], 0});
        out.push_back({std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)});
    }
    return out;
}

void SceneModel::Release(Renderer& renderer) {
    for (Part& part : parts_) renderer.Release(part.gpu);
    for (Texture& t : textures_) renderer.Release(t);
    parts_.clear();
    textures_.clear();
}

}
