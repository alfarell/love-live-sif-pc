#include "engine/ui/ui_text.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"

#include "stb_truetype.h"

namespace sif {
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

// "#rrggbb" -> 0xFFrrggbb (malformed -> black)
uint32_t ParseColor(std::span<const uint8_t> file, uint64_t off) {
    if (off + 7 > file.size() || file[size_t(off)] != '#') return 0xFF000000;
    uint32_t v = 0;
    for (size_t i = 1; i < 7; ++i) {
        const char c = char(file[size_t(off) + i]);
        const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return 0xFF000000;
        v = v << 4 | uint32_t(d);
    }
    return 0xFF000000 | v;
}

// UTF-8 -> code points (invalid bytes become U+FFFD)
std::vector<int> Decode(std::string_view s) {
    std::vector<int> out;
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        const int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (n == 0 || i + size_t(n) > s.size()) {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        int cp = n == 1 ? c : c & (0x7F >> n);
        for (int k = 1; k < n; ++k) cp = cp << 6 | (static_cast<unsigned char>(s[i + size_t(k)]) & 0x3F);
        out.push_back(cp);
        i += size_t(n);
    }
    return out;
}

}

struct UiText::Font {
    std::vector<uint8_t> file;
    stbtt_fontinfo info{};
};

bool UiText::Load(const Vfs& vfs, const std::string& lang, std::string* error) {
    vfs_ = &vfs;
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List("common/common.pac/font.pac", entries, error)) return false;
    for (const pac::ArcEntry& e : entries) font_paths_.push_back("common/common.pac/font.pac/" + e.name);

    // ll3::ui::s_font_table: 80-byte records; strings are "#rrggbb" colours referenced by file offset
    std::string package = "ui2/00_common" + (lang.empty() ? std::string() : "_" + lang) + ".pac";
    if (!vfs.Exists(package)) package = "ui2/00_common.pac";
    std::vector<uint8_t> file;
    std::vector<blbn::Symbol> symbols;
    if (!vfs.Read(package + "/font_param.bin", file, error) || !blbn::Parse(file, symbols, error)) return false;
    const blbn::Symbol* table = blbn::Find(symbols, "ll3::ui::s_font_table");
    if (!table) return Fail(error, "font_param.bin has no s_font_table");
    const std::span<const uint8_t> f = file;
    const uint64_t base = uint64_t(table->data.data() - file.data());
    for (uint64_t r = 0; r + 80 <= table->data.size(); r += 80) {
        const uint64_t o = base + r;
        TextStyle s;
        s.font = int(At<uint32_t>(f, o + 12));
        s.halign = std::min(2, int(At<uint32_t>(f, o + 16)));
        s.valign = std::min(2, int(At<uint32_t>(f, o + 20)));
        s.size = float(At<uint32_t>(f, o + 24));
        s.spacing = At<float>(f, o + 32);
        s.color = ParseColor(f, At<uint64_t>(f, o + 40));
        s.outline = At<uint32_t>(f, o + 48) != 0;
        s.outline_color = ParseColor(f, At<uint64_t>(f, o + 56));
        styles_[int(At<uint32_t>(f, o))] = s;
    }
    return true;
}

const TextStyle* UiText::Style(int id) const {
    const auto it = styles_.find(id);
    return it == styles_.end() ? nullptr : &it->second;
}

const UiText::Font* UiText::GetFont(int index) const {
    if (const auto it = fonts_.find(index); it != fonts_.end()) return it->second.get();
    std::shared_ptr<Font> font;
    if (index >= 0 && size_t(index) < font_paths_.size()) {
        font = std::make_shared<Font>();
        if (!vfs_->Read(font_paths_[size_t(index)], font->file) ||
            !stbtt_InitFont(&font->info, font->file.data(), stbtt_GetFontOffsetForIndex(font->file.data(), 0))) {
            font.reset();
        }
    }
    fonts_[index] = font;
    return font.get();
}

