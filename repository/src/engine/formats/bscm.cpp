#include "engine/formats/bscm.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sif::bscm {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

template <class T> T At(std::span<const uint8_t> d, uint64_t off) {
    T v{};
    if (off + sizeof(T) <= d.size()) std::memcpy(&v, &d[size_t(off)], sizeof(T));
    return v;
}

// Keys sorted by frame: index of the last key at or before `frame` and the blend factor to the next one.
template <class K> size_t Find(const std::vector<K>& keys, float frame, float& t) {
    t = 0;
    if (keys.size() < 2 || frame <= keys.front().first) return 0;
    if (frame >= keys.back().first) return keys.size() - 1;
    const auto hi = std::upper_bound(keys.begin(), keys.end(), frame, [](float f, const K& k) { return f < k.first; });
    const size_t i = size_t(hi - keys.begin()) - 1;
    const float span = keys[i + 1].first - keys[i].first;
    t = span > 0 ? (frame - keys[i].first) / span : 0;
    return i;
}

}

bool Parse(std::span<const uint8_t> f, Camera& out, std::string* error) {
    out = {};
    // chunk "BSCM" (u32 body size, u32 0x00040000) at 0; pointers in its body are file offsets of DATA bodies
    if (f.size() < 12 + 0x68 || std::memcmp(f.data(), "BSCM", 4) != 0) return Fail(error, "not a BSCM camera");
    const uint64_t h = 12;
    out.fps = At<float>(f, h + 4);
    out.frames = At<float>(f, h + 8);
    const uint32_t npos = At<uint32_t>(f, h + 0x0C), nrot = At<uint32_t>(f, h + 0x10), nfov = At<uint32_t>(f, h + 0x18);
    const uint64_t pos = At<uint64_t>(f, h + 0x48), rot = At<uint64_t>(f, h + 0x50), fov = At<uint64_t>(f, h + 0x60);
    // key: u32 frame, u32 flag (0), value: f32[3] position / f32[4] rotation / f32 field of view
    if (pos + 20ull * npos > f.size() || rot + 24ull * nrot > f.size() || fov + 12ull * nfov > f.size() || npos == 0 ||
        nrot == 0 || nfov == 0) {
        return Fail(error, "BSCM key arrays out of range");
    }
    for (uint32_t i = 0; i < npos; ++i) {
        const uint64_t k = pos + 20ull * i;
        out.position.push_back({float(At<uint32_t>(f, k)), {At<float>(f, k + 8), At<float>(f, k + 12), At<float>(f, k + 16)}});
    }
    for (uint32_t i = 0; i < nrot; ++i) {
        const uint64_t k = rot + 24ull * i;
        out.rotation.push_back({float(At<uint32_t>(f, k)), {At<float>(f, k + 8), At<float>(f, k + 12), At<float>(f, k + 16), At<float>(f, k + 20)}});
    }
    for (uint32_t i = 0; i < nfov; ++i) {
        const uint64_t k = fov + 12ull * i;
        out.fov.push_back({float(At<uint32_t>(f, k)), At<float>(f, k + 8)});
    }
    return true;
}

Pose Sample(const Camera& c, float frame) {
    Pose p;
    float t;
    size_t i = Find(c.position, frame, t);
    const Vec3 a = c.position[i].second, b = c.position[std::min(i + 1, c.position.size() - 1)].second;
    p.position = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    i = Find(c.rotation, frame, t);
    const auto& qa = c.rotation[i].second;
    auto qb = c.rotation[std::min(i + 1, c.rotation.size() - 1)].second;
    if (qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3] < 0) {
        for (float& x : qb) x = -x;
    }
    float n = 0;
    for (int k = 0; k < 4; ++k) {
        p.rotation[size_t(k)] = qa[size_t(k)] + (qb[size_t(k)] - qa[size_t(k)]) * t;
        n += p.rotation[size_t(k)] * p.rotation[size_t(k)];
    }
    n = std::sqrt(n);
    if (n > 0) {
        for (float& x : p.rotation) x /= n;
    }
    i = Find(c.fov, frame, t);
    p.fov_deg = c.fov[i].second + (c.fov[std::min(i + 1, c.fov.size() - 1)].second - c.fov[i].second) * t;
    return p;
}

Mat4 ViewMatrix(const Pose& pose) {
    // camera world = T(position) * R(rotation) in the Y-up world; its inverse = R^T * T(-position)
    const Mat4 r = RotationQuat(pose.rotation.data());
    Mat4 view;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) view(row, col) = r(col, row);
        view(row, 3) = -(r(0, row) * pose.position.x + r(1, row) * pose.position.y + r(2, row) * pose.position.z);
    }
    return view * RotationEuler(-1.5707963f, 0, 0);  // Z-up world -> the camera's Y-up world (bml_axisNode)
}

}
