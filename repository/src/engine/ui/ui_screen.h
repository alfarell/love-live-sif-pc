#pragma once
// One `ui2` screen package (spec: analysis/202611011550-m6-ui-boot-flow/): parts `uipNNNN_<Name>` are BMAR models of
// textured quads in pixels, layouts `uilNNNN_<Name>` are BMAR node trees that place parts and other layouts by name.
// Node names may carry an instance suffix (`uip0005_SubMenuList00_03`) or a package prefix (`common_` = 00_common,
// `ctgNN_` = the ui2/NN_* screen). Each part has animation clips (`animation.pac/<part>_aNNN.bmarc`) with node,
// visibility and material tracks. Everything is drawn on a 1920 x 1080 canvas (origin top-left, y negative downwards)
// fitted to the window.

#include <map>
#include <memory>
#include <set>
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
    // back to the base package. Loads every part and layout of the package and the parts its layouts borrow from
    // other packages. `text` (optional, must outlive the screen) draws the package's ui_text.bin strings into their
    // parts' text boxes.
    bool Load(Renderer& renderer, const Vfs& vfs, const std::string& screen, const std::string& lang, std::string* error,
              const UiText* text = nullptr);
    // Replaces the default string of a part's text box (e.g. with message.pac text).
    void SetText(const std::string& part, int box, const std::string& utf8);
    // The same for one placed instance (its part's text style is kept), e.g. each song plate's title.
    void SetInstanceText(const std::string& instance, int box, const std::string& utf8);
    // Number boxes (ui_number.bin + numtex_datas.bin: digit glyphs from a number texture, e.g. a score): the string
    // shown in one placed instance's number box (digits only; the table's default otherwise).
    void SetInstanceNumber(const std::string& instance, int box, const std::string& digits);
    void Release(Renderer& renderer);

    // Canvas -> clip space for the current window (letterboxed, aspect kept).
    static Mat4 CanvasToClip(int window_width, int window_height);
    // A part centred at canvas (x, y), e.g. "uip0010_TitleLogo00". Text-box quads (no texture) are not drawn.
    void DrawPart(Renderer& renderer, const std::string& part, float x, float y, float alpha) const;
    // Every part a layout places (recursively through nested layouts), at its node; `part_alpha` (optional) scales
    // each top-level node by name.
    void DrawLayout(Renderer& renderer, const std::string& layout, float alpha,
                    const std::map<std::string, float>& part_alpha = {}) const;
    bool HasPart(const std::string& part) const { return parts_.contains(part); }

    // Clips: `instance` is the node path "layout/node[/node...]" of a placed part, or "*" for every instance whose part
    // has the clip; `clip` like "a003" (ids: 00_common ui_animation.bin, e.g. 1 in, 3 loop, 20 switch). Each instance
    // has animation layers (the game: FUN_0024bd10(rate, instance, clip, layer)). Update advances all playing clips
    // (60 frames per second); a clip set with `SetClipTime` holds that frame (the game: FUN_004b4790(time, inst, layer)),
    // e.g. the "switch" clip on layer 1 at the label id.
    void Play(const std::string& instance, const std::string& clip, bool loop, int layer = 0);
    void SetClipTime(const std::string& instance, int layer, float frame);
    static std::string ClipName(int id);  // 20 -> "a020"
    // Hides a node by name inside every placed part (the game: FUN_0024b8e0 + FUN_004739c0, e.g. "new00"), or a
    // whole placed instance by its path ("uil0001_TopMenu00/uil0004_SubMenuList01").
    void HideNode(const std::string& node, bool hide);
    // The same for one placed instance only (e.g. the focused plate's highlight).
    void HideNode(const std::string& instance, const std::string& node, bool hide);
    // Replaces a material's texture in one placed instance with a btx from the dump (cached), e.g. a song cover:
    // the game's FUN_0024B850(instance, material) + texture assignment. False if the texture cannot be loaded.
    bool SetMaterialTexture(const std::string& instance, const std::string& material, const std::string& vfs_path);
    // Shifts a material's UVs in one placed instance (e.g. the row of a label atlas), in texture units.
    void SetMaterialUv(const std::string& instance, const std::string& material, float u, float v);
    // Draws a material in one placed instance only where its mesh U is at most `u_max` (a gauge filled to a fraction).
    void SetMaterialClip(const std::string& instance, const std::string& material, float u_max);
    void HideInstance(const std::string& path, bool hide);
    void Update(double seconds);
    std::vector<std::string> Layouts() const;

