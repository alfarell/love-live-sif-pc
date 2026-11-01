#pragma once
// One `ui2` screen package (spec: analysis/202611011550-m6-ui-boot-flow/): parts `uipNNNN_<Name>` are BMAR models of
// textured quads in pixels, layouts `uilNNNN_<Name>` are BMAR node trees that place parts by name. Everything is drawn
// on a 1920 x 1080 canvas (origin top-left, y negative downwards) fitted to the window.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/formats/bmar.h"
#include "engine/render/renderer.h"
#include "engine/render/scene_model.h"

namespace sif {

class Vfs;

class UiScreen {
public:
    static constexpr float kWidth = 1920, kHeight = 1080;

    // `screen` like "ui2/01_title"; `lang` "en", "cn", "tw" or "" (Japanese) picks ui2/01_title_<lang>.pac, falling
    // back to the base package. Loads every part and layout of the package.
    bool Load(Renderer& renderer, const Vfs& vfs, const std::string& screen, const std::string& lang, std::string* error);
    void Release(Renderer& renderer);

    // Canvas -> clip space for the current window (letterboxed, aspect kept).
    static Mat4 CanvasToClip(int window_width, int window_height);
    // A part centred at canvas (x, y), e.g. "uip0010_TitleLogo00". Text-box quads (no texture) are not drawn.
    void DrawPart(Renderer& renderer, const std::string& part, float x, float y, float alpha) const;
    // Every part a layout places, at its node; `part_alpha` (optional) scales each part by name.
    void DrawLayout(Renderer& renderer, const std::string& layout, float alpha,
                    const std::map<std::string, float>& part_alpha = {}) const;
    bool HasPart(const std::string& part) const { return parts_.contains(part); }

private:
    struct Layout {
        std::vector<uint8_t> file;
        bmar::Model model;
        std::vector<Mat4> world;
    };
    std::string package_;
    std::map<std::string, std::unique_ptr<SceneModel>> parts_;
    std::map<std::string, std::unique_ptr<Layout>> layouts_;
};

}
