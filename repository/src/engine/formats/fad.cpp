#include "engine/formats/fad.h"

#include <cstring>

namespace sif::fad {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

template <class T> T At(std::span<const uint8_t> d, size_t off) {
    T v{};
    if (off + sizeof(T) <= d.size()) std::memcpy(&v, &d[off], sizeof(T));
    return v;
}

Matrix ReadMatrix(std::span<const uint8_t> p) {  // after u16 depth, u16 flag
    return {At<float>(p, 4), At<float>(p, 8), At<float>(p, 12), At<float>(p, 16), At<float>(p, 20), At<float>(p, 24)};
}

}

Matrix operator*(const Matrix& o, const Matrix& i) {
    return {o.a * i.a + o.c * i.b, o.b * i.c + o.d * i.d, o.b * i.a + o.d * i.b, o.a * i.c + o.c * i.d,
            o.a * i.tx + o.c * i.ty + o.tx, o.b * i.tx + o.d * i.ty + o.ty};
}

bool Parse(std::span<const uint8_t> f, Animation& out, std::string* error) {
    out = {};
    // header: u16 version 1, u16 ?, i32 x0, x1, y0, y1 (canvas), u16 fps, u16 frames
    if (f.size() < 24 || At<uint16_t>(f, 0) != 1) return Fail(error, "not a .fad animation");
    out.width = float(At<int32_t>(f, 8) - At<int32_t>(f, 4));
    out.height = float(At<int32_t>(f, 16) - At<int32_t>(f, 12));
    out.fps = At<uint16_t>(f, 20);
    std::map<int, std::string> textures;
    std::map<int, std::pair<float, float>> shapes;  // shape id (= its texture id) -> size
    int defining = 0;                               // sprite id between tag 03 and its tag 00
    std::map<int, Placed> list;                     // the main timeline's display list
    // records: u8 tag, u8 0xAA, u16 length in 16-bit words including these 4 bytes, payload
    for (size_t off = 24; off + 4 <= f.size();) {
        const uint8_t tag = f[off];
        const size_t words = At<uint16_t>(f, off + 2);
        if (f[off + 1] != 0xAA || words < 2 || off + words * 2 > f.size()) return Fail(error, "bad .fad record");
        const std::span<const uint8_t> p = f.subspan(off + 4, words * 2 - 4);
        off += words * 2;
        const auto depth = At<uint16_t>(p, 0);
        switch (tag) {
            case 0x09:  // textures: u16 count, then (u16 id, name, padded to an even length)
                for (size_t n = At<uint16_t>(p, 0), o = 2; n-- && o + 2 < p.size();) {
                    const int id = At<uint16_t>(p, o);
                    const char* s = reinterpret_cast<const char*>(&p[o + 2]);
                    const size_t len = strnlen(s, p.size() - o - 2);
                    textures[id] = std::string(s, len);
                    o = (o + 2 + len + 2) & ~size_t(1);
                }
                break;
            case 0x02:  // shape: u16 id (its texture's id), u16 width, u16 height
                shapes[depth] = {float(At<uint16_t>(p, 2)), float(At<uint16_t>(p, 4))};
                break;
            case 0x03:  // sprite definition: u16 id, u16 frame count (always 1 here); its records run to tag 00
                defining = depth;
                break;
            case 0x00:
                defining = 0;
                break;
            case 0x04:  // place: u16 character, u16 depth
                if (defining) {
                    const int shape = At<uint16_t>(p, 0);
                    Sprite& s = out.sprites[defining];
                    s.texture = textures[shape];
                    s.width = shapes[shape].first;
                    s.height = shapes[shape].second;
                } else {
                    list[At<uint16_t>(p, 2)] = {At<uint16_t>(p, 0), {}, {}};
                }
                break;
            case 0x05:  // matrix: u16 depth, u16 flag, f32 a, d, b, c, tx, ty
                if (defining) {
                    out.sprites[defining].matrix = ReadMatrix(p);
                } else if (auto it = list.find(depth); it != list.end()) {
                    it->second.matrix = ReadMatrix(p);
                }
                break;
            case 0x06:  // colour: u16 depth, u16 flags (0x80 | 1 multiply | 2 add), i16 mul RGBA (8.8), i16 add RGBA
                if (auto it = list.find(depth); it != list.end() && !defining) {
                    const uint16_t flags = At<uint16_t>(p, 2);
                    for (size_t k = 0; k < 4; ++k) {
                        it->second.colour.mul[k] = flags & 1 ? float(At<int16_t>(p, 4 + 2 * k)) / 256 : 1.0f;
                        it->second.colour.add[k] = flags & 2 ? float(At<int16_t>(p, 12 + 2 * k)) / 255 : 0.0f;
                    }
                }
                break;
            case 0x07:  // remove: u16 0, u16 depth
                if (!defining) list.erase(At<uint16_t>(p, 2));
                break;
            case 0x08:  // show frame
                if (!defining) out.frames.push_back(list);
                break;
            default: break;  // 0x01: background colour
        }
    }
    if (out.frames.empty()) return Fail(error, ".fad without frames");
    return true;
}

}
