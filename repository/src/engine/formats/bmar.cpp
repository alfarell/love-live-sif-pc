#include "engine/formats/bmar.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace sif::bmar {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

// Bounds-checked little-endian reads from one BML/BMA part; any read out of range clears `ok`.
struct Reader {
    std::span<const uint8_t> p;
    bool ok = true;
    template <class T> T At(uint64_t off) {
        T v{};
        if (off + sizeof(T) > p.size()) {
            ok = false;
            return v;
        }
        std::memcpy(&v, &p[size_t(off)], sizeof(T));
        return v;
    }
    std::string Str(uint64_t off) {
        if (off >= p.size()) {
            ok = false;
            return {};
        }
        const char* s = reinterpret_cast<const char*>(&p[size_t(off)]);
        return std::string(s, strnlen(s, p.size() - size_t(off)));
    }
    std::span<const uint8_t> Bytes(uint64_t off, uint64_t size) {
        if (off + size > p.size()) {
            ok = false;
            return {};
        }
        return p.subspan(size_t(off), size_t(size));
    }
};

struct Chunk {
    std::string tag;
    uint64_t body = 0;  // offset of the body in the part
};

// char[4] tag, u32 body size, u32 0x00040000, body padded to 4; ends at a zero tag
std::vector<Chunk> Chunks(std::span<const uint8_t> p) {
    std::vector<Chunk> out;
    for (size_t pos = 0; pos + 12 <= p.size();) {
        if (p[pos] == 0 && p[pos + 1] == 0 && p[pos + 2] == 0 && p[pos + 3] == 0) break;
        uint32_t size;
        std::memcpy(&size, &p[pos + 4], 4);
        const char* tag = reinterpret_cast<const char*>(&p[pos]);
        out.push_back({std::string(tag, strnlen(tag, 4)), pos + 12});
        pos = (pos + 12 + size + 3) & ~size_t(3);
    }
    return out;
}

// The parts of a BMAR file ("BMAR104\0" + ARC table), by tag.
bool Parts(std::span<const uint8_t> file, std::unordered_map<std::string, std::span<const uint8_t>>& out, std::string* error) {
    if (file.size() < 16 || std::memcmp(file.data(), "BMAR104", 8) != 0) return Fail(error, "not a BMAR104 file");
    const std::span<const uint8_t> table = file.subspan(8);
    std::vector<pac::ArcEntry> entries;
    if (!pac::ParseArc(table, entries, error)) return false;
    for (const pac::ArcEntry& e : entries) {
        if (e.offset + e.size > table.size()) return Fail(error, "BMAR part out of range");
        out[e.tag] = table.subspan(size_t(e.offset), e.size);
    }
    return true;
}

}

float HalfToFloat(uint16_t h) {
    const uint32_t sign = uint32_t(h & 0x8000) << 16, exp = (h >> 10) & 0x1F, man = h & 0x3FF;
    uint32_t bits;
    if (exp == 0) {
        if (man == 0) {
            bits = sign;
        } else {  // subnormal
            float f = std::ldexp(float(man), -24);
            std::memcpy(&bits, &f, 4);
            bits |= sign;
        }
    } else if (exp == 31) {
        bits = sign | 0x7F800000 | man << 13;
    } else {
        bits = sign | (exp + 112) << 23 | man << 13;
    }
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

namespace {

std::array<float, 4> Sample(const std::vector<Key>& keys, float frame, bool quat) {
    if (frame <= keys.front().frame) return keys.front().v;
    if (frame >= keys.back().frame) return keys.back().v;
    const auto hi = std::upper_bound(keys.begin(), keys.end(), frame, [](float f, const Key& k) { return f < k.frame; });
    const Key& a = *(hi - 1);
    const Key& b = *hi;
    if (a.step) return a.v;
    const float t = b.frame > a.frame ? (frame - a.frame) / (b.frame - a.frame) : 0.0f;
    std::array<float, 4> v{};
    // shortcut: all keys are linear (normalized lerp for rotations); flags 1/2 are in 10 channels of 319 files
    const float sign = quat && a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] + a.v[3] * b.v[3] < 0 ? -1.0f : 1.0f;
    for (int i = 0; i < 4; ++i) v[size_t(i)] = a.v[size_t(i)] + (sign * b.v[size_t(i)] - a.v[size_t(i)]) * t;
    if (quat) {
        const float n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
        if (n > 0) {
            for (float& x : v) x /= n;
        }
    }
    return v;
}

}

