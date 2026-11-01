#pragma once
// SIF `BMAR104` models and motions (spec: docs/formats/bmar.md). Model data is Z-up; node 0 (bml_axisNode) turns it
// Y-up and is skipped here, as in tools/bmar_dump.py. Parsed spans point into the caller's file buffer.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "engine/core/math.h"
#include "engine/fs/pac.h"

namespace sif::bmar {

struct Node {
    std::string name;
    int parent = -1;
    float t[3]{}, s[3]{1, 1, 1}, r[3]{};  // translate, scale, Euler rotation (R = Rz * Ry * Rx)
};

// Byte offsets of the attributes inside one vertex, -1 when absent (vertex format bits, docs/formats/bmar.md).
struct VertexLayout {
    int position = -1, skin = -1, normal = -1, uv0 = -1;
    bool normal_f16 = false, uv0_f16 = false;
    uint32_t stride = 0;
};
VertexLayout LayoutOf(uint32_t format, uint32_t extra4);
float HalfToFloat(uint16_t h);  // f16 normals and UVs

struct Mesh {
    std::string name;
    uint32_t format = 0, stride = 0, vertex_count = 0, index_count = 0;
    VertexLayout layout;
    std::span<const uint8_t> vertices, indices;  // indices: u16 triangle list
};

struct Draw {
    uint32_t node = 0, mesh = 0, material = 0;
};

struct Material {
    std::string name, texture;
};

struct PaletteEntry {
    uint32_t node = 0;
    Mat4 inverse_bind;
};

struct Model {
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Draw> draws;
    std::vector<Material> materials;
    std::vector<PaletteEntry> palette;           // MTXP skin palette (empty for rigid models)
    std::span<const uint8_t> texture_archive;    // BMT: ARC table of embedded btx textures (empty if none)
    std::vector<pac::ArcEntry> textures;         // its entries
};

bool Parse(std::span<const uint8_t> file, Model& out, std::string* error = nullptr);

struct Key {
    float frame = 0;
    std::array<float, 4> v{};  // translate/scale: x, y, z; rotate: quaternion x, y, z, w
    bool step = false;         // key flag 2: hold this value until the next key (Hypothesis; ANVS uses it throughout)
};

struct Track {
    std::string node;
    std::array<std::vector<Key>, 3> channels;  // translate, rotate, scale (empty = keep the bind value)
};

// ANMT (material tracks, by material name) and ANVS (visibility, by node name); one channel per entry.
// Types: ANMT 6 colour RGB 0-255, 4 transparency RGB 0-1 (0 = opaque), 5 RGB 0-1 (Hypothesis: incandescence);
// ANVS 11 visible 0/1 (docs/formats/bmar.md).
struct PropertyTrack {
    std::string name;
    uint32_t type = 0;
    std::vector<Key> keys;
};

struct Motion {
    float fps = 60, frames = 0;
    std::vector<Track> tracks;
    std::vector<PropertyTrack> materials, visibility;
};

// A channel's value at `frame` (linear between keys, held after step keys, clamped at both ends).
std::array<float, 4> SampleKeys(const std::vector<Key>& keys, float frame);

bool ParseMotion(std::span<const uint8_t> file, Motion& out, std::string* error = nullptr);

// World matrices of all nodes (model space, Z-up) in the bind pose or posed by `motion` at `frame`.
void Pose(const Model& model, const Motion* motion, float frame, std::vector<Mat4>& world);

// Several motions at their own frames; for a node animated by more than one, the last layer wins.
struct Layer {
    const Motion* motion = nullptr;
    float frame = 0;
};
void Pose(const Model& model, std::span<const Layer> layers, std::vector<Mat4>& world);

}
