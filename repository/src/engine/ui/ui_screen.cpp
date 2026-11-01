#include "engine/ui/ui_screen.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "engine/core/log.h"
#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"

namespace sif {
namespace {

// The ui2 screens; `ctgNN_` node prefixes name them by their number.
constexpr const char* kScreens[] = {"00_common", "01_title",      "02_purchase", "03_card",  "10_top_menu",
                                    "20_setup",  "30_costume",    "40_collection", "50_event", "60_option",
                                    "61_credit", "80_skill_card", "90_ingame",   "95_result"};

std::string PackageOf(const Vfs& vfs, const std::string& screen, const std::string& lang) {
    const std::string localized = screen + (lang.empty() ? "" : "_" + lang) + ".pac";
    return vfs.Exists(localized) ? localized : screen + ".pac";
}

// A node's package prefix ("common_", "ctgNN_") and the screen it names, or {"", ""}.
std::pair<std::string, std::string> ForeignPrefix(const std::string& node) {
    if (node.starts_with("common_")) return {"common_", "ui2/00_common"};
    if (node.size() > 6 && node.starts_with("ctg") && std::isdigit(static_cast<unsigned char>(node[3])) &&
        std::isdigit(static_cast<unsigned char>(node[4])) && node[5] == '_') {
        for (const char* s : kScreens) {
            if (node.compare(3, 2, s, 2) == 0) return {node.substr(0, 6), std::string("ui2/") + s};
        }
    }
    return {"", ""};
}

}

bool UiScreen::Load(Renderer& renderer, const Vfs& vfs, const std::string& screen, const std::string& lang,
                    std::string* error, const UiText* text) {
    text_ = text;
    renderer_ = &renderer;
    lang_ = lang;
    package_ = PackageOf(vfs, screen, lang);
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List(package_, entries, error)) return false;
    for (const pac::ArcEntry& e : entries) {
        // "uip0010_TitleLogo00.pac" holds "uip0010_TitleLogo00.bmarc"
        if (!e.name.ends_with(".pac") || !(e.name.starts_with("uip") || e.name.starts_with("uil"))) continue;
        Require(renderer, vfs, package_, "", e.name.substr(0, e.name.size() - 4), 0);
    }
    if (text_) {
        // ll3::ui::s_ui_text_info_table: 32-byte records (u32 part id, u32 box, u32 style, u32 ?, u32 ?, u32 ?,
        // u64 string*); the part id is the NNNN of uipNNNN_<Name>
        std::vector<uint8_t> file;
        std::vector<blbn::Symbol> symbols;
        const blbn::Symbol* table = nullptr;
        if (vfs.Read(package_ + "/ui_text.bin", file) && blbn::Parse(file, symbols) &&
            (table = blbn::Find(symbols, "ll3::ui::s_ui_text_info_table"))) {
            const auto u32 = [&](size_t off) {
                uint32_t v = 0;
                if (off + 4 <= file.size()) std::memcpy(&v, &file[off], 4);
                return v;
            };
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
    }
    const MeshVertex quad[4] = {{{0, 0, 0}, {0, 0, 1}, {0, 0}}, {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                                {{1, -1, 0}, {0, 0, 1}, {1, 1}}, {{0, -1, 0}, {0, 0, 1}, {0, 1}}};
    const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
    if (!renderer.CreateMesh(quad, indices, false, unit_quad_, error)) return false;
    size_t clips = 0;
    for (const auto& [name, part] : parts_) clips += part.clips.size();
    LogInfo("ui {}: {} parts, {} layouts, {} clips, {} texts", package_, parts_.size(), layouts_.size(), clips,
            texts_.size());
    return true;
}

void UiScreen::Require(Renderer& renderer, const Vfs& vfs, const std::string& package, const std::string& prefix,
                       const std::string& stem, int depth) {
    const std::string key = prefix + stem;
    if (depth > 8 || parts_.contains(key) || layouts_.contains(key)) return;
    const std::string dir = package + "/" + stem + ".pac";
    const std::string path = dir + "/" + stem + ".bmarc";
    std::string why;
    if (stem.starts_with("uip")) {
        Part part;
        part.model = std::make_unique<SceneModel>();
        if (!part.model->Load(renderer, vfs, path, &why)) {
            LogWarn("ui {}: part {}: {}", package, stem, why);
            return;
        }
        part.model->Pose(renderer, nullptr, 0);
        std::vector<pac::ArcEntry> clips;
        if (vfs.List(dir + "/animation.pac", clips)) {
            for (const pac::ArcEntry& c : clips) {
                // "uip0004_MenuList00_a003.bmarc" -> "a003"
                const size_t us = c.name.rfind('_'), dot = c.name.rfind('.');
                if (us == std::string::npos || dot == std::string::npos || dot < us) continue;
                auto& [file, motion] = part.clips[c.name.substr(us + 1, dot - us - 1)];
                if (!vfs.Read(dir + "/animation.pac/" + c.name, file) || !bmar::ParseMotion(file, motion, &why)) {
                    LogWarn("ui {}: clip {}: {}", package, c.name, why);
                    part.clips.erase(c.name.substr(us + 1, dot - us - 1));
                }
            }
        }
        parts_[key] = std::move(part);
        return;
    }
    auto layout = std::make_unique<Layout>();
    if (!vfs.Read(path, layout->file, &why) || !bmar::Parse(layout->file, layout->model, &why)) {
        LogWarn("ui {}: layout {}: {}", package, stem, why);
        return;
    }
    bmar::Pose(layout->model, nullptr, 0, layout->world);
    layout->prefix = prefix;
    std::vector<std::string> targets;
    for (const bmar::Node& node : layout->model.nodes) {
        const auto [foreign, screen] = ForeignPrefix(node.name);
        const std::string node_package = foreign.empty() ? package : PackageOf(vfs, screen, lang_);
        const std::string node_prefix = foreign.empty() ? prefix : foreign;
        const std::string stem = FindStem(vfs, node_package, node.name.substr(foreign.size()));
        targets.push_back(stem.empty() ? std::string() : node_prefix + stem);
        if (!stem.empty()) Require(renderer, vfs, node_package, node_prefix, stem, depth + 1);
    }
    layout->targets = std::move(targets);
    layouts_[key] = std::move(layout);
}

std::string UiScreen::FindStem(const Vfs& vfs, const std::string& package, const std::string& node) {
    if (!(node.starts_with("uip") || node.starts_with("uil")) || node.size() < 8) return {};
    auto it = stems_.find(package);
    if (it == stems_.end()) {
        std::vector<pac::ArcEntry> entries;
        std::vector<std::string> stems;
        if (vfs.List(package, entries)) {
            for (const pac::ArcEntry& e : entries) {
                if (e.name.ends_with(".pac") && (e.name.starts_with("uip") || e.name.starts_with("uil"))) {
                    stems.push_back(e.name.substr(0, e.name.size() - 4));
                }
            }
        }
        it = stems_.emplace(package, std::move(stems)).first;
    }
    for (const std::string& s : it->second) {
        if (s == node) return s;
    }
    const std::string id = node.substr(0, 8);  // "uip0004_"
    for (const std::string& s : it->second) {
        if (s.starts_with(id)) return s;
    }
    return {};
}

void UiScreen::SetText(const std::string& part, int box, const std::string& utf8) {
    TextEntry& t = texts_[{part, box}];
    if (t.rendered && t.utf8 == utf8) return;
    if (renderer_ && t.texture.gpu) renderer_->Release(t.texture);
    t.utf8 = utf8;
    t.rendered = false;
}

void UiScreen::Play(const std::string& instance, const std::string& clip, bool loop) {
    (instance == "*" ? play_all_ : playing_[instance]) = {clip, 0, loop};
}

void UiScreen::Update(double seconds) {
    for (auto& [path, p] : playing_) p.frame += seconds * 60.0;
    play_all_.frame += seconds * 60.0;
}

std::vector<std::string> UiScreen::Layouts() const {
    std::vector<std::string> out;
    for (const auto& [key, l] : layouts_) {
        if (l->prefix.empty()) out.push_back(key);
    }
    return out;
}

void UiScreen::DrawTexts(Renderer& renderer, const std::string& part, const Mat4& to_clip, float alpha) const {
    if (!text_) return;
    const auto model = parts_.find(part);
    if (model == parts_.end()) return;
    const std::vector<std::array<float, 4>> boxes = model->second.model->TextBoxes();
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
    for (auto& [name, part] : parts_) part.model->Release(renderer);
    parts_.clear();
    layouts_.clear();
    playing_.clear();
    play_all_ = {};
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

void UiScreen::DrawPartAt(Renderer& renderer, const std::string& key, const Mat4& to_clip, const std::string& path,
                          float alpha) const {
    const auto it = parts_.find(key);
    if (it == parts_.end()) return;
    const Part& part = it->second;
    // the instance's own clip, else the "*" clip if this part has it
    const auto own = playing_.find(path);
    const Playing& p = own != playing_.end() ? own->second : play_all_;
    const auto c = p.clip.empty() ? part.clips.end() : part.clips.find(p.clip);
    const bmar::Motion* clip = c != part.clips.end() ? &c->second.second : nullptr;
    double frame = p.frame;
    const bool loop = p.loop;
    if (clip && clip->frames > 0) frame = loop ? std::fmod(frame, double(clip->frames)) : std::min(frame, double(clip->frames));
    part.model->DrawUi(renderer, to_clip, alpha, clip, float(frame));
    DrawTexts(renderer, key, to_clip, alpha);
}

void UiScreen::DrawLayoutAt(Renderer& renderer, const std::string& key, const Mat4& to_clip, const std::string& path,
                            float alpha, const std::map<std::string, float>* part_alpha) const {
    const auto it = layouts_.find(key);
    if (it == layouts_.end()) return;
    const Layout& l = *it->second;
    for (size_t i = 0; i < l.model.nodes.size(); ++i) {
        const std::string& name = l.model.nodes[i].name;
        float a = alpha;
        if (part_alpha) {
            if (const auto s = part_alpha->find(name); s != part_alpha->end()) a *= s->second;
        }
        if (a <= 0) continue;
        const std::string& target = l.targets[i];
        if (target.empty()) continue;
        const Mat4 node_clip = to_clip * l.world[i];
        if (layouts_.contains(target)) {
            DrawLayoutAt(renderer, target, node_clip, path + "/" + name, a, nullptr);
        } else {
            DrawPartAt(renderer, target, node_clip, path + "/" + name, a);
        }
    }
}

void UiScreen::DrawPart(Renderer& renderer, const std::string& part, float x, float y, float alpha) const {
    Mat4 at;
    at(0, 3) = x;
    at(1, 3) = y;
    DrawPartAt(renderer, part, CanvasToClip(renderer.Width(), renderer.Height()) * at, part, alpha);
}

void UiScreen::DrawLayout(Renderer& renderer, const std::string& layout, float alpha,
                          const std::map<std::string, float>& part_alpha) const {
    DrawLayoutAt(renderer, layout, CanvasToClip(renderer.Width(), renderer.Height()), layout, alpha, &part_alpha);
}

}
