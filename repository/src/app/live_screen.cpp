#include "app/live_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "app/result_screen.h"
#include "engine/audio/audio_output.h"
#include "engine/core/log.h"
#include "engine/platform/settings.h"
#include "engine/formats/bscm.h"
#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"
#include "engine/ui/ui_screen.h"
#include "engine/render/renderer.h"
#include "engine/render/scene_model.h"
#include "game/live/song.h"

namespace sif {
namespace {

using live::Grade;
using live::kLanes;

// lanes left to right (lane 0 = note number 55, Hypothesis): A S D F Space J K L ;
constexpr SDL_Scancode kLaneKeys[kLanes] = {SDL_SCANCODE_A,     SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
                                            SDL_SCANCODE_SPACE, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,
                                            SDL_SCANCODE_SEMICOLON};

int LaneOf(SDL_Scancode key) {
    for (int i = 0; i < kLanes; ++i) {
        if (kLaneKeys[i] == key) return i;
    }
    return -1;
}

struct Layout {
    float cx = 0, cy = 0, radius = 0, button = 0;
    void Fit(int w, int h) {
        // SIF layout: notes start at a point near the top centre and travel to 9 buttons on a lower half circle
        radius = std::min(float(w) * 0.42f, float(h) * 0.72f);
        cx = float(w) / 2;
        cy = float(h) * 0.18f;
        button = radius * 0.11f;
    }
    void Lane(int lane, float& x, float& y) const {
        const float a = 3.14159265f * (1.0f - float(lane) / float(kLanes - 1));  // pi (left) .. 0 (right), lower arc
        x = cx + radius * std::cos(a);
        y = cy + radius * std::sin(a);
    }
};

// The game's play field (ui/ui_ingame.pac): positions from layout.pac/ui_ingame_layout.bin (BLBN "layout_datas",
// 32-byte records: u32, u32, u64 name*, f32 rect x0 y0 x1 y1 in 1920 x 1080 pixels), sprites from the 5 x 6 atlas
// texture.pac/ui_ingame_notes (256-pixel cells). shortcut: which atlas cell the game uses for buttons, notes and
// holds is not traced (the eboot's note drawing and the .fad animations are not decoded); the cells below are
// picked by eye
struct Playfield {
    static constexpr float kCell = 256, kAtlasW = 1280, kAtlasH = 1536;
    Texture atlas, center_gauge, center_mic, combo_tex;
    GpuMesh ring, note, trail, combo_label, gauge_fan;
    float gauge_progress = 0;
    std::array<GpuMesh, 10> digits;
    std::array<std::array<float, 2>, kLanes> lane{};  // canvas x, y (y down)
    std::array<float, 2> center{958, 146};
    float ring_size = 152, center_size = 288;
    bool loaded = false;

