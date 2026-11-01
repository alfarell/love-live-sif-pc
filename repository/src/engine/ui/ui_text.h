#pragma once
// UI text: the game's TrueType fonts (common/common.pac/font.pac) rasterized with stb_truetype, and the text styles
// of ui2/00_common[_<lang>].pac/font_param.bin (ll3::ui::s_font_table). Field meanings are partly Hypothesis
// (analysis/202611011550-m6-ui-boot-flow/).

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sif {

class Vfs;

struct TextStyle {
    int font = 0;                 // index into font.pac's entries (word 3, Hypothesis)
    int halign = 0, valign = 1;   // 0 left/top, 1 centre, 2 right/bottom (words 4 and 5, Hypothesis)
    float size = 0;               // pixels on the 1920 x 1080 canvas; 0 = fit the text box height (Hypothesis)
    float spacing = 0;            // extra advance per character in pixels (Hypothesis)
    uint32_t color = 0xFF000000;  // ARGB
    bool outline = false;
    uint32_t outline_color = 0xFFFFFFFF;
};

// A rasterized string: straight-alpha BGRA8 pixels.
struct TextBitmap {
    int width = 0, height = 0;
    std::vector<uint8_t> bgra;
};

class UiText {
public:
    // `lang`: "en", "cn", "tw" or "" (Japanese); styles come from that language's 00_common package.
    bool Load(const Vfs& vfs, const std::string& lang, std::string* error);
    const TextStyle* Style(int id) const;
    // Renders `utf8` at `px` pixels high in the style's font, colour and outline. False if the font is missing.
    bool Render(const TextStyle& style, std::string_view utf8, float px, TextBitmap& out) const;

private:
    struct Font;
    const Font* GetFont(int index) const;
    const Vfs* vfs_ = nullptr;
    std::vector<std::string> font_paths_;
    mutable std::map<int, std::shared_ptr<Font>> fonts_;  // loaded on first use (4-13 MB each)
    std::map<int, TextStyle> styles_;
};

}
