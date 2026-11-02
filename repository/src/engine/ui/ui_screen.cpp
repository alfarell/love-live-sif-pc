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
    vfs_ = &vfs;
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
    {
        // ll3::ui::s_ui_number_info_table: 24-byte records (u32 part id, u32 box | numtex id << 16,
        // u32 digit count << 16, u32 alignment, u64 default string*); s_ui_numtex_datas: 200-byte records (u32 id,
        // u32 0, u64 texture name*, f32 x0, y0, cell w, h, u64 0, 20 x (f32 advance, u32 0))
        std::vector<uint8_t> nfile, tfile;
        std::vector<blbn::Symbol> nsym, tsym;
        const blbn::Symbol* ntab = nullptr;
        const blbn::Symbol* ttab = nullptr;
        if (vfs.Read(package_ + "/ui_number.bin", nfile) && blbn::Parse(nfile, nsym) &&
            (ntab = blbn::Find(nsym, "ll3::ui::s_ui_number_info_table")) && vfs.Read(package_ + "/numtex_datas.bin", tfile) &&
            blbn::Parse(tfile, tsym) && (ttab = blbn::Find(tsym, "ll3::ui::s_ui_numtex_datas"))) {
            const auto u32 = [](const std::vector<uint8_t>& f, size_t off) {
                uint32_t v = 0;
                if (off + 4 <= f.size()) std::memcpy(&v, &f[off], 4);
                return v;
            };
            const auto f32 = [&](const std::vector<uint8_t>& f, size_t off) {
                const uint32_t v = u32(f, off);
                float x;
                std::memcpy(&x, &v, 4);
                return x;
            };
            const auto str = [](const std::vector<uint8_t>& f, uint64_t off) {
                if (off >= f.size()) return std::string();
                const char* s = reinterpret_cast<const char*>(&f[size_t(off)]);
                return std::string(s, strnlen(s, f.size() - size_t(off)));
            };
            const size_t nbase = size_t(ntab->data.data() - nfile.data());
            for (size_t r = 0; r + 24 <= ntab->data.size(); r += 24) {
                const uint32_t id = u32(nfile, nbase + r), packed = u32(nfile, nbase + r + 4);
                const uint64_t def = uint64_t(u32(nfile, nbase + r + 16)) | uint64_t(u32(nfile, nbase + r + 20)) << 32;
                char prefix[16];
                std::snprintf(prefix, sizeof(prefix), "uip%04u_", id);
                for (const auto& [name, part] : parts_) {
                    if (!name.starts_with(prefix)) continue;
                    numbers_[{name, int(packed & 0xFFFF)}] = {int(packed >> 16), int(u32(nfile, nbase + r + 8) >> 16),
                                                             int(u32(nfile, nbase + r + 12)), str(nfile, def)};
                }
            }
            const size_t tbase = size_t(ttab->data.data() - tfile.data());
            for (size_t r = 0; r + 200 <= ttab->data.size(); r += 200) {
                NumTex t;
                const uint64_t name = uint64_t(u32(tfile, tbase + r + 8)) | uint64_t(u32(tfile, tbase + r + 12)) << 32;
                std::string tex = str(tfile, name);
                // entries are named "x.btx" (or "x" with tag btx): "x.btx" matches both
                if (tex.ends_with(".png")) tex.resize(tex.size() - 4);
                t.texture = GetTexture(package_ + "/texture.pac/" + tex + ".btx");
                t.x0 = f32(tfile, tbase + r + 16);
                t.y0 = f32(tfile, tbase + r + 20);
                t.w = f32(tfile, tbase + r + 24);
                t.h = f32(tfile, tbase + r + 28);
                for (size_t g = 0; g < 20; ++g) t.advance[g] = f32(tfile, tbase + r + 40 + 8 * g);
                if (!t.texture || !t.texture->width || !t.texture->height) continue;
                const float u = t.w / float(t.texture->width), v = t.h / float(t.texture->height);
                const MeshVertex cell[4] = {{{0, 0, 0}, {0, 0, 1}, {0, 0}}, {{1, 0, 0}, {0, 0, 1}, {u, 0}},
                                            {{1, -1, 0}, {0, 0, 1}, {u, v}}, {{0, -1, 0}, {0, 0, 1}, {0, v}}};
                if (renderer.CreateMesh(cell, indices, false, t.glyph, nullptr)) numtex_[int(u32(tfile, tbase + r))] = t;
            }
        }
    }
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

void UiScreen::SetInstanceText(const std::string& instance, int box, const std::string& utf8) {
    TextEntry& t = instance_texts_[{instance, box}];
    if (t.utf8 == utf8 && (t.rendered || utf8.empty())) return;
    if (renderer_ && t.texture.gpu) renderer_->Release(t.texture);
    t = {};
    t.utf8 = utf8;
}

