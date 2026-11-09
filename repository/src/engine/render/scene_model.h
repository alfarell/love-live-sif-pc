#pragma once
// A BMAR model on the GPU: one mesh per DRAW, textures from the model's own BMT archive or the sibling texture
// archive, skinned meshes posed on the CPU (8 influences, MTXP palette). Positions stay in the model's Z-up space.

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "engine/formats/bmar.h"
#include "engine/render/renderer.h"

namespace sif {

class Vfs;

class SceneModel {
public:
    // `path`: the model entry in the dump, e.g. "chr/c01/c01.pac/model.pac/mod_01_honoka". `more_textures`
    // (optional): one more folder whose texture archive is searched last, e.g. a costume model's own character
    // folder for the face and hair ("chr/c01/c01.pac/model.pac").
    bool Load(Renderer& renderer, const Vfs& vfs, const std::string& path, std::string* error,
              const std::string& more_textures = {});
    // Bind pose when `motion` is null.
    void Pose(Renderer& renderer, const bmar::Motion* motion, float frame);
    void Pose(Renderer& renderer, std::span<const bmar::Layer> layers);
    // `by_texture` (optional): the material's texture file name -> a texture drawn instead (e.g. a stage's
    // screen placeholder "monitor_scrnrogo.tga" -> the streamed movie)
    void Draw(Renderer& renderer, const Mat4& placement,
              const std::map<std::string, const Texture*>* by_texture = nullptr) const;
    // UI: every textured mesh blended in the 2D pass; `to_clip` maps the model to clip space. Clip layers (the game
    // plays several per instance, e.g. an animation on layer 0 and the "switch" label clip on layer 1) apply their
    // node, visibility (ANVS) and material (ANMT: colour, UV offset) tracks; later layers win per node or material.
    // `hidden` (optional): node names hidden with their children, like the game's per-node visibility flag.
    // `textures` (optional): material name -> texture replacing the material's own (e.g. a song's cover).
    // `uv_offsets` (optional): material name -> (U offset, V offset, U cut-off) set by code (e.g. a label atlas row, a
    // fill gauge); clip offsets win over the offset.
    void DrawUi(Renderer& renderer, const Mat4& to_clip, float alpha, std::span<const bmar::Layer> clips = {},
                const std::set<std::string>* hidden = nullptr,
                const std::map<std::string, const Texture*>* textures = nullptr,
                const std::map<std::string, std::array<float, 3>>* uv_offsets = nullptr) const;
    void Release(Renderer& renderer);
    const bmar::Model& Data() const { return model_; }
    // UI text boxes: the meshes whose material names no texture, in draw order, as (x0, y0, x1, y1) after their node
    // transform (a named texture missing from the dump, or a mask node named "*stencil*", does not make a text box)
    std::vector<std::array<float, 4>> TextBoxes() const;

private:
    struct Part {
        uint32_t node = 0;
        int texture = -1;  // index into textures_
        uint32_t material = 0;
        bool skinned = false;
        float bounds[4]{};  // x0, y0, x1, y1 of the vertices in node space (UI text boxes)
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