    static bool LoadTexture(Renderer& r, const Vfs& vfs, const std::string& path, Texture& out) {
        std::vector<uint8_t> file;
        btx::Image image;
        return vfs.Read(path, file) && btx::Parse(file, image) && r.Upload(image, out, nullptr);
    }
    // A unit quad centred on the origin showing the UV rectangle (u0, v0)-(u1, v1)
    static bool Quad(Renderer& r, float u0, float v0, float u1, float v1, GpuMesh& out) {
        const MeshVertex quad[4] = {{{-0.5f, 0.5f, 0}, {0, 0, 1}, {u0, v0}}, {{0.5f, 0.5f, 0}, {0, 0, 1}, {u1, v0}},
                                    {{0.5f, -0.5f, 0}, {0, 0, 1}, {u1, v1}}, {{-0.5f, -0.5f, 0}, {0, 0, 1}, {u0, v1}}};
        const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        return r.CreateMesh(quad, indices, false, out, nullptr);
    }
    static bool Cell(Renderer& r, int col, int row, GpuMesh& out) {
        return Quad(r, col * kCell / kAtlasW, row * kCell / kAtlasH, (col + 1) * kCell / kAtlasW,
                    (row + 1) * kCell / kAtlasH, out);
    }
    // ui_ingame_combo_num (840 x 200): digits 0-9 in 84 x 100 cells on the top row, the "COMBO" label below
    // (measured from the alpha: about (0, 136)-(184, 196))
    static constexpr float kComboW = 840, kComboH = 200, kDigitW = 84, kDigitH = 100;
    static constexpr float kLabelW = 184, kLabelY0 = 136, kLabelY1 = 196;
    bool LoadCombo(Renderer& r, const Vfs& vfs) {
        if (!LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_combo_num", combo_tex)) return false;
        for (int d = 0; d < 10; ++d) {
            if (!Quad(r, d * kDigitW / kComboW, 0, (d + 1) * kDigitW / kComboW, kDigitH / kComboH, digits[size_t(d)])) return false;
        }
        return Quad(r, 0, kLabelY0 / kComboH, kLabelW / kComboW, kLabelY1 / kComboH, combo_label);
    }
    // The centre circle's ring as a song-progress gauge: a fan from the centre over the first `progress` of a turn,
    // clockwise from 12 o'clock (shortcut: direction and start are guesses, the game's gauge code is not traced)
    static constexpr int kFanSegments = 64;
    static constexpr float kGaugeSize = 188;  // the HUD part's quad, canvas pixels
    std::array<MeshVertex, kFanSegments + 2> FanVertices(float progress) const {
        std::array<MeshVertex, kFanSegments + 2> v{};
        v[0] = {{0, 0, 0}, {0, 0, 1}, {0.5f, 0.5f}};
        for (int i = 0; i <= kFanSegments; ++i) {
            const float a = 6.2831853f * std::clamp(progress, 0.0f, 1.0f) * float(i) / kFanSegments;
            const float x = 0.5f * std::sin(a), y = 0.5f * std::cos(a);  // the inscribed circle covers the ring
            v[size_t(i) + 1] = {{x, y, 0}, {0, 0, 1}, {0.5f + x, 0.5f - y}};
        }
        return v;
    }
    bool LoadGauge(Renderer& r) {
        std::array<uint16_t, kFanSegments * 3> indices{};
        for (int i = 0; i < kFanSegments; ++i) {
            indices[size_t(i) * 3] = 0;
            indices[size_t(i) * 3 + 1] = uint16_t(i + 1);
            indices[size_t(i) * 3 + 2] = uint16_t(i + 2);
        }
        const auto v = FanVertices(0);
        return r.CreateMesh(v, indices, true, gauge_fan, nullptr);
    }
    // Call outside a frame (UpdateMesh)
    void SetGauge(Renderer& r, float progress) {
        if (std::abs(progress - gauge_progress) < 1.0f / 2048) return;
        gauge_progress = progress;
        const auto v = FanVertices(progress);
        r.UpdateMesh(gauge_fan, v);
    }
    bool Load(Renderer& r, const Vfs& vfs) {
        std::vector<uint8_t> file;
        std::vector<blbn::Symbol> symbols;
        if (!vfs.Read("ui/ui_ingame.pac/layout.pac/ui_ingame_layout.bin", file) || !blbn::Parse(file, symbols)) return false;
        const blbn::Symbol* table = blbn::Find(symbols, "layout_datas");
        if (!table) return false;
        const size_t base = size_t(table->data.data() - file.data());
        for (size_t o = base; o + 32 <= base + table->data.size(); o += 32) {
            uint64_t name = 0;
            float rect[4];
            std::memcpy(&name, &file[o + 8], 8);
            std::memcpy(rect, &file[o + 16], 16);
            if (name >= file.size()) continue;
            const std::string n(reinterpret_cast<const char*>(&file[size_t(name)]));
            const float cx = (rect[0] + rect[2]) / 2, cy = (rect[1] + rect[3]) / 2;
            int ring_no = 0;
            if (std::sscanf(n.c_str(), "layout_judge_ring%d", &ring_no) == 1 && ring_no >= 1 && ring_no <= kLanes) {
                lane[size_t(ring_no - 1)] = {cx, cy};  // ring01 .. ring09 = lanes 0 .. 8, left to right
                ring_size = rect[2] - rect[0];
            } else if (n == "layout_center_circle") {
                center = {cx, cy};
                center_size = rect[2] - rect[0];
            }
        }
        loaded = LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_notes", atlas) &&
                 // the HUD song gauge's own colour ring (ui2/90_ingame part uip0006_Songgage, 188 px), so the fill
                 // lines up with the HUD's track
                 LoadTexture(r, vfs, "ui2/90_ingame.pac/uip0006_Songgage.pac/texture.pac/IngamesonggageCol.btx", center_gauge) &&
                 LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_centercircle_mic_aq", center_mic) &&
                 Cell(r, 1, 0, ring) && Cell(r, 0, 1, note) && Cell(r, 2, 0, trail) && LoadGauge(r);
        if (!loaded) LogWarn("live: game play field not loaded; drawing the placeholder lanes");
        if (loaded && !LoadCombo(r, vfs)) LogWarn("live: combo digits not loaded");
        return loaded;
    }
    // A sprite centred at canvas pixel (x, y), y downwards
    void Draw(Renderer& r, const GpuMesh& mesh, const Texture& texture, float x, float y, float size,
              const std::array<float, 4>& tint) const {
        Draw(r, mesh, texture, x, y, size, size, tint);
    }
    void Draw(Renderer& r, const GpuMesh& mesh, const Texture& texture, float x, float y, float w, float h,
              const std::array<float, 4>& tint) const {
        Mat4 m;
        m(0, 0) = w;
        m(1, 1) = h;
        m(0, 3) = x;
        m(1, 3) = -y;
        r.DrawUiMesh(mesh, &texture, UiScreen::CanvasToClip(r.Width(), r.Height()) * m, tint);
    }
    // The combo count centred at canvas x, digits centred on `y`, the label under them (shortcut: position, glyph
    // spacing and the "from 2 notes" rule are guesses; the game's combo drawing is not traced)
    void DrawCombo(Renderer& r, int combo, float x, float y) const {
        if (!combo_tex.width || combo < 2) return;
        const std::string text = std::to_string(combo);
        const float advance = kDigitW * 0.86f;
        float left = x - advance * float(text.size()) / 2 + advance / 2;
        for (const char c : text) {
            Draw(r, digits[size_t(c - '0')], combo_tex, left, y, kDigitW, kDigitH, {1, 1, 1, 1});
            left += advance;
        }
        Draw(r, combo_label, combo_tex, x, y + kDigitH * 0.5f + (kLabelY1 - kLabelY0) * 0.4f, kLabelW,
             kLabelY1 - kLabelY0, {1, 1, 1, 1});
    }
    void Release(Renderer& r) {
        for (Texture* t : {&atlas, &center_gauge, &center_mic, &combo_tex}) r.Release(*t);
        for (GpuMesh* m : {&ring, &note, &trail, &combo_label, &gauge_fan}) r.Release(*m);
        for (GpuMesh& m : digits) r.Release(m);
    }
};

// First entry in an archive folder whose name starts with `prefix` ("" if none).
std::string FindEntry(const Vfs& vfs, const std::string& folder, const std::string& prefix) {
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List(folder, entries)) return {};
    for (const pac::ArcEntry& e : entries) {
        if (e.name.starts_with(prefix)) return folder + "/" + e.name;
    }
    return {};
}

// Stage, the members dancing the song, and the song's camera; missing parts are logged and skipped. Each member's
// dance is authored at that member's spot on the stage (the root moves to the formation), so all are drawn unplaced.
struct LiveScene {
    struct Dancer {
        int member = 0;
        SceneModel model;
        std::vector<uint8_t> motion_file;
        bmar::Motion dance;
        bool has_dance = false;
    };
    SceneModel stage;
    bool has_stage = false;
    struct Loop {
        std::string name;
        std::vector<uint8_t> file;
        bmar::Motion motion;
    };
    std::vector<std::unique_ptr<Loop>> stage_loops;  // stage/stNNN.pac/motion.pac, played looped on the dance clock
    std::vector<std::unique_ptr<Dancer>> dancers;
    std::vector<uint8_t> camera_file;
    bscm::Camera camera;
    bool has_camera = false;