void UiScreen::SetText(const std::string& part, int box, const std::string& utf8) {
    TextEntry& t = texts_[{part, box}];
    if (t.rendered && t.utf8 == utf8) return;
    if (renderer_ && t.texture.gpu) renderer_->Release(t.texture);
    t.utf8 = utf8;
    t.rendered = false;
}

void UiScreen::Play(const std::string& instance, const std::string& clip, bool loop, int layer) {
    if (layer > 0) {
        held_[{instance, layer}].clip = clip;
        return;
    }
    (instance == "*" ? play_all_ : playing_[instance]) = {clip, 0, loop};
}

void UiScreen::SetClipTime(const std::string& instance, int layer, float frame) {
    if (layer > 0) {
        held_[{instance, layer}].frame = frame;
    } else if (const auto it = playing_.find(instance); it != playing_.end()) {
        it->second.frame = frame;
    }
}

std::string UiScreen::ClipName(int id) {
    char name[16];
    std::snprintf(name, sizeof(name), "a%03d", id);
    return name;
}

void UiScreen::HideNode(const std::string& node, bool hide) {
    if (hide) {
        hidden_nodes_.insert(node);
    } else {
        hidden_nodes_.erase(node);
    }
}

void UiScreen::HideNode(const std::string& instance, const std::string& node, bool hide) {
    if (hide) {
        instance_hidden_[instance].insert(node);
    } else if (const auto it = instance_hidden_.find(instance); it != instance_hidden_.end()) {
        it->second.erase(node);
    }
}

const Texture* UiScreen::GetTexture(const std::string& vfs_path) {
    auto it = loaded_textures_.find(vfs_path);
    if (it == loaded_textures_.end()) {
        std::vector<uint8_t> file;
        btx::Image image;
        Texture texture;
        std::string why;
        if (!vfs_ || !renderer_ || !vfs_->Read(vfs_path, file, &why) || !btx::Parse(file, image, &why) ||
            !renderer_->Upload(image, texture, &why)) {
            LogWarn("ui: texture {}: {}", vfs_path, why);
            return nullptr;
        }
        it = loaded_textures_.emplace(vfs_path, texture).first;
    }
    return &it->second;
}

bool UiScreen::SetMaterialTexture(const std::string& instance, const std::string& material, const std::string& vfs_path) {
    const Texture* t = GetTexture(vfs_path);
    if (t) instance_textures_[instance][material] = t;
    return t != nullptr;
}

void UiScreen::SetMaterialUv(const std::string& instance, const std::string& material, float u, float v) {
    const auto it = instance_uvs_[instance].try_emplace(material, std::array<float, 3>{0, 0, 1e9f}).first;
    it->second[0] = u;
    it->second[1] = v;
}

void UiScreen::SetMaterialClip(const std::string& instance, const std::string& material, float u_max) {
    instance_uvs_[instance].try_emplace(material, std::array<float, 3>{0, 0, 1e9f}).first->second[2] = u_max;
}

void UiScreen::SetInstanceNumber(const std::string& instance, int box, const std::string& digits) {
    instance_numbers_[{instance, box}] = digits;
}

namespace {

// Glyph slot of a number-box character: '0'-'9' are 0-9; res_record_num00's clear-rate row also has '%' in slot 15 and
// '.' in slot 18 (their advances in s_ui_numtex_datas are the digits' width). -1: not drawn.
int GlyphSlot(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    return c == '%' ? 15 : c == '.' ? 18 : -1;
}

}

void UiScreen::DrawNumbers(Renderer& renderer, const std::string& part, const std::string& path, const Mat4& to_clip,
                           float alpha) const {
    const auto model = parts_.find(part);
    if (model == parts_.end()) return;
    std::vector<std::array<float, 4>> boxes;
    for (auto it = numbers_.lower_bound({part, 0}); it != numbers_.end() && it->first.first == part; ++it) {
        if (boxes.empty()) boxes = model->second.model->TextBoxes();
        const int box = it->first.second;
        const NumberBox& spec = it->second;
        const auto tex = numtex_.find(spec.numtex);
        if (tex == numtex_.end() || box >= int(boxes.size())) continue;
        const NumTex& nt = tex->second;
        const auto own = instance_numbers_.find({path, box});
        const std::string& value = own != instance_numbers_.end() ? own->second : spec.value;
        const auto& b = boxes[size_t(box)];  // x0, y0, x1, y1 (y up)
        const float scale = (b[3] - b[1]) / nt.h;
        float width = 0;
        for (const char c : value) {
            if (GlyphSlot(c) >= 0) width += nt.advance[size_t(GlyphSlot(c))] * scale;
        }
        float x = spec.align == 0 ? b[0] : spec.align == 1 ? (b[0] + b[2] - width) / 2 : b[2] - width;
        for (const char c : value) {
            const int d = GlyphSlot(c);
            if (d < 0) continue;
            const float advance = nt.advance[size_t(d)] * scale;
            // the glyph cell is centred on its advance
            Mat4 place;
            place(0, 0) = nt.w * scale;
            place(1, 1) = nt.h * scale;
            place(0, 3) = x + (advance - nt.w * scale) / 2;
            place(1, 3) = b[3];
            // glyphs run left to right and wrap at the texture's width (res_judge_num00: 0-4 over 5-9)
            const int cols = std::max(1, int((float(nt.texture->width) - nt.x0) / nt.w));
            const std::array<float, 2> uv{(nt.x0 + float(d % cols) * nt.w) / float(nt.texture->width),
                                          (nt.y0 + float(d / cols) * nt.h) / float(nt.texture->height)};
            renderer.DrawUiMesh(nt.glyph, nt.texture, to_clip * place, {1, 1, 1, alpha}, uv);
            x += advance;
        }
    }
}

