#include "engine/ui/ui_screen.h"

#include <algorithm>

#include "engine/core/log.h"
#include "engine/fs/vfs.h"

namespace sif {

bool UiScreen::Load(Renderer& renderer, const Vfs& vfs, const std::string& screen, const std::string& lang,
                    std::string* error) {
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
    LogInfo("ui {}: {} parts, {} layouts", package_, parts_.size(), layouts_.size());
    return true;
}

void UiScreen::Release(Renderer& renderer) {
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
    it->second->DrawUi(renderer, CanvasToClip(renderer.Width(), renderer.Height()) * at, alpha);
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
        part->second->DrawUi(renderer, CanvasToClip(renderer.Width(), renderer.Height()) * l.world[i], a);
    }
}

}