    void Load(Renderer& renderer, const Vfs& vfs, const LiveOptions& opt) {
        std::string error;
        const std::string stage_path = FindEntry(vfs, "stage/" + opt.stage + ".pac/model.pac", "");
        has_stage = !stage_path.empty() && stage.Load(renderer, vfs, stage_path, &error);
        if (!has_stage) LogWarn("live: stage {}: {}", opt.stage, stage_path.empty() ? "not found" : error);
        // shortcut: every stage loop plays from clock 0; where variants move the same nodes (yukalight A1/A2/B1/B2) the
        // first in the archive wins. The song's stage timeline (scene_stage_<song>_00.btlb, BTL) that picks them is
        // not decoded, nor are the material tracks (ANMT) that most loops carry
        std::vector<pac::ArcEntry> loop_entries;
        std::set<std::string> claimed;  // stage nodes already moved by an earlier loop
        if (has_stage && vfs.List("stage/" + opt.stage + ".pac/motion.pac", loop_entries)) {
            for (const pac::ArcEntry& e : loop_entries) {
                auto loop = std::make_unique<Loop>();
                loop->name = e.name;
                if (!vfs.Read("stage/" + opt.stage + ".pac/motion.pac/" + e.name, loop->file) ||
                    !bmar::ParseMotion(loop->file, loop->motion) || loop->motion.frames <= 0) {
                    continue;  // some carry only material tracks (no ANSK)
                }
                bool clash = false, moves = false;
                for (const bmar::Track& t : loop->motion.tracks) {
                    const bool in_stage = std::any_of(stage.Data().nodes.begin(), stage.Data().nodes.end(),
                                                      [&](const bmar::Node& n) { return n.name == t.node; });
                    moves |= in_stage && t.node != "model_root";
                    clash |= in_stage && t.node != "model_root" && claimed.contains(t.node);
                }
                if (!moves || clash) continue;
                for (const bmar::Track& t : loop->motion.tracks) {
                    if (t.node != "model_root") claimed.insert(t.node);
                }
                stage_loops.push_back(std::move(loop));
            }
        }
        for (int m = opt.member ? opt.member : 1; m <= (opt.member ? opt.member : 9); ++m) {
            char chr[64], mot[64], prefix[32];
            std::snprintf(chr, sizeof(chr), "chr/c%02d/c%02d.pac/model.pac", m, m);
            std::snprintf(mot, sizeof(mot), "live/%04d/c%02d_%04d.pac", opt.song, m, opt.song);
            std::snprintf(prefix, sizeof(prefix), "mod_%02d_", m);
            const std::string model_path = FindEntry(vfs, chr, prefix), motion_path = FindEntry(vfs, mot, "mot_");
            if (motion_path.empty()) continue;  // not in this song
            auto d = std::make_unique<Dancer>();
            d->member = m;
            if (model_path.empty() || !d->model.Load(renderer, vfs, model_path, &error)) {
                LogWarn("live: member {}: {}", m, model_path.empty() ? "no model" : error);
                continue;
            }
            d->has_dance = vfs.Read(motion_path, d->motion_file, &error) && bmar::ParseMotion(d->motion_file, d->dance, &error);
            if (!d->has_dance) LogWarn("live: dance {}: {}", motion_path, error);
            dancers.push_back(std::move(d));
        }
        char cam[96];
        std::snprintf(cam, sizeof(cam), "live/%04d/%04d.pac/scene.pac/camera_anim_%04d_00_main.bscam", opt.song, opt.song, opt.song);
        has_camera = vfs.Read(cam, camera_file, &error) && bscm::Parse(camera_file, camera, &error);
        if (!has_camera) LogWarn("live: camera {}: {}", cam, error);
        std::string loops;
        for (const auto& l : stage_loops) loops += (loops.empty() ? "" : ", ") + l->name;
        LogInfo("live scene: stage {} ({}; loops: {}), {} members dancing, camera {}", opt.stage, stage_path,
                loops.empty() ? "none" : loops, dancers.size(), has_camera ? cam : "fixed");
    }

