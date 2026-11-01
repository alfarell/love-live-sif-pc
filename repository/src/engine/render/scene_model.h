#pragma once
// A BMAR model on the GPU: one mesh per DRAW, textures from the model's own BMT archive or the sibling texture
// archive, skinned meshes posed on the CPU (8 influences, MTXP palette). Positions stay in the model's Z-up space.

#include <array>
#include <string>
#include <vector>

#include "engine/formats/bmar.h"
#include "engine/render/renderer.h"

namespace sif {

class Vfs;

class SceneModel {
public:
    // `path`: the model entry in the dump, e.g. "chr/c01/c01.pac/model.pac/mod_01_honoka".
    bool Load(Renderer& renderer, const Vfs& vfs, const std::string& path, std::string* error);
    // Bind pose when `motion` is null.
    void Pose(Renderer& renderer, const bmar::Motion* motion, float frame);
    void Pose(Renderer& renderer, std::span<const bmar::Layer> layers);
    void Draw(Renderer& renderer, const Mat4& placement) const;
    void Release(Renderer& renderer);
    const bmar::Model& Data() const { return model_; }

private:
    struct Part {
        uint32_t node = 0;
        int texture = -1;  // index into textures_
        bool skinned = false;
        GpuMesh gpu;
        std::vector<MeshVertex> bind, posed;              // skinned parts only keep these
        std::vector<std::array<uint8_t, 8>> bones;
        std::vector<std::array<float, 8>> weights;
    };

    std::vector<uint8_t> file_;  // the parsed model points into it
    bmar::Model model_;
    std::vector<Part> parts_;
    std::vector<Texture> textures_;
    std::vector<Mat4> world_, skin_;
};

}
