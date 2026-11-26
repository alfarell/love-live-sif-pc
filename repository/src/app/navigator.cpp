#include "app/navigator.h"

#include <algorithm>
#include <cmath>

#include "engine/core/log.h"
#include "engine/fs/vfs.h"
#include "engine/render/renderer.h"

namespace sif {
namespace {

std::string FirstEntry(const Vfs& vfs, const std::string& folder, const std::string& prefix) {
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List(folder, entries)) return {};
    for (const pac::ArcEntry& e : entries) {
        if (e.name.starts_with(prefix)) return folder + "/" + e.name;
    }
    return {};
}

int test_face = -1;  // --face
constexpr int kFacialBase = 100000;  // facial clips' keys in clips_
// blinks (FUN_0002b760, U1 §5): clip 100 after 300 frames, then every 60-300 frames
constexpr int kBlinkClip = 100, kFirstBlink = 300, kBlinkMin = 60, kBlinkMax = 300;
constexpr int kTalk[] = {503, 504, 505, 506};  // talk1-4 (shortcut: which talk motion goes with which line is not traced)

}

bool Navigator::Load(Renderer& renderer, const Vfs& vfs, int member, int costume, bool special, std::string* error) {
    if (loaded_) Release(renderer);  // a reload: another member, costume or colour
    clips_.clear(), playing_ = face_ = default_ = blink_ = nullptr;
    vfs_ = &vfs, member_ = member;
    char chr[64], prefix[16];
    std::snprintf(chr, sizeof(chr), "chr/c%02d/c%02d.pac/model.pac", member, member);
    std::snprintf(prefix, sizeof(prefix), "mod_%02d_", member);
    std::string path = FirstEntry(vfs, chr, prefix), textures;
    const std::string own = path;  // her own model: the face's blend shapes (M18 U4)
    if (costume < 0) costume = member::DefaultCostume(member::Group(member));
    // the costume pac holds the whole member in that costume; face and hair textures stay in her own model.pac
    for (const bool colour : {special, false}) {
        const std::string dressed = FirstEntry(vfs, member::CostumePack(member, costume, colour), "mod_");
        if (!dressed.empty()) {
            path = dressed, textures = chr;
            break;
        }
        if (colour) LogWarn("navigator: member {} costume {} has no special colour", member, costume);
    }
    if (path.empty() || !model_.Load(renderer, vfs, path, error, textures, own)) return false;
    loaded_ = true;
    std::string idle_error;
    if (!member::LoadIdle(vfs, idle_, &idle_error)) LogWarn("navigator: {}", idle_error);
    Start(member::kStandMotion);
    wait_ = member::IdleWait(idle_, random_.Next() >> 16);
    LogInfo("navigator: member {} in {}{}", member, path, special ? " (special colour)" : "");
    default_ = Facial(0);
    blink_wait_ = kFirstBlink;
    if (test_face >= 0) Face(test_face);
    return true;
}

const Navigator::Clip* Navigator::Get(int number) {
    for (const auto& c : clips_) {
        if (c->number == number) return c->motion.frames > 0 ? c.get() : nullptr;
    }
    auto c = std::make_unique<Clip>();  // the parsed motion points into its file: keep both in place
    c->number = number;
    const std::string path = member::MotionPath(*vfs_, member_, number);
    std::string error;
    if (path.empty() || !vfs_->Read(path, c->file, &error) || !bmar::ParseMotion(c->file, c->motion, &error)) {
        LogWarn("navigator: member {} motion {}: {}", member_, number, path.empty() ? "none" : error);
        c->motion = {};
    }
    clips_.push_back(std::move(c));
    return clips_.back()->motion.frames > 0 ? clips_.back().get() : nullptr;
}

const Navigator::Clip* Navigator::Facial(int number) {
    for (const auto& c : clips_) {
        if (c->number == kFacialBase + number) return c->motion.frames > 0 ? c.get() : nullptr;
    }
    auto c = std::make_unique<Clip>();
    c->number = kFacialBase + number;
    const std::string path = member::FacialPath(*vfs_, member_, number);
    std::string error;
    if (path.empty() || !vfs_->Read(path, c->file, &error) || !bmar::ParseMotion(c->file, c->motion, &error)) {
        LogWarn("navigator: member {} facial {}: {}", member_, number, path.empty() ? "none" : error);
        c->motion = {};
    }
    clips_.push_back(std::move(c));
    return clips_.back()->motion.frames > 0 ? clips_.back().get() : nullptr;
}

void Navigator::Face(int number) {
    if (!loaded_) return;
    face_ = number < 0 ? nullptr : Facial(number);
    LogInfo("navigator: face {}", face_ ? number : -1);
}