    // `now_ms`: audio time. The dance (and camera) clock runs `start_frame` ahead of the music: frames before it are
    // the stage intro, which the port skips (shortcut: the music starts at once; play the intro first when the
    // pre-music flow is built)
    void Draw(Renderer& renderer, double now_ms, float start_frame) {
        const float frame = float(std::max(0.0, now_ms) / 1000.0 * 60.0) + start_frame;  // dance and camera clock, 60 fps
        for (auto& d : dancers) d->model.Pose(renderer, d->has_dance ? &d->dance : nullptr, frame);
        if (has_stage && !stage_loops.empty()) {
            std::vector<bmar::Layer> layers;
            for (const auto& l : stage_loops) layers.push_back({&l->motion, std::fmod(frame, l->motion.frames)});
            stage.Pose(renderer, layers);
        }
        const float aspect = float(renderer.Width()) / float(std::max(1, renderer.Height()));
        if (has_camera) {
            const bscm::Pose pose = bscm::Sample(camera, frame);
            renderer.SetCamera(Perspective(pose.fov_deg * 3.14159265f / 180.0f, aspect, 0.1f, 500.0f) * bscm::ViewMatrix(pose));
        } else {  // fixed front view; Z-up world, models face -Y
            renderer.SetCamera(Perspective(0.70f, aspect, 0.1f, 500.0f) * LookAt({0, -6.0f, 1.4f}, {0, 0, 1.0f}, {0, 0, 1}));
        }
        if (has_stage) stage.Draw(renderer, Mat4{});
        for (auto& d : dancers) d->model.Draw(renderer, Mat4{});
    }