bool UiText::Render(const TextStyle& style, std::string_view utf8, float px, TextBitmap& out) const {
    const Font* font = GetFont(style.font);
    if (!font || px <= 0) return false;
    const stbtt_fontinfo& info = font->info;
    const float scale = stbtt_ScaleForPixelHeight(&info, px);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    const int pad = style.outline ? 3 : 1;
    const std::vector<int> cps = Decode(utf8);
    // width: sum of advances, kerning and spacing
    float pen = 0;
    for (size_t i = 0; i < cps.size(); ++i) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&info, cps[i], &advance, &lsb);
        pen += float(advance) * scale + style.spacing;
        if (i + 1 < cps.size()) pen += float(stbtt_GetCodepointKernAdvance(&info, cps[i], cps[i + 1])) * scale;
    }
    const int width = std::max(1, int(std::ceil(pen)) + 2 * pad);
    const int height = int(std::ceil(float(ascent - descent) * scale)) + 2 * pad;
    const int baseline = pad + int(std::round(float(ascent) * scale));
    std::vector<uint8_t> cover(size_t(width) * size_t(height), 0);
    pen = float(pad);
    for (size_t i = 0; i < cps.size(); ++i) {
        int advance, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(&info, cps[i], &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&info, cps[i], scale, scale, &x0, &y0, &x1, &y1);
        const int gx = int(std::round(pen)) + x0, gy = baseline + y0, gw = x1 - x0, gh = y1 - y0;
        if (gw > 0 && gh > 0) {
            std::vector<uint8_t> glyph(size_t(gw) * size_t(gh));
            stbtt_MakeCodepointBitmap(&info, glyph.data(), gw, gh, gw, scale, scale, cps[i]);
            for (int y = 0; y < gh; ++y) {
                for (int x = 0; x < gw; ++x) {
                    const int tx = gx + x, ty = gy + y;
                    if (tx < 0 || ty < 0 || tx >= width || ty >= height) continue;
                    uint8_t& c = cover[size_t(ty) * size_t(width) + size_t(tx)];
                    c = std::max(c, glyph[size_t(y) * size_t(gw) + size_t(x)]);
                }
            }
        }
        pen += float(advance) * scale + style.spacing;
        if (i + 1 < cps.size()) pen += float(stbtt_GetCodepointKernAdvance(&info, cps[i], cps[i + 1])) * scale;
    }
    // outline: the coverage dilated by `pad - 1` pixels, drawn under the fill
    std::vector<uint8_t> edge;
    if (style.outline) {
        edge.assign(cover.size(), 0);
        const int r = pad - 1;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t m = 0;
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        const int sx = x + dx, sy = y + dy;
                        if (sx >= 0 && sy >= 0 && sx < width && sy < height && dx * dx + dy * dy <= r * r + 1) {
                            m = std::max(m, cover[size_t(sy) * size_t(width) + size_t(sx)]);
                        }
                    }
                }
                edge[size_t(y) * size_t(width) + size_t(x)] = m;
            }
        }
    }
    out.width = width;
    out.height = height;
    out.bgra.assign(size_t(width) * size_t(height) * 4, 0);
    const auto channel = [](uint32_t c, int shift) { return float((c >> shift) & 0xFF); };
    for (size_t i = 0; i < cover.size(); ++i) {
        const float fa = float(cover[i]) / 255.0f, oa = edge.empty() ? 0.0f : float(edge[i]) / 255.0f;
        const float a = fa + oa * (1 - fa);  // fill over outline
        if (a <= 0) continue;
        uint8_t* p = &out.bgra[i * 4];
        for (int k = 0; k < 3; ++k) {  // B, G, R
            const float f = channel(style.color, 8 * k), o = channel(style.outline_color, 8 * k);
            p[k] = uint8_t(std::clamp((f * fa + o * oa * (1 - fa)) / a, 0.0f, 255.0f));
        }
        p[3] = uint8_t(std::clamp(a * 255.0f, 0.0f, 255.0f));
    }
    return true;
}

}
