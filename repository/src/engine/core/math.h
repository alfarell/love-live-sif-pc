#pragma once
// Small 3D math for the renderer and model posing. Mat4 is column-major (m[col * 4 + row]) with column vectors,
// the layout GLSL expects. Projection: right-handed view space, depth 0..1 (SDL GPU convention).

#include <cmath>

namespace sif {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline Vec3 Normalize(Vec3 v) {
    const float l = std::sqrt(Dot(v, v));
    return l > 0 ? Vec3{v.x / l, v.y / l, v.z / l} : v;
}

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float& operator()(int row, int col) { return m[col * 4 + row]; }
    float operator()(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r(row, c) = a(row, 0) * b(0, c) + a(row, 1) * b(1, c) + a(row, 2) * b(2, c) + a(row, 3) * b(3, c);
        }
    }
    return r;
}

inline Vec3 TransformPoint(const Mat4& a, Vec3 p) {
    return {a(0, 0) * p.x + a(0, 1) * p.y + a(0, 2) * p.z + a(0, 3), a(1, 0) * p.x + a(1, 1) * p.y + a(1, 2) * p.z + a(1, 3),
            a(2, 0) * p.x + a(2, 1) * p.y + a(2, 2) * p.z + a(2, 3)};
}
inline Vec3 TransformDir(const Mat4& a, Vec3 d) {
    return {a(0, 0) * d.x + a(0, 1) * d.y + a(0, 2) * d.z, a(1, 0) * d.x + a(1, 1) * d.y + a(1, 2) * d.z,
            a(2, 0) * d.x + a(2, 1) * d.y + a(2, 2) * d.z};
}

// Rotation matrices from Euler angles (R = Rz * Ry * Rx, radians) or a unit quaternion (x, y, z, w).
inline Mat4 RotationEuler(float rx, float ry, float rz) {
    const float cx = std::cos(rx), sx = std::sin(rx), cy = std::cos(ry), sy = std::sin(ry), cz = std::cos(rz), sz = std::sin(rz);
    Mat4 r;
    r(0, 0) = cz * cy; r(0, 1) = cz * sy * sx - sz * cx; r(0, 2) = cz * sy * cx + sz * sx;
    r(1, 0) = sz * cy; r(1, 1) = sz * sy * sx + cz * cx; r(1, 2) = sz * sy * cx - cz * sx;
    r(2, 0) = -sy;     r(2, 1) = cy * sx;                r(2, 2) = cy * cx;
    return r;
}
inline Mat4 RotationQuat(const float q[4]) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    Mat4 r;
    r(0, 0) = 1 - 2 * (y * y + z * z); r(0, 1) = 2 * (x * y - z * w);     r(0, 2) = 2 * (x * z + y * w);
    r(1, 0) = 2 * (x * y + z * w);     r(1, 1) = 1 - 2 * (x * x + z * z); r(1, 2) = 2 * (y * z - x * w);
    r(2, 0) = 2 * (x * z - y * w);     r(2, 1) = 2 * (y * z + x * w);     r(2, 2) = 1 - 2 * (x * x + y * y);
    return r;
}
// T * R * S
inline Mat4 Compose(const float t[3], const Mat4& rotation, const float s[3]) {
    Mat4 r = rotation;
    for (int row = 0; row < 3; ++row) {
        for (int c = 0; c < 3; ++c) r(row, c) *= s[c];
        r(row, 3) = t[row];
    }
    return r;
}

inline Mat4 Perspective(float fovy_rad, float aspect, float near_z, float far_z) {
    const float f = 1.0f / std::tan(fovy_rad / 2);
    Mat4 r;
    r(0, 0) = f / aspect;
    r(1, 1) = f;
    r(2, 2) = far_z / (near_z - far_z);
    r(2, 3) = near_z * far_z / (near_z - far_z);
    r(3, 2) = -1;
    r(3, 3) = 0;
    return r;
}

inline Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 f = Normalize(target - eye), s = Normalize(Cross(f, up)), u = Cross(s, f);
    Mat4 r;
    r(0, 0) = s.x;  r(0, 1) = s.y;  r(0, 2) = s.z;  r(0, 3) = -Dot(s, eye);
    r(1, 0) = u.x;  r(1, 1) = u.y;  r(1, 2) = u.z;  r(1, 3) = -Dot(u, eye);
    r(2, 0) = -f.x; r(2, 1) = -f.y; r(2, 2) = -f.z; r(2, 3) = Dot(f, eye);
    return r;
}

}
