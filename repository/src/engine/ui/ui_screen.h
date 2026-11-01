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
#include "engine/ui/ui_text.h"

namespace sif {

class Vfs;

class UiScreen {
public:
    static constexpr float kWidth = 1920, kHeight = 1080;

    // `screen` like "ui2/01_title"; `lang` "en", "cn", "tw" or "" (Japanese) picks ui2/01_title_<lang>.pac, falling
    // back to the base package. Loads every part and layout of the package.
    // `text` (optional, must outlive the screen) draws the package's ui_text.bin strings into their parts' text boxes.
    bool Load(Renderer& renderer, const Vfs& vfs, const std::string& screen, const std::string& lang, std::string* error,
              const UiText* text = nullptr);
    // Replaces the default string of a part's text box (e.g. with message.pac text).
    void SetText(const std::string& part, int box, const std::string& utf8);
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
    struct TextEntry {
        int style = 0;
        std::string utf8;
        Texture texture;   // rendered on first draw
        int width = 0, height = 0;
        bool rendered = false;
    };
    void DrawTexts(Renderer& renderer, const std::string& part, const Mat4& to_clip, float alpha) const;

    struct Layout {
        std::vector<uint8_t> file;
        bmar::Model model;
        std::vector<Mat4> world;
    };
    std::string package_;
    std::map<std::string, std::unique_ptr<SceneModel>> parts_;
    std::map<std::string, std::unique_ptr<Layout>> layouts_;
    const UiText* text_ = nullptr;
    mutable std::map<std::pair<std::string, int>, TextEntry> texts_;  // (part, box) -> string
    GpuMesh unit_quad_;  // (0,0)-(1,-1), for text bitmaps
    Renderer* renderer_ = nullptr;
};

}