void Navigator::TestFace(int number) { test_face = number; }

void Navigator::Start(int number) {
    const Clip* c = Get(number);
    if (!c && number != member::kStandMotion) c = Get(member::kStandMotion);
    playing_ = c, frame_ = 0;
}

void Navigator::Update(double seconds) {
    if (!loaded_ || !playing_) return;
    frame_ += float(seconds * 60.0);
    if (!mouth_.empty() && (mouth_frame_ += float(seconds * 60.0)) > mouth_.back().frame + mouth_.back().blend + 1) {
        mouth_.clear();
    }
    if (blink_) {
        blink_frame_ += float(seconds * 60.0);
        if (blink_frame_ >= blink_->motion.frames) blink_ = nullptr;
    } else if ((blink_wait_ -= float(seconds * 60.0)) <= 0) {
        // shortcut: how the game draws in its range is not traced; this is min..max inclusive
        blink_wait_ = float(kBlinkMin + random_.Below(kBlinkMax - kBlinkMin + 1));
        blink_ = Facial(kBlinkClip), blink_frame_ = 0;
        LogInfo("navigator: blink");
    }
    const bool standing = playing_->number == member::kStandMotion;
    // shortcut: clips switch at once; the game blends over s_common_param field 2 (60 frames)
    if (standing) {
        frame_ = std::fmod(frame_, playing_->motion.frames);  // stand loops
        wait_ -= float(seconds * 60.0);
        if (wait_ <= 0) {
            Start(member::PickIdle(idle_, member::Group(member_), random_.Next() >> 16));
            LogInfo("navigator: idle {}", playing_ ? playing_->number : 0);
        }
    } else if (frame_ >= playing_->motion.frames) {
        Start(member::kStandMotion);
        wait_ = member::IdleWait(idle_, random_.Next() >> 16);
    }
}

void Navigator::Talk(int motion, std::vector<talk::MouthKey> mouth) {
    if (!loaded_) return;
    Start(motion >= 0 ? motion : kTalk[random_.Below(4)]);
    mouth_ = std::move(mouth), mouth_frame_ = 0;
    LogInfo("navigator: talk {}, {} mouth keys", playing_ ? playing_->number : 0, mouth_.size());
}

void Navigator::Play(int number) {
    if (!loaded_) return;
    Start(number);
    LogInfo("navigator: motion {}", playing_ ? playing_->number : 0);
}

void Navigator::Draw(Renderer& renderer, const Mat4& view_proj) {
    if (!loaded_) return;
    renderer.SetCamera(view_proj);
    std::vector<bmar::Layer> layers;
    if (playing_) layers.push_back({&playing_->motion, frame_});
    if (default_) layers.push_back({&default_->motion, default_->motion.frames});
    if (face_) layers.push_back({&face_->motion, face_->motion.frames});
    if (blink_) layers.push_back({&blink_->motion, blink_frame_});
    std::vector<std::pair<int, float>> shapes;
    if (!mouth_.empty()) {
        const std::array<float, 8> w = talk::MouthAt(mouth_, mouth_frame_);
        float open = 0;
        for (int s = 1; s < 8; ++s) shapes.push_back({s, w[size_t(s)]}), open = std::max(open, w[size_t(s)]);
        // shortcut: `default` (shape 0, the closed mouth) yields to an open mouth; the game's facial layers and modes
        // (facial_datas.bin) are not traced
        if (default_) shapes.push_back({0, 1 - open});
    }
    model_.Pose(renderer, layers, shapes);
    model_.Draw(renderer, Mat4{});
}

void Navigator::Release(Renderer& renderer) {
    model_.Release(renderer);
    loaded_ = false;
}

Mat4 Navigator::MenuCamera(float aspect) {
    // N2 shots (work/m13/shad/s01.png, umi.png): head top near y 150 of 720, cut at the thighs, her middle at x 0.68 of
    // the width. shortcut: fitted by eye, the game's menu camera is not traced
    const float fov = 0.55f, x = -0.44f, z = 1.17f;
    return Perspective(fov, aspect, 0.05f, 100.0f) * LookAt({x, -2.2f, z}, {x, 0, z}, {0, 0, 1});
}

Mat4 Navigator::CostumeCamera(float aspect) {
    // N2 shots cs2.png-cs6.png: head near y 120 of 720, feet near y 670, her middle at x 0.17 of the width.
    // shortcut: fitted by eye, as MenuCamera
    const float fov = 0.55f, x = 1.44f, z = 1.01f;
    return Perspective(fov, aspect, 0.05f, 100.0f) * LookAt({x, -4.18f, z}, {x, 0, z}, {0, 0, 1});
}

}
