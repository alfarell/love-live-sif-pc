#pragma once
// SIF `BSCM` camera animations (`camera_anim_<song>_00_main.bscam` in live/<song>/<song>.pac/scene.pac; spec in
// docs/formats/bscm.md). Keys per frame of the song's dance clock: position, rotation and field of view, in a Y-up
// world; the camera looks along its local -Z.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "engine/core/math.h"

namespace sif::bscm {

struct Camera {
    float fps = 60, frames = 0;
    std::vector<std::pair<float, Vec3>> position;
    std::vector<std::pair<float, std::array<float, 4>>> rotation;  // quaternion x, y, z, w
    std::vector<std::pair<float, float>> fov;                      // degrees (Hypothesis: vertical)
};

bool Parse(std::span<const uint8_t> file, Camera& out, std::string* error = nullptr);

struct Pose {
    Vec3 position;
    std::array<float, 4> rotation{0, 0, 0, 1};
    float fov_deg = 30;
};
Pose Sample(const Camera& camera, float frame);

// View matrix for a Z-up world (BMAR model space): the camera's Y-up world turned by bml_axisNode's -90 deg about X.
Mat4 ViewMatrix(const Pose& pose);

}