VertexLayout LayoutOf(uint32_t format, uint32_t extra4) {
    // storage order and sizes from all 22 format/stride pairs in the game (docs/formats/bmar.md)
    struct Attr { uint32_t bit; int size; int* field; bool* f16; bool f16_value; };
    VertexLayout l;
    int unused = 0;
    const Attr attrs[] = {
        {0x00000001, 12, &l.position, nullptr, false}, {0x40000000, 40, &l.skin, nullptr, false},
        {0x00000004, 12, &l.normal, &l.normal_f16, false}, {0x00000008, 8, &l.normal, &l.normal_f16, true},
        {0x00000020, 4, &unused, nullptr, false},
        {0x00000100, 8, &l.uv0, &l.uv0_f16, false}, {0x00000400, 4, &l.uv0, &l.uv0_f16, true},
        {0x00001000, 8, &unused, nullptr, false}, {0x00004000, 4, &unused, nullptr, false},
        {0x00040000, 12, &unused, nullptr, false}, {0x00080000, 8, &unused, nullptr, false},
        {0x00200000, 12, &unused, nullptr, false}, {0x00400000, 8, &unused, nullptr, false},
        {0x10000000, 16, &unused, nullptr, false}};
    int pos = 0;
    for (const Attr& a : attrs) {
        if (!(format & a.bit)) continue;
        *a.field = pos;
        if (a.f16) *a.f16 = a.f16_value;
        pos += a.size;
    }
    l.stride = uint32_t(pos) + 4 * extra4;
    return l;
}

bool Parse(std::span<const uint8_t> file, Model& out, std::string* error) {
    out = {};
    std::unordered_map<std::string, std::span<const uint8_t>> parts;
    if (!Parts(file, parts, error)) return false;
    if (!parts.contains("BML")) return Fail(error, "BMAR file has no BML part");
    Reader r{parts["BML"]};
    for (const Chunk& c : Chunks(r.p)) {
        const uint64_t b = c.body;
        if (c.tag == "NODE") {
            Node n;
            n.name = r.Str(r.At<uint64_t>(b));
            for (int i = 0; i < 3; ++i) {
                n.t[i] = r.At<float>(b + 0x10 + 4 * i);
                n.s[i] = r.At<float>(b + 0x1C + 4 * i);
                n.r[i] = r.At<float>(b + 0x28 + 4 * i);
            }
            n.parent = r.At<int32_t>(b + 0x60);
            out.nodes.push_back(std::move(n));
        } else if (c.tag == "MESH") {
            Mesh m;
            m.name = r.Str(r.At<uint64_t>(b));
            const uint64_t vb = r.At<uint64_t>(b + 8), ib = r.At<uint64_t>(b + 0x10);
            m.format = r.At<uint32_t>(b + 0x48);
            const uint32_t extra4 = r.At<uint32_t>(b + 0x4C);
            m.index_count = r.At<uint32_t>(b + 0x50);
            m.vertex_count = r.At<uint32_t>(b + 0x54);
            m.stride = r.At<uint16_t>(b + 0x5A);
            m.layout = LayoutOf(m.format, extra4);
            if (m.layout.stride != m.stride || m.layout.position < 0) {
                return Fail(error, "mesh " + m.name + ": vertex format does not match its stride");
            }
            m.vertices = r.Bytes(vb, uint64_t(m.vertex_count) * m.stride);
            m.indices = r.Bytes(ib, uint64_t(m.index_count) * 2);
            out.meshes.push_back(std::move(m));
        } else if (c.tag == "DRAW") {
            out.draws.push_back({r.At<uint32_t>(b), r.At<uint32_t>(b + 4), r.At<uint32_t>(b + 8)});
        } else if (c.tag == "MATE") {
            out.materials.push_back({r.Str(r.At<uint64_t>(b)), r.Str(r.At<uint64_t>(b + 8))});
        } else if (c.tag == "MTXP") {
            const uint64_t list = r.At<uint64_t>(b), mats = r.At<uint64_t>(b + 8);
            const uint16_t count = r.At<uint16_t>(b + 0x12);
            for (uint32_t k = 0; k < count; ++k) {
                PaletteEntry e;
                e.node = r.At<uint32_t>(list + 4 * k);
                for (int i = 0; i < 16; ++i) e.inverse_bind.m[i] = r.At<float>(mats + 64 * k + 4 * i);  // column-major
                out.palette.push_back(e);
            }
        }
    }
    if (!r.ok) return Fail(error, "BML data out of range");
    for (const Draw& d : out.draws) {
        if (d.node >= out.nodes.size() || d.mesh >= out.meshes.size() || d.material >= out.materials.size()) {
            return Fail(error, "DRAW index out of range");
        }
    }
    for (size_t i = 0; i < out.nodes.size(); ++i) {
        if (out.nodes[i].parent >= int(i)) return Fail(error, "node parent after its child");
    }
    for (const PaletteEntry& e : out.palette) {
        if (e.node >= out.nodes.size()) return Fail(error, "MTXP node out of range");
    }
    if (parts.contains("BMT")) {
        const std::vector<Chunk> chunks = Chunks(parts["BMT"]);
        if (!chunks.empty() && chunks[0].tag == "BMT") {
            out.texture_archive = parts["BMT"].subspan(size_t(chunks[0].body));
            if (!pac::ParseArc(out.texture_archive, out.textures, error)) return false;
        }
    }
    return true;
}