void UiScreen::HideInstance(const std::string& path, bool hide) {
    if (hide) {
        hidden_paths_.insert(path);
    } else {
        hidden_paths_.erase(path);
    }
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

void UiScreen::Render(Renderer& renderer, TextEntry& t, const TextStyle& style, float px) const {
    t.rendered = true;
    TextBitmap bitmap;
    if (!text_->Render(style, t.utf8, px, bitmap)) return;
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

void UiScreen::DrawTexts(Renderer& renderer, const std::string& part, const std::string& path, const Mat4& to_clip,
                         float alpha) const {
    if (!text_) return;
    const auto model = parts_.find(part);
    if (model == parts_.end()) return;
    const std::vector<std::array<float, 4>> boxes = model->second.model->TextBoxes();
    for (auto it = texts_.lower_bound({part, 0}); it != texts_.end() && it->first.first == part; ++it) {
        const int box = it->first.second;
        const TextStyle* style = text_->Style(it->second.style);
        const auto own = instance_texts_.find({path, box});
        TextEntry& t = own != instance_texts_.end() ? own->second : it->second;
        if (!style || box >= int(boxes.size()) || t.utf8.empty()) continue;
        const auto& b = boxes[size_t(box)];  // x0, y0, x1, y1 (y up)
        if (!t.rendered) {  // size 0 fits the box: its height shared by the lines
            const float lines = float(1 + std::count(t.utf8.begin(), t.utf8.end(), '\n'));
            Render(renderer, t, *style, style->size > 0 ? style->size : (b[3] - b[1]) / lines);
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
    for (auto& [key, t] : instance_texts_) renderer.Release(t.texture);
    texts_.clear();
    instance_texts_.clear();
    for (auto& [id, t] : numtex_) renderer.Release(t.glyph);
    numtex_.clear();
    numbers_.clear();
    instance_numbers_.clear();
    renderer.Release(unit_quad_);
    for (auto& [name, part] : parts_) part.model->Release(renderer);
    parts_.clear();
    layouts_.clear();
    playing_.clear();
    held_.clear();
    instance_hidden_.clear();
    instance_textures_.clear();
    instance_uvs_.clear();
    for (auto& [path, t] : loaded_textures_) renderer.Release(t);
    loaded_textures_.clear();
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
    std::vector<bmar::Layer> layers;
    if (c != part.clips.end() && c->second.second.frames > 0) {
        const double length = c->second.second.frames;
        layers.push_back({&c->second.second, float(p.loop ? std::fmod(p.frame, length) : std::min(p.frame, length))});
    }
    for (auto h = held_.lower_bound({path, 1}); h != held_.end() && h->first.first == path; ++h) {
        if (const auto hc = part.clips.find(h->second.clip); hc != part.clips.end()) {
            // a held time past the clip's end wraps (Hypothesis: the top menu asks the 6-label wheel clip, frames 0-5,
            // for frames 3-10)
            const float length = hc->second.second.frames;
            const float frame = h->second.frame > length ? std::fmod(h->second.frame, length + 1) : h->second.frame;
            layers.push_back({&hc->second.second, frame});
        }
    }
    const std::set<std::string>* hidden = &hidden_nodes_;
    std::set<std::string> merged;
    if (const auto own = instance_hidden_.find(path); own != instance_hidden_.end() && !own->second.empty()) {
        merged = hidden_nodes_;
        merged.insert(own->second.begin(), own->second.end());
        hidden = &merged;
    }
    const auto textures = instance_textures_.find(path);
    const auto uvs = instance_uvs_.find(path);
    part.model->DrawUi(renderer, to_clip, alpha, layers, hidden,
                       textures != instance_textures_.end() ? &textures->second : nullptr,
                       uvs != instance_uvs_.end() ? &uvs->second : nullptr);
    DrawTexts(renderer, key, path, to_clip, alpha);
    DrawNumbers(renderer, key, path, to_clip, alpha);
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
        if (target.empty() || hidden_paths_.contains(path + "/" + name)) continue;
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