    void Release(Renderer& renderer) {
        stage.Release(renderer);
        for (auto& d : dancers) d->model.Release(renderer);
    }
};

const float kGradeColor[live::kGrades][4] = {
    {1.0f, 0.85f, 0.2f, 1}, {1.0f, 0.45f, 0.7f, 1}, {0.4f, 0.9f, 0.5f, 1}, {0.35f, 0.5f, 1.0f, 1}, {0.5f, 0.5f, 0.55f, 1}};

}

int RunLive(SDL_Window* window, Renderer& renderer, AudioOutput& audio, const Vfs& vfs, const AppSettings& settings,
            const LiveOptions& opt, LiveResult* result) {
    live::Song song;
    cri::Pcm music;
    std::string error;
    if (!live::LoadSong(vfs, opt.song, opt.variant, song, &error) || !live::LoadSongMusic(vfs, song, music, &error)) {
        LogError("live {:04d}/{:02d}: {}", opt.song, opt.variant, error);
        return 1;
    }
    LogInfo("live {:04d}/{:02d} '{}': chart {}, {} notes, lead-in {:.0f} ms, {:.0f} BPM, {:.1f} s audio, offset {} ms{}",
            opt.song, opt.variant, song.title, song.chart_name, song.chart.notes.size(), song.chart.lead_in_ms,
            song.chart.first_bpm, double(music.Frames()) / music.rate, settings.timing.audio_offset_ms,
            opt.autoplay ? ", autoplay" : "");
    const live::Chart& chart = song.chart;
    const double audio_ms = std::max(1.0, double(music.Frames()) * 1000.0 / music.rate);
    live::Judge judge;
    judge.Reset(chart, song.windows, song.rules);
    const double offset = settings.timing.audio_offset_ms;
    LiveScene scene;
    scene.Load(renderer, vfs, opt);
    audio.Play(std::move(music));

    Layout layout;
    Playfield field;
    field.Load(renderer, vfs);
    // the game's top HUD bar (ui2/90_ingame, layout uil0001_Ingame00); shortcut: gauges and the FULL COMBO marks
    // stay at their defaults until the HUD logic is traced
    long hud_score = -1;
    UiText hud_text;
    UiScreen hud;
    bool has_hud = hud_text.Load(vfs, opt.lang, &error) && hud.Load(renderer, vfs, "ui2/90_ingame", opt.lang, &error, &hud_text);
    if (has_hud) {
        hud.HideInstance("uil0001_Ingame00/uip0008_FinaleON00", true);
        hud.HideNode("IngamesonggageCol", true);  // the play field draws the filled part of this ring
        for (int k = 0; k < 9; ++k) hud.HideInstance("uil0001_Ingame00/uip0007_FFmark_ef_0" + std::to_string(k), true);
        std::vector<live::SongInfo> songs;
        const int language = opt.lang == "en" ? 1 : opt.lang == "cn" ? 2 : opt.lang == "tw" ? 3 : 0;
        if (live::LoadSongList(vfs, language, songs)) {
            for (const live::SongInfo& s : songs) {
                if (s.id == opt.song) hud.SetInstanceText("uil0001_Ingame00/uip0005_Songname", 1, s.title);
            }
        }
    } else {
        LogWarn("live: HUD: {}", error);
    }
    GpuMesh unit_quad;  // (0,0)-(1,-1) for the centre-circle textures
    {
        const MeshVertex quad[4] = {{{-0.5f, 0.5f, 0}, {0, 0, 1}, {0, 0}}, {{0.5f, 0.5f, 0}, {0, 0, 1}, {1, 0}},
                                    {{0.5f, -0.5f, 0}, {0, 0, 1}, {1, 1}}, {{-0.5f, -0.5f, 0}, {0, 0, 1}, {0, 1}}};
        const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        renderer.CreateMesh(quad, indices, false, unit_quad, nullptr);
    }
    std::vector<double> flash(kLanes, -1e9);
    std::vector<Grade> flash_grade(kLanes, Grade::Miss);
    size_t log_seen = 0, auto_next = 0;
    std::vector<std::pair<double, int>> auto_releases;  // autoplay: (time, lane)
    double end_at = -1;
    bool closed = false;
    std::vector<double> frame_cpu_ms;
    long frame = 0;
    for (bool running = true; running && (opt.max_frames < 0 || frame < opt.max_frames); ++frame) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)) {
                running = false;
                closed = closed || e.type == SDL_EVENT_QUIT;
            } else if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !e.key.repeat && !opt.autoplay) {
                const int lane = LaneOf(e.key.scancode);
                if (lane < 0) continue;
                const double t = audio.PlayheadMs(e.key.timestamp) - offset;  // input timestamp on the master clock
                judge.Update(t);
                e.type == SDL_EVENT_KEY_DOWN ? judge.Press(lane, t) : judge.Release(lane, t);
            }
        }
        const double now = audio.PlayheadMs(SDL_GetTicksNS()) - offset;
        if (opt.autoplay) {
            for (; auto_next < chart.notes.size() && chart.notes[auto_next].time_ms <= now; ++auto_next) {
                const live::Note& n = chart.notes[auto_next];
                judge.Press(n.lane, n.time_ms);
                auto_releases.push_back({n.IsHold() ? n.end_ms : n.time_ms + 30, n.lane});
            }
            for (auto it = auto_releases.begin(); it != auto_releases.end();) {
                if (it->first <= now) {
                    judge.Release(it->second, it->first);
                    it = auto_releases.erase(it);
                } else {
                    ++it;
                }
            }
        }
        judge.Update(now);
        for (; log_seen < judge.Log().size(); ++log_seen) {
            const live::Judgement& j = judge.Log()[log_seen];
            const int lane = chart.notes[size_t(j.note)].lane;
            flash[size_t(lane)] = now;
            flash_grade[size_t(lane)] = j.grade;
        }

        if (has_hud && long(judge.score) != hud_score) {
            // uip0004_IngameScore: 7 digit meshes, M_IngameScore_00 the ones; each shows one cell of the 8 x 2 digit
            // grid (ui_ingame_top_score00: 0-7 over 8-9) by its UV offset
            hud_score = long(judge.score);
            long v = std::clamp(hud_score, 0L, 9999999L);
            for (int d = 0; d < 7; ++d, v /= 10) {
                const int g = int(v % 10);
                hud.SetMaterialUv("uil0001_Ingame00/uip0004_IngameScore", "M_IngameScore_0" + std::to_string(d),
                                  0.125f * float(g % 8), 0.5f * float(g / 8));
            }
            // uip0002_Ingame00Topscgage: ingame_score_gage00 has the bar in 5 colour rows of 1/8 height; the bar spans
            // U 0.039-0.961 and its C/B/A/S dividers sit at clear rates 0.45/0.65/0.825/0.925 of a full bar = 1.2 (the
            // top threshold). shortcut: row per rank (blue below C, then green, yellow, orange, pink) is a guess
            const double full = song.rules.rank_rates.back();
            const float fill = 0.039f + 0.922f * float(std::clamp(judge.ClearRate() / full, 0.0, 1.0));
            const std::string gauge = "uil0001_Ingame00/uip0002_Ingame00Topscgage";
            hud.SetMaterialClip(gauge, "M_Ingame00Topscgage", fill);
            hud.SetMaterialUv(gauge, "M_Ingame00Topscgage", 0, 0.125f * float(std::min(judge.Rank(), 4)));
        }
        if (field.loaded) field.SetGauge(renderer, float(now / audio_ms));
        if (renderer.BeginFrame()) {
            const uint64_t draw_start = SDL_GetTicksNS();
            scene.Draw(renderer, now, song.dance_start_frame);
            if (has_hud) hud.DrawLayout(renderer, "uil0001_Ingame00", 1.0f);
            if (field.loaded) {
                const auto grade_tint = [&](int l) {
                    const float* c = kGradeColor[int(flash_grade[size_t(l)])];
                    return std::array<float, 4>{c[0], c[1], c[2], float(1.0 - (now - flash[size_t(l)]) / 150.0)};
                };
                field.Draw(renderer, field.gauge_fan, field.center_gauge, field.center[0], field.center[1], Playfield::kGaugeSize, {1, 1, 1, 1});
                field.Draw(renderer, unit_quad, field.center_mic, field.center[0], field.center[1], field.center_size * 0.55f, {1, 1, 1, 1});
                for (int l = 0; l < kLanes; ++l) {
                    const auto& p = field.lane[size_t(l)];
                    field.Draw(renderer, field.ring, field.atlas, p[0], p[1], field.ring_size * 1.68f, {1, 1, 1, 1});
                    if (now - flash[size_t(l)] < 150) {
                        field.Draw(renderer, field.trail, field.atlas, p[0], p[1], field.ring_size * 2.0f, grade_tint(l));
                    }
                }
                // notes fly from the centre circle to their ring, growing (far notes first, nearer ones on top)
                const auto at = [&](int lane, double t, float& x, float& y, float& size) {
                    const float k = float(std::clamp(1.0 - (t - now) / opt.approach_ms, 0.0, 1.0));
                    const auto& p = field.lane[size_t(lane)];
                    x = field.center[0] + (p[0] - field.center[0]) * k;
                    y = field.center[1] + (p[1] - field.center[1]) * k;
                    size = field.ring_size * 1.68f * (0.2f + 0.8f * k);  // the atlas cell has a margin round the art
                };
                for (size_t i = chart.notes.size(); i-- > 0;) {
                    const live::Note& n = chart.notes[i];
                    if (!judge.Active(i) || n.time_ms - now > opt.approach_ms) continue;
                    float x, y, size;
                    if (n.IsHold()) {
                        const double head = judge.Held(i) ? now : n.time_ms;
                        for (double t = n.end_ms; t > head; t -= 40) {  // shortcut: trail of discs, not the long-note bar
                            if (t - now > opt.approach_ms) continue;
                            at(n.lane, t, x, y, size);
                            field.Draw(renderer, field.trail, field.atlas, x, y, size * 0.7f, {1, 1, 1, 0.45f});
                        }
                        at(n.lane, n.end_ms, x, y, size);
                        field.Draw(renderer, field.note, field.atlas, x, y, size * 0.8f, {1, 1, 1, 0.8f});
                    }
                    at(n.lane, judge.Held(i) ? now : n.time_ms, x, y, size);
                    field.Draw(renderer, field.note, field.atlas, x, y, size, {1, 1, 1, 1});
                }
                field.DrawCombo(renderer, judge.combo, 960, 470);  // below the centre circle (Hypothesis)
            } else {  // placeholder lanes when the game's play field cannot be loaded
            layout.Fit(renderer.Width(), renderer.Height());
            const float ring[4] = {0.85f, 0.85f, 0.95f, 0.9f};
            for (int l = 0; l < kLanes; ++l) {
                float x, y;
                layout.Lane(l, x, y);
                renderer.DrawCircle(x, y, layout.button, 0.82f, ring);
                if (now - flash[size_t(l)] < 150) {
                    float c[4];
                    std::copy(kGradeColor[int(flash_grade[size_t(l)])], kGradeColor[int(flash_grade[size_t(l)])] + 4, c);
                    c[3] = float(1.0 - (now - flash[size_t(l)]) / 150.0);
                    renderer.DrawCircle(x, y, layout.button * 1.25f, 0.6f, c);
                }
            }
            const auto point = [&](int lane, double t, float& x, float& y, float& r) {
                const float p = float(std::clamp(1.0 - (t - now) / opt.approach_ms, 0.0, 1.0));
                float lx, ly;
                layout.Lane(lane, lx, ly);
                x = layout.cx + (lx - layout.cx) * p;
                y = layout.cy + (ly - layout.cy) * p;
                r = layout.button * (0.2f + 0.75f * p);
            };
            // far notes first, so nearer ones are drawn on top
            for (size_t i = chart.notes.size(); i-- > 0;) {
                const live::Note& n = chart.notes[i];
                if (!judge.Active(i) || n.time_ms - now > opt.approach_ms) continue;
                float x, y, r;
                if (n.IsHold()) {
                    const float trail[4] = {0.95f, 0.95f, 1.0f, 0.35f};
                    const double head = judge.Held(i) ? now : n.time_ms;
                    for (double t = n.end_ms; t > head; t -= 40) {  // trail: discs from the tail to the head
                        if (t - now > opt.approach_ms) continue;
                        point(n.lane, t, x, y, r);
                        renderer.DrawCircle(x, y, r * 0.7f, 0, trail);
                    }
                    point(n.lane, n.end_ms, x, y, r);
                    renderer.DrawCircle(x, y, r * 0.8f, 0.5f, ring);
                }
                point(n.lane, judge.Held(i) ? now : n.time_ms, x, y, r);
                const float body[4] = {1.0f, 0.55f, 0.75f, 1.0f};
                renderer.DrawCircle(x, y, r, 0, body);
            }
            }
            renderer.EndFrame();
            const double cpu_ms = double(SDL_GetTicksNS() - draw_start) / 1e6;  // CPU cost of one frame, without the vsync wait
            frame_cpu_ms.push_back(cpu_ms);
        } else {
            SDL_Delay(5);
        }

        if (frame % 15 == 0) {
            char title[256];
            std::snprintf(title, sizeof(title), "SIF port - %s [%02d]  combo %d  P %d / Gr %d / Go %d / B %d / M %d  score %.0f",
                          song.title.c_str(), opt.variant, judge.combo, judge.counts[0], judge.counts[1], judge.counts[2],
                          judge.counts[3], judge.counts[4], judge.score);
            SDL_SetWindowTitle(window, title);
        }
        if (judge.Finished() && end_at < 0) end_at = now;
        if (end_at >= 0 && (!audio.Playing() || now - end_at > 3000)) running = false;
    }

    scene.Release(renderer);
    field.Release(renderer);
    hud.Release(renderer);
    renderer.Release(unit_quad);
    if (!frame_cpu_ms.empty()) {
        std::sort(frame_cpu_ms.begin(), frame_cpu_ms.end());
        double total = 0;
        for (double t : frame_cpu_ms) total += t;
        LogInfo("live frame cost (CPU, {} members): mean {:.2f} ms, median {:.2f} ms, 99th percentile {:.2f} ms over {} frames",
                scene.dancers.size(), total / double(frame_cpu_ms.size()), frame_cpu_ms[frame_cpu_ms.size() / 2],
                frame_cpu_ms[frame_cpu_ms.size() * 99 / 100], frame_cpu_ms.size());
    }
    double sum = 0, sum2 = 0;
    int n = 0;
    for (const live::Judgement& j : judge.Log()) {
        if (j.release || j.grade == Grade::Miss) continue;
        sum += j.offset_ms;
        sum2 += j.offset_ms * j.offset_ms;
        ++n;
    }
    const double mean = n ? sum / n : 0, sd = n ? std::sqrt(std::max(0.0, sum2 / n - mean * mean)) : 0;
    LogInfo("live result: Perfect {} Great {} Good {} Bad {} Miss {}, max combo {}, provisional score {:.0f}; "
            "press offset mean {:+.1f} ms, sd {:.1f} ms over {} hits",
            judge.counts[0], judge.counts[1], judge.counts[2], judge.counts[3], judge.counts[4], judge.max_combo,
            judge.score, mean, sd, n);
    if (result) {
        *result = {opt.song, opt.variant, {}, judge.max_combo, judge.score, judge.Finished(), closed};
        std::copy(judge.counts.begin(), judge.counts.end(), result->counts.begin());
    }
    return 0;
}

