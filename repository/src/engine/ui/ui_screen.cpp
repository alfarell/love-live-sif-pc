#include "engine/ui/ui_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "engine/formats/blbn.h"

#include "engine/core/log.h"
#include "engine/fs/vfs.h"

namespace sif {

bool UiScreen::Load(Renderer& renderer, const Vfs& vfs, const std::string& screen, const std::string& lang,
                    std::string* error, const UiText* text) {
    text_ = text;
    renderer_ = &renderer;
    package_ = screen + (lang.empty() ? "" : "_" + lang) + ".pac";
    if (!vfs.Exists(package_)) package_ = screen + ".pac";
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List(package_, entries, error)) return false;
    for (const pac::ArcEntry& e : entries) {
        // "uip0010_TitleLogo00.pac" holds "uip0010_TitleLogo00.bmarc"
        if (!e.name.ends_with(".pac") || !(e.name.starts_with("uip") || e.name.starts_with("uil"))) continue;
        const std::string stem = e.name.substr(0, e.name.size() - 4);
        const std::string path = package_ + "/" + e.name + "/" + stem + ".bmarc";
        std::string why;
        if (stem.starts_with("uip")) {
            auto part = std::make_unique<SceneModel>();
            if (!part->Load(renderer, vfs, path, &why)) {
                LogWarn("ui {}: part {}: {}", package_, stem, why);
                continue;
            }
            part->Pose(renderer, nullptr, 0);
            parts_[stem] = std::move(part);
        } else {
            auto layout = std::make_unique<Layout>();
            if (!vfs.Read(path, layout->file, &why) || !bmar::Parse(layout->file, layout->model, &why)) {
                LogWarn("ui {}: layout {}: {}", package_, stem, why);
                continue;
            }
            bmar::Pose(layout->model, nullptr, 0, layout->world);
            layouts_[stem] = std::move(layout);
        }
    }
    if (text_) {
        // ll3::ui::s_ui_text_info_table: 32-byte records (u32 part id, u32 box, u32 style, u32 ?, u32 ?, u32 ?,
        // u64 string*); the part id is the NNNN of uipNNNN_<Name>
        std::vector<uint8_t> file;
        std::vector<blbn::Symbol> symbols;
        const blbn::Symbol* table = nullptr;
        if (vfs.Read(package_ + "/ui_text.bin", file) && blbn::Parse(file, symbols) &&
            (table = blbn::Find(symbols, "ll3::ui::s_ui_text_info_table"))) {
            const auto u32 = [&](size_t off) { uint32_t v = 0; if (off + 4 <= file.size()) std::memcpy(&v, &file[off], 4); return v; };
            const size_t base = size_t(table->data.data() - file.data());
            for (size_t r = 0; r + 32 <= table->data.size(); r += 32) {
                const uint32_t id = u32(base + r), box = u32(base + r + 4), style = u32(base + r + 8);
                const size_t str = size_t(u32(base + r + 24)) | size_t(u32(base + r + 28)) << 32;
                char prefix[16];
                std::snprintf(prefix, sizeof(prefix), "uip%04u_", id);
                for (const auto& [name, part] : parts_) {
                    if (!name.starts_with(prefix) || str >= file.size()) continue;
                    const char* s = reinterpret_cast<const char*>(&file[str]);
                    texts_[{name, int(box)}] = {int(style), std::string(s, strnlen(s, file.size() - str))};
                }
            }
        }
        const MeshVertex quad[4] = {{{0, 0, 0}, {0, 0, 1}, {0, 0}}, {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                                    {{1, -1, 0}, {0, 0, 1}, {1, 1}}, {{0, -1, 0}, {0, 0, 1}, {0, 1}}};
        const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        if (!renderer.CreateMesh(quad, indices, false, unit_quad_, error)) return false;
    }
    LogInfo("ui {}: {} parts, {} layouts, {} texts", package_, parts_.size(), layouts_.size(), texts_.size());
    return true;
}

void UiScreen::SetText(const std::string& part, int box, const std::string& utf8) {
    TextEntry& t = texts_[{part, box}];
    if (t.rendered && t.utf8 == utf8) return;
    if (renderer_ && t.texture.gpu) renderer_->Release(t.texture);
    t.utf8 = utf8;
    t.rendered = false;
}

void UiScreen::DrawTexts(Renderer& renderer, const std::string& part, const Mat4& to_clip, float alpha) const {
    if (!text_) return;
    const auto model = parts_.find(part);
    if (model == parts_.end()) return;
    const std::vector<std::array<float, 4>> boxes = model->second->TextBoxes();
    for (auto it = texts_.lower_bound({part, 0}); it != texts_.end() && it->first.first == part; ++it) {
        TextEntry& t = it->second;
        const int box = it->first.second;
        const TextStyle* style = text_->Style(t.style);
        if (!style || box >= int(boxes.size()) || t.utf8.empty()) continue;
        const auto& b = boxes[size_t(box)];  // x0, y0, x1, y1 (y up)
        if (!t.rendered) {
            t.rendered = true;
            TextBitmap bitmap;
            const float px = style->size > 0 ? style->size : b[3] - b[1];
            if (text_->Render(*style, t.utf8, px, bitmap)) {
                btx::Image image;
                image.width = uint32_t(bitmap.width);
                image.height = uint32_t(bitmap.height);
                image.levels = {{image.width, image.height, image.width * 4, 0, uint64_t(bitmap.bgra.size())}};
                image.data = bitmap.bgra;
                image.chain_size = bitmap.bgra.size();
                if (renderer.Upload(image, t.texture, nullptr)) {
                    t.width = bitmap.width;
                    t.height = bitmap.height;
                }
            }
        }
        if (!t.texture.gpu) continue;
        const float w = float(t.width), h = float(t.height);
        const float x = style->halign == 0 ? b[0] : style->halign == 1 ? (b[0] + b[2] - w) / 2 : b[2] - w;
        const float y = style->valign == 0 ? b[3] : style->valign == 1 ? (b[1] + b[3] + h) / 2 : b[1] + h;  // top edge
        Mat4 place;
        place(0, 0) = w;
        place(1, 1) = h;
        place(0, 3) = x;
        place(1, 3) = y;
        renderer.DrawUiMesh(unit_quad_, &t.texture, to_clip * place, alpha);
    }
}

void UiScreen::Release(Renderer& renderer) {
    for (auto& [key, t] : texts_) renderer.Release(t.texture);
    texts_.clear();
    renderer.Release(unit_quad_);
    for (auto& [name, part] : parts_) part->Release(renderer);
    parts_.clear();
    layouts_.clear();
}

Mat4 UiScreen::CanvasToClip(int w, int h) {
    // x 0..1920 -> -1..1, y 0..-1080 -> 1..-1, then shrunk on one axis to keep 16:9 inside the window
    const float s = std::min(float(w) / kWidth, float(h) / kHeight);
    const float sx = s * kWidth / float(std::max(1, w)), sy = s * kHeight / float(std::max(1, h));
    Mat4 m;
    m(0, 0) = 2 / kWidth * sx;
    m(0, 3) = -sx;
    m(1, 1) = 2 / kHeight * sy;
    m(1, 3) = sy;
    m(2, 2) = 0;  // flat
    return m;
}

void UiScreen::DrawPart(Renderer& renderer, const std::string& part, float x, float y, float alpha) const {
    const auto it = parts_.find(part);
    if (it == parts_.end()) return;
    Mat4 at;
    at(0, 3) = x;
    at(1, 3) = y;
    const Mat4 to_clip = CanvasToClip(renderer.Width(), renderer.Height()) * at;
    it->second->DrawUi(renderer, to_clip, alpha);
    DrawTexts(renderer, part, to_clip, alpha);
}

void UiScreen::DrawLayout(Renderer& renderer, const std::string& layout, float alpha,
                          const std::map<std::string, float>& part_alpha) const {
    const auto it = layouts_.find(layout);
    if (it == layouts_.end()) return;
    const Layout& l = *it->second;
    for (size_t i = 0; i < l.model.nodes.size(); ++i) {
        const auto scale = part_alpha.find(l.model.nodes[i].name);
        const float a = alpha * (scale == part_alpha.end() ? 1.0f : scale->second);
        const auto part = parts_.find(l.model.nodes[i].name);
        if (part == parts_.end()) continue;
        const Mat4 to_clip = CanvasToClip(renderer.Width(), renderer.Height()) * l.world[i];
        part->second->DrawUi(renderer, to_clip, a);
        DrawTexts(renderer, part->first, to_clip, a);
    }
}

}