namespace {

// ANMT / ANVS: u64 table*, u16 count; records (name*, channel table*, u32 hash, u32 channel count) of 24 bytes;
// channels as in ANSK (keys*, values*, u32 key count, u32 type); values are f32 x 1 (ANVS) or x 3 (ANMT)
void ParseProperties(Reader& r, uint64_t body, int default_width, std::vector<PropertyTrack>& out) {
    const uint64_t table = r.At<uint64_t>(body);
    const uint16_t count = r.At<uint16_t>(body + 8);
    for (uint32_t i = 0; i < count && r.ok; ++i) {
        const uint64_t t = table + 24 * uint64_t(i);
        const std::string name = r.Str(r.At<uint64_t>(t));
        const uint64_t chans = r.At<uint64_t>(t + 8);
        const uint32_t nchan = r.At<uint32_t>(t + 20);
        for (uint32_t j = 0; j < nchan && r.ok; ++j) {
            const uint64_t c = chans + 24 * uint64_t(j);
            const uint64_t keys = r.At<uint64_t>(c), values = r.At<uint64_t>(c + 8);
            const uint32_t nkeys = r.At<uint32_t>(c + 16);
            PropertyTrack track{name, r.At<uint32_t>(c + 20), std::vector<Key>(nkeys)};
            // the value width follows from the key offsets (a few ANMT channels store one float, replicated here)
            int width = default_width;
            if (nkeys > 1) width = std::clamp(int(r.At<uint32_t>(keys + 16) - r.At<uint32_t>(keys + 4)) / 4, 1, 4);
            for (uint32_t k = 0; k < nkeys; ++k) {
                Key& key = track.keys[k];
                key.frame = r.At<float>(keys + 12 * uint64_t(k));
                const uint32_t off = r.At<uint32_t>(keys + 12 * uint64_t(k) + 4);
                key.step = r.At<uint32_t>(keys + 12 * uint64_t(k) + 8) == 2;
                for (int v = 0; v < 4; ++v) key.v[size_t(v)] = r.At<float>(values + off + 4 * uint64_t(std::min(v, width - 1)));
            }
            if (nkeys) out.push_back(std::move(track));
        }
    }
}

}

std::array<float, 4> SampleKeys(const std::vector<Key>& keys, float frame) {
    return keys.empty() ? std::array<float, 4>{} : Sample(keys, frame, false);
}