bool RunCalibration(SDL_Window* window, Renderer& renderer, AudioOutput& audio, AppSettings& settings, long max_frames) {
    // click track: 2 s silence, then 24 clicks at 120 BPM (a 15 ms 1 kHz tone burst each)
    constexpr int kRate = 48000, kClicks = 24;
    constexpr double kFirst = 2000, kBeat = 500;
    cri::Pcm pcm{kRate, 2, std::vector<int16_t>(size_t(kRate * (kFirst + kBeat * (kClicks + 2)) / 1000) * 2)};
    for (int c = 0; c < kClicks; ++c) {
        const size_t start = size_t((kFirst + kBeat * c) * kRate / 1000);
        for (size_t i = 0; i < size_t(kRate * 15 / 1000); ++i) {
            const double env = 1.0 - double(i) / (kRate * 0.015);
            const auto s = int16_t(12000 * env * std::sin(2 * 3.14159265358979 * 1000 * double(i) / kRate));
            pcm.samples[(start + i) * 2] = pcm.samples[(start + i) * 2 + 1] = s;
        }
    }
    SDL_SetWindowTitle(window, "SIF port - calibration: press Space on every click");
    LogInfo("calibration: {} clicks at {:.0f} ms intervals", kClicks, kBeat);
    audio.Play(std::move(pcm));
    std::vector<double> taps;
    double last_tap = -1e9;
    long frame = 0;
    for (bool running = true; running && (max_frames < 0 || frame < max_frames); ++frame) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)) {
                running = false;
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_SPACE) {
                const double t = audio.PlayheadMs(e.key.timestamp);  // no offset: this measures it
                const double nearest = kFirst + kBeat * std::round((t - kFirst) / kBeat);
                if (nearest >= kFirst && std::abs(t - nearest) < kBeat / 2) taps.push_back(t - nearest);
                last_tap = t;
            }
        }
        const double now = audio.PlayheadMs(SDL_GetTicksNS());
        if (renderer.BeginFrame()) {
            const float cx = float(renderer.Width()) / 2, cy = float(renderer.Height()) / 2;
            const double phase = std::fmod(std::max(0.0, now - kFirst), kBeat) / kBeat;  // visual beat
            const float beat[4] = {1, 0.85f, 0.2f, float(now >= kFirst ? 1.0 - phase : 0.0)};
            const float tap[4] = {0.4f, 0.8f, 1.0f, float(std::max(0.0, 1.0 - (now - last_tap) / 200))};
            renderer.DrawCircle(cx, cy, 80, 0.8f, beat);
            renderer.DrawCircle(cx, cy, 50, 0, tap);
            renderer.EndFrame();
        }
        if (!audio.Playing()) running = false;
    }
    if (taps.size() < 8) {
        LogWarn("calibration: only {} taps, nothing stored", taps.size());
        return false;
    }
    std::sort(taps.begin(), taps.end());
    const double median = taps[taps.size() / 2];
    settings.timing.audio_offset_ms = int(std::lround(median));
    LogInfo("calibration: {} taps, median offset {:+.1f} ms (range {:+.1f} .. {:+.1f}); stored as audio_offset_ms",
            taps.size(), median, taps.front(), taps.back());
    return true;
}

}