private:
    struct Part {
        std::unique_ptr<SceneModel> model;
        std::map<std::string, std::pair<std::vector<uint8_t>, bmar::Motion>> clips;  // "a003" -> file, motion
    };
    struct Layout {
        std::vector<uint8_t> file;
        bmar::Model model;
        std::vector<Mat4> world;
        std::string prefix;                // package prefix of its nodes ("" or "common_" ...)
        std::vector<std::string> targets;  // per node: the loaded part or layout key it places ("" = none)
    };
    struct Playing {
        std::string clip;  // "a003"
        double frame = 0;
        bool loop = false;
    };
    struct TextEntry {
        int style = 0;
        std::string utf8;
        Texture texture;   // rendered on first draw
        int width = 0, height = 0;
        bool rendered = false;
    };

    // Loads `stem` (a uip or uil entry) of `package` under the key prefix + stem, and what a layout needs.
    void Require(Renderer& renderer, const Vfs& vfs, const std::string& package, const std::string& prefix,
                 const std::string& stem, int depth);
    // The entry stem of `package` a layout node names: the same name, else the entry with the same uipNNNN/uilNNNN
    // id (nodes are named "<id>_<local name>[_NN]", e.g. uip0004_MenuIcon00_00 -> uip0004_MenuList00); "" if none.
    std::string FindStem(const Vfs& vfs, const std::string& package, const std::string& node);
    void DrawLayoutAt(Renderer& renderer, const std::string& key, const Mat4& to_clip, const std::string& path,
                      float alpha, const std::map<std::string, float>* part_alpha) const;
    void DrawPartAt(Renderer& renderer, const std::string& key, const Mat4& to_clip, const std::string& path,
                    float alpha) const;
    void DrawTexts(Renderer& renderer, const std::string& part, const std::string& path, const Mat4& to_clip,
                   float alpha) const;
    void Render(Renderer& renderer, TextEntry& t, const TextStyle& style, float px) const;
    void DrawNumbers(Renderer& renderer, const std::string& part, const std::string& path, const Mat4& to_clip,
                     float alpha) const;
    const Texture* GetTexture(const std::string& vfs_path);

    std::string package_, lang_;
    std::map<std::string, std::vector<std::string>> stems_;  // package -> its uip/uil entry stems
    std::map<std::string, Part> parts_;
    std::map<std::string, std::unique_ptr<Layout>> layouts_;
    struct Held {
        std::string clip;
        float frame = 0;
    };
    std::map<std::string, Playing> playing_;               // by instance path (layer 0)
    std::map<std::pair<std::string, int>, Held> held_;     // (instance path, layer >= 1) -> clip at a fixed frame
    Playing play_all_;  // "*": this clip on every instance whose part has it (empty clip = none)
    std::set<std::string> hidden_nodes_, hidden_paths_;
    std::map<std::string, std::set<std::string>> instance_hidden_;                      // instance -> node names
    std::map<std::string, std::map<std::string, const Texture*>> instance_textures_;  // instance -> material -> texture
    std::map<std::string, std::map<std::string, std::array<float, 3>>> instance_uvs_;  // instance -> material -> u, v, u_max
    std::map<std::string, Texture> loaded_textures_;                                  // vfs path -> texture
    const Vfs* vfs_ = nullptr;
    const UiText* text_ = nullptr;
    mutable std::map<std::pair<std::string, int>, TextEntry> texts_;           // (part, box) -> string
    mutable std::map<std::pair<std::string, int>, TextEntry> instance_texts_;  // (instance path, box) -> string
    struct NumberBox {
        int numtex = 0, digits = 0, align = 2;  // align: 0 left, 1 centre, 2 right (Hypothesis: 2 everywhere)
        std::string value;                      // the table's default
    };
    struct NumTex {
        const Texture* texture = nullptr;
        float x0 = 0, y0 = 0, w = 64, h = 64;  // the row origin and cell size in the texture (pixels)
        std::array<float, 20> advance{};        // per glyph ('0'-'9' first)
        GpuMesh glyph;                          // one cell's quad, (0,0)-(1,-1) with the cell's UV span
    };
    std::map<std::pair<std::string, int>, NumberBox> numbers_;      // (part, box)
    std::map<int, NumTex> numtex_;                                  // numtex id
    std::map<std::pair<std::string, int>, std::string> instance_numbers_;
    GpuMesh unit_quad_;  // (0,0)-(1,-1), for text bitmaps
    Renderer* renderer_ = nullptr;
};

}