bool ParseMotion(std::span<const uint8_t> file, Motion& out, std::string* error) {
    out = {};
    std::unordered_map<std::string, std::span<const uint8_t>> parts;
    if (!Parts(file, parts, error)) return false;
    if (!parts.contains("BMA")) return Fail(error, "BMAR file has no BMA part");
    Reader r{parts["BMA"]};
    uint64_t table = 0;
    uint16_t count = 0;
    for (const Chunk& c : Chunks(r.p)) {
        if (c.tag == "ANHD") {
            out.fps = r.At<float>(c.body + 4);
            out.frames = r.At<float>(c.body + 8);
        } else if (c.tag == "ANSK") {
            table = r.At<uint64_t>(c.body);
            count = r.At<uint16_t>(c.body + 8);
        } else if (c.tag == "ANMT") {
            ParseProperties(r, c.body, 3, out.materials);
        } else if (c.tag == "ANVS") {
            ParseProperties(r, c.body, 1, out.visibility);
        }
    }
    for (uint32_t i = 0; i < count && r.ok; ++i) {
        const uint64_t t = table + 24 * uint64_t(i);
        Track track;
        track.node = r.Str(r.At<uint64_t>(t));
        const uint64_t chans = r.At<uint64_t>(t + 8);
        const uint32_t nchan = r.At<uint32_t>(t + 20);
        for (uint32_t j = 0; j < nchan && r.ok; ++j) {
            const uint64_t c = chans + 24 * uint64_t(j);
            const uint64_t keys = r.At<uint64_t>(c), values = r.At<uint64_t>(c + 8);
            const uint32_t nkeys = r.At<uint32_t>(c + 16), type = r.At<uint32_t>(c + 20);
            const int slot = type == 0 ? 0 : type == 3 ? 1 : type == 2 ? 2 : -1;
            if (slot < 0 || nkeys == 0) continue;
            std::vector<Key>& out_keys = track.channels[size_t(slot)];
            out_keys.resize(nkeys);
            for (uint32_t k = 0; k < nkeys; ++k) {
                Key& key = out_keys[k];
                key.frame = r.At<float>(keys + 12 * uint64_t(k));
                const uint32_t off = r.At<uint32_t>(keys + 12 * uint64_t(k) + 4);
                for (int v = 0; v < (slot == 1 ? 4 : 3); ++v) key.v[size_t(v)] = r.At<float>(values + off + 4 * uint64_t(v));
            }
        }
        out.tracks.push_back(std::move(track));
    }
    if (!r.ok) return Fail(error, "BMA data out of range");
    return true;
}

void Pose(const Model& model, const Motion* motion, float frame, std::vector<Mat4>& world) {
    const Layer layer{motion, frame};
    Pose(model, motion ? std::span<const Layer>(&layer, 1) : std::span<const Layer>(), world);
}

void Pose(const Model& model, std::span<const Layer> layers, std::vector<Mat4>& world) {
    std::unordered_map<std::string_view, std::pair<const Track*, float>> by_node;  // track and its frame
    for (const Layer& l : layers) {
        if (!l.motion) continue;
        for (const Track& t : l.motion->tracks) by_node[t.node] = {&t, l.frame};
    }
    world.resize(model.nodes.size());
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const Node& n = model.nodes[i];
        float t[3] = {n.t[0], n.t[1], n.t[2]}, s[3] = {n.s[0], n.s[1], n.s[2]};
        Mat4 rot = RotationEuler(n.r[0], n.r[1], n.r[2]);
        if (const auto it = by_node.find(n.name); it != by_node.end()) {
            const Track& tr = *it->second.first;
            const float frame = it->second.second;
            if (!tr.channels[0].empty()) std::copy_n(Sample(tr.channels[0], frame, false).begin(), 3, t);
            if (!tr.channels[1].empty()) rot = RotationQuat(Sample(tr.channels[1], frame, true).data());
            if (!tr.channels[2].empty()) std::copy_n(Sample(tr.channels[2], frame, false).begin(), 3, s);
        }
        const Mat4 local = Compose(t, rot, s);
        // node 0 (bml_axisNode, Z-up -> Y-up) and its direct children start from the model's own Z-up space
        world[i] = n.parent <= 0 ? local : world[size_t(n.parent)] * local;
    }
}

}
