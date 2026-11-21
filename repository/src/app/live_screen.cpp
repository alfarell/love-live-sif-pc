#include "app/live_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "app/fade.h"
#include "app/input.h"
#include "app/result_screen.h"
#include "app/route.h"
#include "app/skill_screen.h"
#include "engine/audio/audio_output.h"
#include "engine/audio/sound_bank.h"
#include "engine/core/log.h"
#include "engine/platform/settings.h"
#include "engine/formats/bscm.h"
#include "engine/formats/blbn.h"
#include "engine/formats/fad.h"
#include "engine/fs/vfs.h"
#include "engine/ui/ui_screen.h"
#include "engine/render/renderer.h"
#include "engine/render/scene_model.h"
#include "engine/video/movie.h"
#include "game/live/song.h"

namespace sif {
namespace {

using live::Grade;
using live::kGrades;
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
// 32-byte records: u32, u32, u64 name*, f32 rect x0 y0 x1 y1 in 1920 x 1080 pixels), sprites from two atlases of
// 256-pixel cells: texture.pac/ui_ingame_notes (5 x 6: note ring, fill, finale discs, brackets ...) and
// texture.pac/ui_ingame_judgering_all_mark (4 x 5: the indicator ring and the button icons). Cells and sizes were
// matched to shadPS4 frames (analysis/202611072212-gameplay-feedback/): the indicator cell draws 1:1 (its orange ring
// is 177 px in the cell and on screen), a note arrives at 1:1 too, icons share their base's scale
struct Playfield {
    static constexpr float kCell = 256, kAtlasW = 1280, kAtlasH = 1536, kMarkW = 1024, kMarkH = 1280;
    // a note's quad when it reaches its indicator: 1:1 like the indicator, so the note ring ((1, 0): 163 px, rows
    // 47-209 of its cell) is exactly the indicator's grey body inside the orange outline (mark (0, 0): rows 47-209)
    static constexpr float kNoteSize = kCell;
    static constexpr float kIconGap = 30;               // two icons in one lane (→ □): centres ±30 px at 1:1
    Texture atlas, mark, center_gauge, center_mic, combo_tex;
    GpuMesh indicator, note_ring, note_fill, combo_label, gauge_fan;
    // the home charts' L1/R1 lanes are blue (shadPS4, analysis/202611141918-home-charts/): the blue indicator (mark
    // (1, 0)), the blue note ring (3, 0) and the dark blue fill (3, 4)
    GpuMesh indicator_blue, note_ring_blue, note_fill_blue;
    bool home = false;  // a home-version chart: six lanes (live::kHomeLanes)
    std::array<GpuMesh, 2> bracket;  // simultaneous-note brackets: orange (3, 3), blue (4, 3)
    GpuMesh finale_disc;  // the finale note's yellow disc: star (0, 1), anchor (1, 1) or snowman (2, 1) by group
    GpuMesh icon_mark;  // the Finale Rhythm Icon on a note: two stars (3, 2), anchors (4, 2) or snowmen (0, 3) by group
    // button icons per PadButton: rows 1-3 of the mark atlas, and the smaller shoulder icons of row 4 (used on notes;
    // Hypothesis: row 3 on indicators, row 4 on notes, from their sizes on screen)
    std::array<GpuMesh, size_t(PadButton::Count)> icon, note_icon;
    std::array<std::vector<PadButton>, kLanes> lane_buttons;  // the pad buttons of each lane (Button Controls)
    Texture hold_glow;  // ui_ingame_longnote_circle_01: a glowing ring (240 px; the ring is about 0.71 of it)
    float gauge_progress = 0;
    std::array<GpuMesh, 10> digits;
    std::array<fad::Animation, kGrades> judge_fad;  // anim.pac ui_ingame_judge_text_perfect .. _miss
    fad::Animation live_clear;                       // anim.pac ui_ingame_end_text_liveclear
    Texture completion;                              // ui_ingame_end_completion: FULL COMBO top half, ALL PERFECT bottom
    GpuMesh completion_half[2];                      // the `corner` quad over the top / bottom half of it
    fad::Animation ring_break;                       // anim.pac ui_ingame_judgering_break
    std::array<float, 2> banner{962, 500};           // layout_FULLCOMBO centre (1052 x 147, like LIVE CLEAR's text)
    std::map<std::string, Texture> fad_textures;     // by the names the .fad files list
    GpuMesh corner;                                  // unit quad (0,0)-(1,-1) over the whole texture
    std::array<std::array<float, 2>, kLanes> lane{};  // canvas x, y (y down)
    std::array<float, 2> center{958, 146};
    float ring_size = 152, center_size = 288;
    bool loaded = false;

    static bool LoadTexture(Renderer& r, const Vfs& vfs, const std::string& path, Texture& out) {
        std::vector<uint8_t> file;
        btx::Image image;
        return vfs.Read(path, file) && btx::Parse(file, image) && r.Upload(image, out, nullptr);
    }
    // The `corner` quad (0,0)-(1,-1) over the top (half 0) or bottom (half 1) half of a texture
    static bool CornerHalf(Renderer& r, int half, GpuMesh& out) {
        const float v0 = 0.5f * float(half), v1 = v0 + 0.5f;
        const MeshVertex quad[4] = {{{0, 0, 0}, {0, 0, 1}, {0, v0}}, {{1, 0, 0}, {0, 0, 1}, {1, v0}},
                                    {{1, -1, 0}, {0, 0, 1}, {1, v1}}, {{0, -1, 0}, {0, 0, 1}, {0, v1}}};
        const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        return r.CreateMesh(quad, indices, false, out, nullptr);
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
    static bool MarkCell(Renderer& r, int col, int row, GpuMesh& out) {
        return Quad(r, col * kCell / kMarkW, row * kCell / kMarkH, (col + 1) * kCell / kMarkW,
                    (row + 1) * kCell / kMarkH, out);
    }
    // The mark atlas cell of a button: rows 1-2 in PadButton order (↑ ← ↓ →, △ □ × ○), row 3 L1 L2 R2 R1; `small`
    // takes row 4 (the same shoulder icons, smaller) for L1-R2
    static std::array<int, 2> IconCell(PadButton b, bool small) {
        const int i = int(b);
        if (i < 8) return {i % 4, 1 + i / 4};
        static constexpr int kShoulderCol[4] = {0, 1, 3, 2};  // L1 L2 R1 R2
        return {kShoulderCol[i - 8], small ? 4 : 3};
    }
    bool LoadIcons(Renderer& r) {
        for (int b = 0; b < int(PadButton::Count); ++b) {
            const auto big = IconCell(PadButton(b), false), small = IconCell(PadButton(b), true);
            if (!MarkCell(r, big[0], big[1], icon[size_t(b)]) || !MarkCell(r, small[0], small[1], note_icon[size_t(b)])) {
                return false;
            }
        }
        const PadSlots& slots = CurrentPadSlots();
        for (int s = 0; s < kPadSlots; ++s) lane_buttons[size_t(kLaneOfSlot[size_t(s)])].push_back(slots[size_t(s)]);
        return true;
    }
    // The lane's button icons centred at (x, y), `scale` of their 1:1 size
    void DrawIcons(Renderer& r, int lane, float x, float y, float scale, bool on_note) const {
        const auto& buttons = lane_buttons[size_t(lane)];
        const float step = kIconGap * scale * 2, left = x - step * float(buttons.size() - 1) / 2;
        for (size_t i = 0; i < buttons.size(); ++i) {
            const GpuMesh& m = (on_note ? note_icon : icon)[size_t(buttons[i])];
            Draw(r, m, mark, left + step * float(i), y, kCell * scale, {1, 1, 1, 1});
        }
    }
    // Measured in shadPS4 frames (analysis 202611082335): a note leaves the centre circle at about 0.8 of its size at
    // the ring and grows linearly to full size; its brackets span 1.26x its ring (the user's PS4 still, analysis
    // 202611082356; the cell's own ratio is 1.24)
    static constexpr float kNoteStart = 0.8f;
    static constexpr float kBracketScale = 1.02f;
    // How far a note due at `t` is along its path at `now`: 0 at the centre circle, 1 at its ring
    static float Progress(double t, double now, double approach_ms) {
        return float(std::clamp(1.0 - (t - now) / approach_ms, 0.0, 1.0));
    }
    // Where a note due at `t` is at `now`: on the line from the centre circle to its ring, growing as it comes
    void Path(int lane, double t, double now, double approach_ms, float& x, float& y, float& size) const {
        const float k = Progress(t, now, approach_ms);
        const auto& p = this->lane[size_t(lane)];
        x = center[0] + (p[0] - center[0]) * k;
        y = center[1] + (p[1] - center[1]) * k;
        size = kNoteSize * (kNoteStart + (1 - kNoteStart) * k);
    }
    // A hold tail's size: it narrows toward the centre circle much more than a note does (the band is about 35 px wide
    // at the mic in 1280 px frames), so it keeps the steeper G3 curve
    static float TailSize(double t, double now, double approach_ms) {
        return kNoteSize * (0.2f + 0.8f * Progress(t, now, approach_ms));
    }
    // Hold bands (shadPS4 frames, analysis/202611072212-gameplay-feedback/): common_tex connect_note, a flat grey strip,
    // stretched from the note head back to the tail as a band that tapers with the path (as wide as the end disc at
    // the tail, 0.8 of the note ring's pink diameter at each end), drawn at 62% opacity, tinted pink while held; a
    // solid end cap at the tail as wide as the band: ui_ingame_notes (3, 1) white, (4, 1) pink while held (analysis
    // 202611082306). Band meshes are rebuilt each frame before it starts (UpdateMesh needs that)
    static constexpr float kBandWidth = 0.8f * 150.0f / 256.0f;  // of a note quad
    static constexpr float kBandAlpha = 0.62f;
    static constexpr std::array<float, 4> kHeldTint{1.0f, 0.78f, 0.79f, 1.0f};
    static constexpr float kCapDisc = 152.0f / 256.0f;  // the cap's opaque disc within its cell
    static constexpr float kNoteRing = 163.0f / 256.0f;  // the note ring's outer edge within its cell (1, 0)
    static constexpr size_t kMaxBands = 32;
    Texture connect, connect_finale;  // connect_finale: common_tex connect_note_finale, for finale holds
    std::array<GpuMesh, 2> end_cap;   // white, pink (held)
    std::array<GpuMesh, kMaxBands> band;
    struct Tail {
        float x, y, size;
        bool held, finale;
        int group;  // Note::end_group: brackets around the cap when the tail ends with others
    };
    std::array<Tail, kMaxBands> tails{};
    size_t bands = 0;
    bool LoadBands(Renderer& r, const Vfs& vfs) {
        if (!LoadTexture(r, vfs, "common/common.pac/common_tex.pac/connect_note", connect) ||
            !LoadTexture(r, vfs, "common/common.pac/common_tex.pac/connect_note_finale", connect_finale) ||
            !Cell(r, 3, 1, end_cap[0]) || !Cell(r, 4, 1, end_cap[1])) {
            return false;
        }
        const MeshVertex v[4]{};
        const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        for (GpuMesh& m : band) {
            if (!r.CreateMesh(v, indices, true, m, nullptr)) return false;
        }
        return true;
    }
    // Call outside a frame: one band per hold on screen, from its head (held: pinned on the ring) to its tail
    void SetBands(Renderer& r, const live::Chart& chart, const live::Judge& judge, double now, double approach_ms) {
        bands = 0;
        for (size_t i = 0; i < chart.notes.size() && bands < kMaxBands; ++i) {
            const live::Note& n = chart.notes[i];
            if (!n.IsHold() || !judge.Active(i) || n.time_ms - now > approach_ms) continue;
            const bool held = judge.Held(i);
            float hx, hy, hs, tx, ty, ts;
            Path(n.lane, held ? now : n.time_ms, now, approach_ms, hx, hy, hs);
            Path(n.lane, n.end_ms, now, approach_ms, tx, ty, ts);
            ts = TailSize(n.end_ms, now, approach_ms);
            const float dx = hx - tx, dy = hy - ty, len = std::hypot(dx, dy);
            const float nx = len > 0 ? -dy / len : 0, ny = len > 0 ? dx / len : 1;  // across the band
            const float wh = hs * kBandWidth / 2, wt = ts * kBandWidth / 2;
            // canvas pixels with y up (drawn through CanvasToClip); the strip's opaque half (v 0-0.45) is uniform
            const MeshVertex v[4] = {{{tx + nx * wt, -(ty + ny * wt), 0}, {0, 0, 1}, {0, 0}},
                                     {{tx - nx * wt, -(ty - ny * wt), 0}, {0, 0, 1}, {1, 0}},
                                     {{hx - nx * wh, -(hy - ny * wh), 0}, {0, 0, 1}, {1, 0.45f}},
                                     {{hx + nx * wh, -(hy + ny * wh), 0}, {0, 0, 1}, {0, 0.45f}}};
            r.UpdateMesh(band[bands], v);
            tails[bands++] = {tx, ty, ts, held, judge.finale && n.time_ms >= chart.finale_ms, n.end_group};
        }
    }
    // The bands and their end discs, under the notes
    void DrawBands(Renderer& r) const {
        const Mat4 to_clip = UiScreen::CanvasToClip(r.Width(), r.Height());
        for (size_t b = 0; b < bands; ++b) {
            const std::array<float, 4> tint = tails[b].held ? kHeldTint : std::array<float, 4>{1, 1, 1, 1};
            r.DrawUiMesh(band[b], tails[b].finale ? &connect_finale : &connect, to_clip, {tint[0], tint[1], tint[2], kBandAlpha});
            const float cap = tails[b].size * kBandWidth;  // the cap's disc diameter
            Draw(r, end_cap[tails[b].held ? 1 : 0], atlas, tails[b].x, tails[b].y, cap / kCapDisc, {1, 1, 1, 1});
            // simultaneous tails: the brackets sit on the cap as on a note whose ring is the cap (image 4)
            if (tails[b].group >= 0) {
                Draw(r, bracket[size_t(tails[b].group % 2)], atlas, tails[b].x, tails[b].y,
                     cap / kNoteRing * kBracketScale, {1, 1, 1, 1});
            }
        }
    }
    GpuMesh corner_centered;  // centred unit quad over a whole texture
    // Home charts: only the six home lanes have rings; L1 and R1 are the blue ones
    bool Shown(int lane) const {
        return !home || std::find(live::kHomeLanes.begin(), live::kHomeLanes.end(), lane) != live::kHomeLanes.end();
    }
    bool Blue(int lane) const { return home && (lane == 1 || lane == 7); }
    // Home charts place their six rings on the arcade rings' arc (centre between ring01 and ring09, radius down to
    // ring05) at 180, 140, 112.5, 67.5, 40 and 0 degrees: L1 and R1 (lanes 1, 7) move out to the corners (ring01 and
    // ring09's spots), ← and ○ (lanes 2, 6) to 140 and 40 degrees; ↓ and ✕ keep ring04 and ring06. Measured in shadPS4
    // (0340 HYPER, ±1 degree). Hypothesis: no table in the dump holds the 140 and 40
    void SetHome(bool on, HomeConfig config) {
        home = on;
        if (!on) return;
        for (auto& b : lane_buttons) b.clear();  // the ring icons show the 6-lane Button Controls
        const HomeSlots& slots = CurrentHomeSlots(config);
        for (int s = 0; s < kHomeSlots; ++s) lane_buttons[size_t(kLaneOfHomeSlot[size_t(s)])].push_back(slots[size_t(s)]);
        const float cx = (lane[0][0] + lane[8][0]) / 2, cy = (lane[0][1] + lane[8][1]) / 2, radius = lane[4][1] - cy;
        for (const auto& [l, degrees] : {std::pair<int, float>{1, 180.0f}, {2, 140.0f}, {6, 40.0f}, {7, 0.0f}}) {
            const float a = degrees * 3.14159265f / 180.0f;
            lane[size_t(l)] = {cx + radius * std::cos(a), cy + radius * std::sin(a)};
        }
    }
    // A normal note: fill (its cell has 40% alpha built in), ring, then the lane's icons; `size` is its quad
    // `finale`: a note of the finale section, on the yellow group disc instead of the pink fill (shadPS4 run 1)
    void DrawNote(Renderer& r, int lane, float x, float y, float size, float alpha, bool finale = false) const {
        const bool blue = Blue(lane);
        Draw(r, finale ? finale_disc : blue ? note_fill_blue : note_fill, atlas, x, y, size, {1, 1, 1, alpha});
        Draw(r, blue ? note_ring_blue : note_ring, atlas, x, y, size, {1, 1, 1, alpha});
        DrawIcons(r, lane, x, y, size / kCell, true);
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
    // `group`: the song's group (0 mu's, 1 Aqours, 2 Saint Snow), kept for the finale and icon looks (M7.5 G5-G6)
    int group = 0;
    bool Load(Renderer& r, const Vfs& vfs, int song_group) {
        group = std::clamp(song_group, 0, 2);
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
            } else if (n == "layout_FULLCOMBO") {
                banner = {cx, cy};
            }
        }
        loaded = LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_notes", atlas) &&
                 LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_judgering_all_mark", mark) &&
                 MarkCell(r, 0, 0, indicator) && MarkCell(r, 1, 0, indicator_blue) && Cell(r, 3, 0, note_ring_blue) &&
                 Cell(r, 3, 4, note_fill_blue) && LoadIcons(r) && LoadBands(r, vfs) && Quad(r, 0, 0, 1, 1, corner_centered) &&
                 // the HUD song gauge's own colour ring (ui2/90_ingame part uip0006_Songgage, 188 px), so the fill
                 // lines up with the HUD's track
                 LoadTexture(r, vfs, "ui2/90_ingame.pac/uip0006_Songgage.pac/texture.pac/IngamesonggageCol.btx", center_gauge) &&
                 LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_centercircle_mic_aq", center_mic) &&
                 Cell(r, 1, 0, note_ring) && Cell(r, 2, 0, note_fill) && LoadGauge(r) &&
                 LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_longnote_circle_01", hold_glow) &&
                 Cell(r, 3, 3, bracket[0]) && Cell(r, 4, 3, bracket[1]) &&
                 Cell(r, group == 2 ? 0 : 3 + group, group == 2 ? 3 : 2, icon_mark) && Cell(r, group, 1, finale_disc);
        if (!loaded) LogWarn("live: game play field not loaded; drawing the placeholder lanes");
        if (loaded && !LoadCombo(r, vfs)) LogWarn("live: combo digits not loaded");
        if (loaded && !LoadJudgeTexts(r, vfs)) LogWarn("live: judgement texts not loaded");
        if (loaded && !LoadFad(r, vfs, "ui_ingame_end_text_liveclear", live_clear)) LogWarn("live: LIVE CLEAR not loaded");
        if (loaded && !(LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/ui_ingame_end_completion", completion) &&
                        CornerHalf(r, 0, completion_half[0]) && CornerHalf(r, 1, completion_half[1]))) {
            LogWarn("live: FULL COMBO / ALL PERFECT text not loaded");
        }
        if (loaded && !LoadFad(r, vfs, "ui_ingame_judgering_break", ring_break)) LogWarn("live: ring break not loaded");
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
    // anim.pac/<name>.fad and every texture it names
    bool LoadFad(Renderer& r, const Vfs& vfs, const std::string& name, fad::Animation& a) {
        std::vector<uint8_t> file;
        std::string error;
        if (!vfs.Read("ui/ui_ingame.pac/anim.pac/" + name + ".fad", file, &error) || !fad::Parse(file, a, &error)) {
            LogWarn("live: {}: {}", name, error);
            return false;
        }
        for (const auto& [id, sprite] : a.sprites) {
            if (fad_textures.contains(sprite.texture)) continue;
            if (!LoadTexture(r, vfs, "ui/ui_ingame.pac/texture.pac/" + sprite.texture, fad_textures[sprite.texture])) return false;
        }
        return true;
    }
    // The judgement animations (ui_ingame_judge_text_*.fad)
    bool LoadJudgeTexts(Renderer& r, const Vfs& vfs) {
        static constexpr const char* kNames[kGrades] = {"perfect", "great", "good", "bad", "miss"};
        for (int g = 0; g < kGrades; ++g) {
            if (!LoadFad(r, vfs, std::string("ui_ingame_judge_text_") + kNames[g], judge_fad[size_t(g)])) return false;
        }
        const MeshVertex quad[4] = {{{0, 0, 0}, {0, 0, 1}, {0, 0}}, {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                                    {{1, -1, 0}, {0, 0, 1}, {1, 1}}, {{0, -1, 0}, {0, 0, 1}, {0, 1}}};
        const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        return r.CreateMesh(quad, indices, false, corner, nullptr);
    }
    // One frame of a .fad animation, `ms` after it started, with its canvas centre at canvas pixel (x, y) (shortcut:
    // the anchor is a guess)
    // `text_half` 0/1: the animation's text sprite (texture `ui_ingame_end_text_liveclear`) is drawn as that half of
    // `completion` instead, at the sprite's width and the half's own aspect, centred on the sprite (Hypothesis: the
    // game swaps the texture the same way; its FULL COMBO / ALL PERFECT frames move and shine like LIVE CLEAR)
    void DrawFad(Renderer& r, const fad::Animation& a, double ms, float x, float y, int text_half = -1) const {
        const size_t frame = size_t(std::max(0.0, ms) * a.fps / 1000.0);
        if (ms < 0 || frame >= a.frames.size()) return;
        const Mat4 to_clip = UiScreen::CanvasToClip(r.Width(), r.Height());
        const float ox = x - a.width / 2, oy = y - a.height / 2;
        for (const auto& [depth, placed] : a.frames[frame]) {
            const auto s = a.sprites.find(placed.sprite);
            if (s == a.sprites.end()) continue;
            const auto t = fad_textures.find(s->second.texture);
            if (t == fad_textures.end()) continue;
            // the textured rectangle (0,0)-(w,h), y down, through the sprite's and the timeline's matrices; the
            // corner quad's vertices are (u, -v)
            const fad::Matrix m = placed.matrix * s->second.matrix;
            const bool swap = text_half >= 0 && completion.width && s->second.texture == "ui_ingame_end_text_liveclear";
            const float w = s->second.width;
            const float h = swap ? w * float(completion.height) / 2 / float(completion.width) : s->second.height;
            const float dy = (s->second.height - h) / 2;  // the swapped text centred on the sprite (sprite-local y)
            Mat4 place;
            place(0, 0) = m.a * w;
            place(0, 1) = -m.c * h;
            place(0, 3) = ox + m.tx + m.c * dy;
            place(1, 0) = -m.b * w;
            place(1, 1) = m.d * h;
            place(1, 3) = -(oy + m.ty + m.d * dy);
            const auto& c = placed.colour;
            r.DrawUiMesh(swap ? completion_half[size_t(text_half)] : corner, swap ? &completion : &t->second, to_clip * place, {c.mul[0], c.mul[1], c.mul[2], c.mul[3]}, {0, 0}, 1e9f, c.add);
        }
    }
    // The combo as the game lays it out (shadPS4 frames, analysis/202611072212-gameplay-feedback/): digits at 1:1, 80 px
    // apart, centred on `y`; the "COMBO" label to their right, 13 px after them, 13 px lower; the whole line centred on
    // `x`; shown from the first note. `alpha`: the user's 40% (the game looks about 50%)
    static constexpr float kDigitAdvance = 80, kLabelGap = 13, kLabelDrop = 13;
    void DrawCombo(Renderer& r, int combo, float x, float y, float alpha) const {
        if (!combo_tex.width || combo < 1) return;
        const std::string text = std::to_string(combo);
        const float digits_w = kDigitAdvance * float(text.size());
        float left = x - (digits_w + kLabelGap + kLabelW) / 2;
        for (size_t i = 0; i < text.size(); ++i) {
            Draw(r, digits[size_t(text[i] - '0')], combo_tex, left + kDigitAdvance * (float(i) + 0.5f), y, kDigitW, kDigitH,
                 {1, 1, 1, alpha});
        }
        Draw(r, combo_label, combo_tex, left + digits_w + kLabelGap + kLabelW / 2, y + kLabelDrop, kLabelW,
             kLabelY1 - kLabelY0, {1, 1, 1, alpha});
    }
    void Release(Renderer& r) {
        for (Texture* t : {&atlas, &mark, &center_gauge, &center_mic, &combo_tex, &connect, &connect_finale}) r.Release(*t);
        for (GpuMesh& m : band) r.Release(m);
        r.Release(corner_centered);
        for (auto& [name, t] : fad_textures) r.Release(t);
        r.Release(corner);
        for (GpuMesh* m : {&indicator_blue, &note_ring_blue, &note_fill_blue}) r.Release(*m);
        r.Release(completion);
        for (GpuMesh& m : completion_half) r.Release(m);
        for (GpuMesh* m : {&indicator, &note_ring, &note_fill, &combo_label, &gauge_fan, &bracket[0], &bracket[1], &icon_mark, &finale_disc, &end_cap[0], &end_cap[1]}) r.Release(*m);
        for (GpuMesh& m : icon) r.Release(m);
        for (GpuMesh& m : note_icon) r.Release(m);
        r.Release(hold_glow);
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

// View Live Show's own cameras (M12 P4b; shadPS4 work/m12/shad2/v05-v11). Member: an orbit around one member's head
// (Bip001_Head), close as in the game's Member Camera; View: an orbit around the stage centre from the front. Yaw 0 is
// in front (the members face -Y in the Z-up world). shortcut: the game's distances, limits and speeds are not traced;
// these are chosen to look like its shots
struct CameraRig {
    int mode = 0;    // 0 MV (the song's camera), 1 Member, 2 View
    int member = 0;  // Member: index into LiveScene::dancers
    struct Orbit {
        float yaw = 0, pitch = 0, dist = 1, height = 0;  // radians, metres; height moves the look-at point up
    };
    Orbit member_orbit{0, 0.05f, 1.1f, 0};
    Orbit view_orbit{0, 0.12f, 9.0f, 0};
    // held keys: arrows orbit; L1 (Page Up) held moves the camera away (with Up: nearer), L2 (Home) raises it (with
    // Down: lowers). In the game holding L1 alone zoomed out (shad2/v08); the rest of its key rule is not traced
    static void Move(Orbit& o, bool member_cam, float dt, bool left, bool right, bool up, bool down, bool l1, bool l2) {
        const float v = float(up) - float(down), h = float(right) - float(left);
        if (l1) {
            o.dist = std::clamp(o.dist + (v != 0 ? -v : 1.0f) * (member_cam ? 1.2f : 5.0f) * dt, member_cam ? 0.5f : 3.0f,
                                member_cam ? 4.0f : 18.0f);
        } else if (l2) {
            o.height = std::clamp(o.height + (v != 0 ? v : 1.0f) * (member_cam ? 0.6f : 1.5f) * dt, -1.0f, member_cam ? 0.4f : 3.0f);
        } else {
            o.yaw += h * 1.6f * dt;
            o.pitch = std::clamp(o.pitch + v * 1.0f * dt, -0.5f, 1.2f);
        }
    }
    // `base`: added to the yaw (the followed member's facing, so yaw 0 is in front of her)
    static Mat4 Look(const Orbit& o, Vec3 target, float fov, float aspect, float base = 0) {
        target.z += o.height;
        const float c = std::cos(o.pitch), yaw = o.yaw + base;
        const Vec3 eye{target.x + std::sin(yaw) * c * o.dist, target.y - std::cos(yaw) * c * o.dist,
                       target.z + std::sin(o.pitch) * o.dist};
        return Perspective(fov, aspect, 0.05f, 500.0f) * LookAt(eye, target, {0, 0, 1});
    }
};

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
    // The stage screens' movie (M8 V3): the song's movie/monitor_NNN(_b).usm streamed into a texture that replaces the
    // screens' placeholder textures (the logo shown otherwise); the dot-mask overlay meshes stay as they are
    video::Movie screen_movie;
    Texture screen_texture;
    std::map<std::string, const Texture*> screens;  // placeholder texture name -> screen_texture
    static constexpr const char* kScreenPlaceholders[] = {"monitor_scrnrogo.tga", "monitor.tga", "s01_monitor_off.tga",
                                                          "cyber_monitor_dummy.tga"};

    void LoadScreens(Renderer& renderer, const Vfs& vfs, const LiveOptions& opt, int monitor) {
        int model = 0;
        if (!has_stage || std::sscanf(opt.stage.c_str(), "st%d", &model) != 1) return;
        const std::string path = live::MonitorMoviePath(vfs, monitor, model);
        if (path.empty()) return;
        std::string error;
        if (!screen_movie.Open(vfs, path, &error) ||
            !renderer.CreateTexture(uint32_t(screen_movie.Info().width), uint32_t(screen_movie.Info().height),
                                    screen_texture, &error)) {
            LogWarn("live: stage screens: {}: {}", path, error);  // the screens keep their logo
            screen_movie.Close();
            return;
        }
        for (const char* name : kScreenPlaceholders) screens[name] = &screen_texture;
        const auto& i = screen_movie.Info();
        LogInfo("live: stage screens play {} ({}x{}, {:.0f} fps, {} frames)", path, i.width, i.height, i.fps, i.frames);
    }
    // Call outside a frame: the screen picture for audio time `now_ms`, looping on the music clock (no drift)
    void UpdateScreens(Renderer& renderer, double now_ms) {
        if (!screen_movie.IsOpen()) return;
        const auto& i = screen_movie.Info();
        const int n = int(std::max(0.0, now_ms) / 1000.0 * i.fps) % i.frames;
        if (n == screen_movie.Current()) return;
        const uint64_t t0 = SDL_GetTicksNS();
        if (const auto* rgba = screen_movie.Frame(n)) renderer.UpdateTexture(screen_texture, *rgba);
        screen_ms += double(SDL_GetTicksNS() - t0) / 1e6;
        ++screen_updates;
    }
    double screen_ms = 0;  // decode + upload time, logged at the end of the live
    float facing = 0;        // the Member camera's eased yaw of the followed member
    int facing_member = -1;
    float frame_dt = 0;      // seconds since the last Draw (the easing)
    double last_draw_ms = -1;
    long screen_updates = 0;

    void Load(Renderer& renderer, const Vfs& vfs, const LiveOptions& opt, int monitor) {
        std::string error;
        const std::string stage_path = FindEntry(vfs, "stage/" + opt.stage + ".pac/model.pac", "");
        has_stage = !stage_path.empty() && stage.Load(renderer, vfs, stage_path, &error);
        if (!has_stage) LogWarn("live: stage {}: {}", opt.stage, stage_path.empty() ? "not found" : error);
        LoadScreens(renderer, vfs, opt, monitor);
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
        for (int m = opt.member ? opt.member : 1; m <= (opt.member ? opt.member : 22); ++m) {
            char chr[64], mot[64], prefix[32];
            std::snprintf(chr, sizeof(chr), "chr/c%02d/c%02d.pac/model.pac", m, m);
            std::snprintf(mot, sizeof(mot), "live/%04d/c%02d_%04d.pac", opt.song, m, opt.song);
            std::snprintf(prefix, sizeof(prefix), "mod_%02d_", m);
            std::string model_path = FindEntry(vfs, chr, prefix), textures;
            const std::string motion_path = FindEntry(vfs, mot, "mot_");
            // chr/cNN/dXXX/dXXX.pac: the whole member in that costume (face and hair textures stay in the member's own
            // model.pac): the member's own costume first, then the chosen one; members with neither keep theirs
            const auto own = opt.wear.find(m);
            for (const std::string& costume : {own == opt.wear.end() ? std::string() : own->second, opt.costume}) {
                if (costume.empty()) continue;
                char dress[64];
                // "dNNN", or "dNNN_v1" for the special colour (M13), which sits in the dNNN folder
                std::snprintf(dress, sizeof(dress), "chr/c%02d/%s/%s.pac", m, costume.substr(0, 4).c_str(), costume.c_str());
                const std::string dressed = FindEntry(vfs, dress, "mod_");
                if (!dressed.empty()) {
                    model_path = dressed, textures = chr;
                    if (costume != opt.costume) LogInfo("live: member {} wears {} (own costume)", m, costume);
                    break;
                }
            }
            if (motion_path.empty()) continue;  // not in this song
            auto d = std::make_unique<Dancer>();
            d->member = m;
            if (model_path.empty() || !d->model.Load(renderer, vfs, model_path, &error, textures)) {
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
    void Draw(Renderer& renderer, double now_ms, float start_frame, const CameraRig* rig = nullptr) {
        frame_dt = last_draw_ms < 0 ? 0 : float(std::clamp((now_ms - last_draw_ms) / 1000.0, 0.0, 0.1));
        last_draw_ms = now_ms;
        const float frame = float(std::max(0.0, now_ms) / 1000.0 * 60.0) + start_frame;  // dance and camera clock, 60 fps
        for (auto& d : dancers) d->model.Pose(renderer, d->has_dance ? &d->dance : nullptr, frame);
        if (has_stage && !stage_loops.empty()) {
            std::vector<bmar::Layer> layers;
            for (const auto& l : stage_loops) layers.push_back({&l->motion, std::fmod(frame, l->motion.frames)});
            stage.Pose(renderer, layers);
        }
        const float aspect = float(renderer.Width()) / float(std::max(1, renderer.Height()));
        Vec3 head{}, eye_l{}, eye_r{};
        if (rig && rig->mode == 1 && size_t(rig->member) < dancers.size() &&
            dancers[size_t(rig->member)]->model.NodePosition("Bip001_Head", head)) {
            // her facing from the line between her eyes (left minus right points to her left; the front is that
            // crossed with up), turned into a yaw and eased so the camera follows her turns without snapping
            const SceneModel& m = dancers[size_t(rig->member)]->model;
            if (m.NodePosition("Bip001_eye_L_ctrl", eye_l) && m.NodePosition("Bip001_eye_R_ctrl", eye_r)) {
                const Vec3 front = Cross(eye_l - eye_r, {0, 0, 1});
                if (front.x * front.x + front.y * front.y > 1e-8f) {
                    const float want = std::atan2(front.x, -front.y);
                    float d = std::remainder(want - facing, 6.2831853f);
                    if (rig->member != facing_member) d = want - facing, facing_member = rig->member;  // a new member: at once
                    facing += d * std::min(1.0f, frame_dt * 3.0f);
                }
            }
            renderer.SetCamera(CameraRig::Look(rig->member_orbit, head, 0.55f, aspect, facing));
        } else if (rig && rig->mode == 2) {
            renderer.SetCamera(CameraRig::Look(rig->view_orbit, {0, 0, 1.0f}, 0.70f, aspect));
        } else if (has_camera) {
            const bscm::Pose pose = bscm::Sample(camera, frame);
            renderer.SetCamera(Perspective(pose.fov_deg * 3.14159265f / 180.0f, aspect, 0.1f, 500.0f) * bscm::ViewMatrix(pose));
        } else {  // fixed front view; Z-up world, models face -Y
            renderer.SetCamera(Perspective(0.70f, aspect, 0.1f, 500.0f) * LookAt({0, -6.0f, 1.4f}, {0, 0, 1.0f}, {0, 0, 1}));
        }
        if (has_stage) stage.Draw(renderer, Mat4{}, screens.empty() ? nullptr : &screens);
        for (auto& d : dancers) d->model.Draw(renderer, Mat4{});
    }

    void Release(Renderer& renderer) {
        renderer.Release(screen_texture);
        stage.Release(renderer);
        for (auto& d : dancers) d->model.Release(renderer);
    }
};

const float kGradeColor[live::kGrades][4] = {
    {1.0f, 0.85f, 0.2f, 1}, {1.0f, 0.45f, 0.7f, 1}, {0.4f, 0.9f, 0.5f, 1}, {0.35f, 0.5f, 1.0f, 1}, {0.5f, 0.5f, 0.55f, 1}};

// The skill banner: ui2/90_ingame uil0005_ActiveSkillLayout00 places six uip0018_ActiveSkill00 (a plate of
// ui_ingame_skill_frame00 and a name of ui_ingame_skill_text00_T); the port uses the four at the top (_00-_03; _04/_05
// at the bottom corners are a guess for the support players). The name atlas has 4 x 10 cells (320 x 48 px) by kind.
const std::string kSkillHud = "uil0005_ActiveSkillLayout00";

// View Live Show's HUD (M12 P4; shadPS4 work/m12/shad2/v03-v11): ui2/90_ingame uil0002_LiveView00. The camera bar
// (uip0010: "Camera: R1 MV >> Member >> View", the current mode lit), one key-help part per mode (uip0011-0013), the
// Take Photo / Info ON/OFF buttons (uip0014), the member icons of the Member camera (uip0017), the photo counter
// (uip0015) and up to five thumbnails (uip0016), and the song name (uip0005_Songname_00).
const std::string kLiveViewHud = "uil0002_LiveView00";
constexpr const char* kCameraModeNames[3] = {"MV", "Member", "View"};
void ApplyLiveViewHud(UiScreen& hud, int mode, int photos, const std::vector<int>& members = {}, int focus = 0) {
    const std::string bar = kLiveViewHud + "/uip0010_CameraModeInfo00";
    static constexpr const char* kNodes[3] = {"cam_mv", "cam_member", "cam_view"};
    for (int m = 0; m < 3; ++m) {
        hud.HideNode(bar, std::string(kNodes[m]) + "_on", m != mode);
        hud.HideNode(bar, std::string(kNodes[m]) + "_off", m == mode);
    }
    // the MV help (L1 "Launch Finale Focus") was not shown in the game's MV mode (shad2/v03); Finale Focus is not built
    hud.HideInstance(kLiveViewHud + "/uip0011_MvCameraInfo00", true);
    hud.HideInstance(kLiveViewHud + "/uip0012_MembCameraInfo00", mode != 1);
    hud.HideInstance(kLiveViewHud + "/uip0013_ViewCameraInfo00", mode != 2);
    hud.HideInstance(kLiveViewHud + "/uip0017_CharaOrder00", mode != 1);
    hud.HideInstance(kLiveViewHud + "/uip0020_Warning00", true);
    // the Member camera's members, from the left, with the camera icon over the followed one; ui_medley_ch_icon00 has
    // 5 x 4 cells in member order (µ's 1-9, Aqours 11-19, Saint Snow 21-22)
    const std::string order = kLiveViewHud + "/uip0017_CharaOrder00";
    for (int k = 0; k < 11; ++k) {
        char chara[24], icon[24];
        std::snprintf(chara, sizeof(chara), "focus_chara%02d", k);
        std::snprintf(icon, sizeof(icon), "focus_icon%02d", k);
        const bool on = size_t(k) < members.size();
        hud.HideNode(order, chara, !on);
        hud.HideNode(order, icon, !on || k != focus);
        if (!on) continue;
        const int m = members[size_t(k)], cell = m <= 9 ? m - 1 : m <= 19 ? m - 2 : m - 3;
        hud.SetMaterialUv(order, std::string(chara) + "_M", 0.2f * float(cell % 5), 0.25f * float(cell / 5));
    }
    // shortcut: "Photos" is the English label (message 657) in every language; the counter is the game's "n/5"
    hud.SetInstanceText(kLiveViewHud + "/uip0015_PhotoNamber00", 0, "Photos");
    hud.SetInstanceText(kLiveViewHud + "/uip0015_PhotoNamber00", 1, std::to_string(photos) + "/5");
    for (int k = 0; k < 5; ++k) {
        hud.HideNode(kLiveViewHud + "/uip0016_ScreenShot00", "screen_shot0" + std::to_string(k), k >= photos);
    }
}
constexpr int kSkillBanners = 4;
constexpr double kUseBannerMs = 2000;  // shortcut: an untimed skill's use shows this long; the game's time is not traced
std::array<float, 2> SkillNameCell(int kind) {
    switch (kind) {
    case 13: return {0, 0};      // Judge Support
    case 0: return {0.25f, 0};   // Judge Plus
    case 1: return {0.5f, 0};    // Combo Support
    case 14: return {0.75f, 0};  // Finale Support
    case 15: return {0, 0.1f};   // Long Notes Support
    case 38: return {0.25f, 0.1f};  // Judge Support +
    case 17: return {0.5f, 0.1f};   // Starting Combo Support
    case 16: return {0, 0.2f};      // Shoutout
    case 6: return {0.75f, 0.2f};   // Member Focus
    case 36: return {0, 0.4f};      // Special Effect Shower
    case 37: return {0.25f, 0.4f};  // Special Effect Frame
    default: return {0, 0};
    }
}

}

int RunLive(SDL_Window* window, Renderer& renderer, AudioOutput& audio, const Vfs& vfs, const AppSettings& settings,
            const LiveOptions& opt, LiveResult* result) {
    route::SetScreen("live");
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
    const double approach_ms =
        opt.approach_ms > 0 ? opt.approach_ms : song.rules.approach_ms[size_t(std::clamp(opt.note_speed, 0, 4))];
    LogInfo("live: difficulty index {}, note speed {}, travel {:.0f} ms", song.rules.difficulty, opt.note_speed, approach_ms);
    const live::Chart& chart = song.chart;
    const double audio_ms = std::max(1.0, double(music.Frames()) * 1000.0 / music.rate);
    std::array<bool, 3> set_slots{};
    for (const live::Skill& s : opt.skills) set_slots[size_t(std::clamp(s.slot, 0, 2))] = true;
    live::MarkSkillNotes(song.chart, set_slots);
    for (const live::Skill& s : opt.skills) {
        double at = -1;
        for (const live::Note& n : chart.notes) {
            if (n.skill == s.slot) at = n.time_ms;
        }
        LogInfo("live: skill slot {}: card {} kind {}, {}, skill note at {}", s.slot + 1, s.card, s.kind,
                s.duration_ms > 0 ? std::to_string(int(s.duration_ms)) + " ms" : "untimed, amount " + std::to_string(s.amount),
                at >= 0 ? std::to_string(int(at)) + " ms" : "none");
    }
    live::Judge judge;
    judge.Reset(chart, song.windows, song.rules, opt.skills);
    const double offset = settings.timing.audio_offset_ms;
    LiveScene scene;
    LiveOptions dressed = opt;
    if (dressed.costume.empty()) {  // no costume chosen: the song's own, else the school uniform (live::PickCostume)
        std::vector<live::SongInfo> songs;
        std::vector<live::DressInfo> dresses;
        if (live::LoadSongList(vfs, 1, songs) && live::LoadDressList(vfs, 1, dresses)) {
            for (const live::SongInfo& s : songs) {
                if (s.id == opt.song) dressed.costume = live::PickCostume(vfs, s, dresses);
            }
        }
        LogInfo("live: costume {}", dressed.costume.empty() ? "base model" : dressed.costume);
    }
    scene.Load(renderer, vfs, dressed, song.monitor);
    // judgement sounds: sys.acb cues 20030-20034 (Perfect .. miss), 20039-20043 the finale's "_fl" set; the eboot's
    // judgement sound function (0xB06C0) reads them from its table at 0x5E8BA0 (analysis 202611082306). Mixed in the
    // music's format so they can be summed onto it
    std::array<std::array<cri::Pcm, kGrades>, 2> hit_sound;  // [finale][grade]
    {
        cri::CueSheet sys;
        int loaded = 0;
        if (cri::LoadCueSheet(vfs, "sys", sys, &error)) {
            for (int f = 0; f < 2; ++f) {
                for (int g = 0; g < kGrades; ++g) {
                    if (cri::LoadCueMix(vfs, sys, (f ? 20039 : 20030) + g, music.rate, music.channels,
                                        hit_sound[size_t(f)][size_t(g)], &error)) {
                        ++loaded;
                    }
                }
            }
        }
        if (loaded < 2 * kGrades) LogWarn("live: judgement sounds: {} of {} loaded ({})", loaded, 2 * kGrades, error);
    }
    // where the music is audibly over: the files end in seconds of silence (song 0010: audible to 121.1 s of 126.4 s)
    double audible_end_ms = 0;
    for (size_t i = music.samples.size(); i-- > 0;) {
        if (std::abs(int(music.samples[i])) > 328) {  // about -40 dBFS
            audible_end_ms = double(i / size_t(std::max(1, music.channels))) * 1000.0 / music.rate;
            break;
        }
    }
    audio.Play(std::move(music));

    Layout layout;
    Playfield field;
    if (!opt.view) field.Load(renderer, vfs, song.group);
    field.SetHome(chart.home, opt.variant >= 12 ? HomeConfig::Hard : HomeConfig::Easy);
    // the game's top HUD bar (ui2/90_ingame, layout uil0001_Ingame00); shortcut: gauges and the FULL COMBO marks
    // stay at their defaults until the HUD logic is traced
    long hud_score = -1;
    UiText hud_text;
    UiScreen hud;
    bool has_hud = hud_text.Load(vfs, opt.lang, &error) && hud.Load(renderer, vfs, "ui2/90_ingame", opt.lang, &error, &hud_text);
    if (has_hud) {
        hud.HideInstance("uil0001_Ingame00/uip0008_FinaleON00", true);
        for (int k = 0; k < 6; ++k) hud.HideInstance(kSkillHud + "/uip0018_ActiveSkill00_0" + std::to_string(k), true);
        hud.HideNode("IngamesonggageCol", true);  // the play field draws the filled part of this ring
        for (int k = 0; k < 9; ++k) hud.HideInstance("uil0001_Ingame00/uip0007_FFmark_ef_0" + std::to_string(k), true);
        // the LOVELIVE! marks: the layout's texture is the mu's set; the other groups use their own
        if (song.group == 1 || song.group == 2) {
            const std::string tex = std::string("ui2/90_ingame.pac/texture.pac/ingame_ffmark_") + (song.group == 1 ? "aq.btx" : "ss.btx");
            for (int k = 0; k < 9; ++k) hud.SetMaterialTexture("uil0001_Ingame00/uip0003_FFmark", "M_FFmark_0" + std::to_string(k), tex);
        }
        std::vector<live::SongInfo> songs;
        const int language = opt.lang == "en" ? 1 : opt.lang == "cn" ? 2 : opt.lang == "tw" ? 3 : 0;
        if (live::LoadSongList(vfs, language, songs)) {
            for (const live::SongInfo& s : songs) {
                if (s.id != opt.song) continue;
                hud.SetInstanceText("uil0001_Ingame00/uip0005_Songname", 1, s.title);
                hud.SetInstanceText(kLiveViewHud + "/uip0005_Songname_00", 1, s.title);
            }
        }
        if (opt.view) ApplyLiveViewHud(hud, 0, 0);
    } else {
        LogWarn("live: HUD: {}", error);
    }
    GpuMesh unit_quad;  // centred unit quad over a whole texture (centre circle, hold glow)
    {
        const MeshVertex quad[4] = {{{-0.5f, 0.5f, 0}, {0, 0, 1}, {0, 0}}, {{0.5f, 0.5f, 0}, {0, 0, 1}, {1, 0}},
                                    {{0.5f, -0.5f, 0}, {0, 0, 1}, {1, 1}}, {{-0.5f, -0.5f, 0}, {0, 0, 1}, {0, 1}}};
        const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        renderer.CreateMesh(quad, indices, false, unit_quad, nullptr);
    }
    std::vector<double> flash(kLanes, -1e9);
    std::vector<Grade> flash_grade(kLanes, Grade::Miss);
    std::vector<double> missed(kLanes, -1e9);  // the latest Miss per lane, for the ring break
    size_t log_seen = 0, auto_next = 0, skill_seen = 0;
    std::vector<live::SkillEvent> banners;  // skills in effect, shown by the HUD
    std::vector<std::pair<double, int>> auto_releases;  // autoplay: (time, lane)
    uint64_t end_ticks = 0;  // when every note was judged (0: not yet)
    // The ending, in ms after end_ticks (the game's frames, analysis/202611101907-checklist-feedback/): FULL COMBO or
    // ALL PERFECT from 0 when earned; LIVE CLEAR 3.9 s later, or when the music is audibly over if that is later; the
    // dance goes on; 4.5 s after LIVE CLEAR began, 1.0 s to white, then the 0.3 s screen fade to black
    int award = -1;          // the completion texture half: 0 FULL COMBO, 1 ALL PERFECT, -1 none
    double clear_at = 0;     // LIVE CLEAR start
    static constexpr double kAwardToClear = 3900, kClearToWhite = 4500, kWhiteMs = 1000;
    bool closed = false;
    std::vector<double> frame_cpu_ms;
    long frame = 0;
    ScreenFade fade;  // in from black; out to black at the song's end or on Esc
    uint64_t fade_last = SDL_GetTicksNS();
    SetLiveHome(chart.home, live::IsHomeVariant(opt.variant) && opt.variant >= 12 ? HomeConfig::Hard : HomeConfig::Easy);
    SetPadLayout(opt.view ? PadLayout::View : PadLayout::Live);
    CameraRig rig;           // View Live Show: rig.mode 0 MV, 1 Member, 2 View (R1 = Page Down cycles)
    std::vector<int> dancing;  // the members in the Member camera's order
    for (const auto& d : scene.dancers) dancing.push_back(d->member);
    std::map<SDL_Scancode, bool> held;  // View Live Show: camera keys held down
    uint64_t camera_last = SDL_GetTicksNS();
    bool show_info = true;   // View Live Show: □ (Q) hides the whole HUD
    bool view_ending = false;
    bool take_photo = false;         // View Live Show: ○ (P) pressed; the next frame is captured before the HUD
    std::vector<LiveResult::Photo> photos;
    std::map<int, double> camera_seconds;  // the Member camera's time per member (mission kind 26)
    std::array<Texture, 5> thumbs;   // the photos as HUD thumbnails (a quarter size)
    for (bool running = true; running && !fade.Left() && (opt.max_frames < 0 || frame < opt.max_frames); ++frame) {
        const uint64_t fade_now = SDL_GetTicksNS();
        fade.Update(double(fade_now - fade_last) / 1e9);
        fade_last = fade_now;
        SDL_Event e;
        while (PollEvent(e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
                closed = true;
            } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE) {
                fade.Leave(hud);
            } else if (opt.view) {
                const SDL_Scancode k = e.key.scancode;
                const bool moves = k == SDL_SCANCODE_LEFT || k == SDL_SCANCODE_RIGHT || k == SDL_SCANCODE_UP ||
                                   k == SDL_SCANCODE_DOWN || k == SDL_SCANCODE_PAGEUP || k == SDL_SCANCODE_HOME;
                if (e.type == SDL_EVENT_KEY_UP && held[k] && moves && rig.mode > 0) {  // where a move ended
                    const CameraRig::Orbit& o = rig.mode == 1 ? rig.member_orbit : rig.view_orbit;
                    LogInfo("live view: {} camera yaw {:.2f} pitch {:.2f} distance {:.2f} height {:.2f}",
                            kCameraModeNames[rig.mode], o.yaw, o.pitch, o.dist, o.height);
                }
                if (e.type == SDL_EVENT_KEY_UP) held[e.key.scancode] = false;
                if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat) continue;
                held[e.key.scancode] = true;
                const int n = std::max(1, int(dancing.size()));
                if (e.key.scancode == SDL_SCANCODE_PAGEDOWN) {
                    rig.mode = (rig.mode + 1) % 3;
                    LogInfo("live view: camera mode {}", kCameraModeNames[rig.mode]);
                } else if (e.key.scancode == SDL_SCANCODE_P && !take_photo && photos.size() < 5) {
                    take_photo = true;
                } else if (e.key.scancode == SDL_SCANCODE_Q) {
                    show_info = !show_info;
                    LogInfo("live view: info {}", show_info ? "on" : "off");
                } else if (rig.mode == 1 &&
                           (e.key.scancode == SDL_SCANCODE_COMMA || e.key.scancode == SDL_SCANCODE_PERIOD)) {
                    rig.member = (rig.member + (e.key.scancode == SDL_SCANCODE_PERIOD ? 1 : n - 1)) % n;
                    LogInfo("live view: member camera on member {}", dancing.empty() ? 0 : dancing[size_t(rig.member)]);
                }
                if (has_hud) ApplyLiveViewHud(hud, rig.mode, int(photos.size()), dancing, rig.member);
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
        if (!opt.view) judge.Update(now);
        if (opt.view) {  // the Member and View cameras move while their keys are held
            const uint64_t t = SDL_GetTicksNS();
            const float dt = float(double(t - camera_last) / 1e9);
            camera_last = t;
            if (rig.mode == 1 && size_t(rig.member) < dancing.size()) camera_seconds[dancing[size_t(rig.member)]] += dt;
            if (rig.mode > 0) {
                CameraRig::Move(rig.mode == 1 ? rig.member_orbit : rig.view_orbit, rig.mode == 1, dt,
                                held[SDL_SCANCODE_LEFT], held[SDL_SCANCODE_RIGHT], held[SDL_SCANCODE_UP],
                                held[SDL_SCANCODE_DOWN], held[SDL_SCANCODE_PAGEUP], held[SDL_SCANCODE_HOME]);
            }
        }
        // each judgement plays its sound, each sound at most once per frame (a simultaneous pair judged alike sounds
        // once; shortcut: the game's rule for this and for hold releases is not traced, its caller of 0xB06C0 is a
        // function pointer)
        unsigned played = 0;
        for (; log_seen < judge.Log().size(); ++log_seen) {
            const live::Judgement& j = judge.Log()[log_seen];
            const live::Note& note = chart.notes[size_t(j.note)];
            const int lane = note.lane;
            flash[size_t(lane)] = now;
            flash_grade[size_t(lane)] = j.grade;
            if (j.grade == Grade::Miss) missed[size_t(lane)] = now;
            const int finale = judge.finale && note.time_ms >= chart.finale_ms ? 1 : 0;
            const unsigned bit = 1u << (finale * kGrades + int(j.grade));
            if (!(played & bit)) {
                played |= bit;
                audio.PlayEffect(&hit_sound[size_t(finale)][size_t(j.grade)]);
            }
        }

        // skills taking effect: a log line each and the skill banner while they last. shortcut: the firing voice
        // (eVoice_invokeSkillMain) and the skill effects (ef_842/843) are not traced; skill notes look like icon notes
        for (; skill_seen < judge.SkillLog().size(); ++skill_seen) {
            live::SkillEvent e = judge.SkillLog()[skill_seen];
            const live::Skill& s = judge.Skills()[size_t(e.skill)];
            LogInfo("live: skill fired: slot {} card {} kind {} at {:.0f} ms, until {:.0f} ms", s.slot + 1, s.card, s.kind,
                    e.t_ms, e.until_ms);
            if (e.until_ms <= e.t_ms) e.until_ms = e.t_ms + kUseBannerMs;
            banners.push_back(e);
        }
        std::erase_if(banners, [&](const live::SkillEvent& b) { return now >= b.until_ms; });
        if (has_hud) {
            for (int k = 0; k < kSkillBanners; ++k) {
                const std::string inst = kSkillHud + "/uip0018_ActiveSkill00_0" + std::to_string(k);
                const bool on = size_t(k) < banners.size();
                hud.HideInstance(inst, !on);
                if (!on) continue;
                const live::Skill& s = judge.Skills()[size_t(banners[size_t(k)].skill)];
                const auto cell = SkillNameCell(s.kind);
                hud.SetMaterialUv(inst, "skill_plate00_M", 0, SkillPlateV(s.card));
                hud.SetMaterialUv(inst, "skill_name00_M", cell[0], cell[1]);
            }
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
            // uip0003_FFmark: nine 68 px cells of ingame_ffmark_<group> (4 x 2); each collected icon lights the next
            // letter of L O V E L I V E !, the rest show the grey symbol (0, 0)
            static constexpr int kLetter[9][2] = {{1, 0}, {2, 0}, {3, 0}, {0, 1}, {1, 0}, {1, 1}, {3, 0}, {0, 1}, {2, 1}};
            const int lit = judge.icons;
            for (int k = 0; k < 9; ++k) {
                hud.SetMaterialUv("uil0001_Ingame00/uip0003_FFmark", "M_FFmark_0" + std::to_string(k),
                                  k < lit ? 0.25f * float(kLetter[k][0]) : 0, k < lit ? 0.5f * float(kLetter[k][1]) : 0);
            }
        }
        if (field.loaded) {
            field.SetGauge(renderer, float(now / audio_ms));
            field.SetBands(renderer, chart, judge, now, approach_ms);
        }
        scene.UpdateScreens(renderer, now);
        if (renderer.BeginFrame()) {
            const uint64_t draw_start = SDL_GetTicksNS();
            scene.Draw(renderer, now, song.dance_start_frame, opt.view ? &rig : nullptr);
            if (take_photo) renderer.CaptureFrame();  // the 3D view only: the HUD is drawn after the copy
            if (opt.view) {
                if (has_hud && show_info) hud.DrawLayout(renderer, kLiveViewHud, 1.0f);
            } else if (field.loaded) {
                for (int l = 0; l < kLanes; ++l) {
                    if (!field.Shown(l)) continue;
                    const auto& p = field.lane[size_t(l)];
                    field.Draw(renderer, field.Blue(l) ? field.indicator_blue : field.indicator, field.mark, p[0], p[1],
                               Playfield::kCell, {1, 1, 1, 1});
                    field.DrawIcons(renderer, l, p[0], p[1], 1.0f, false);
                }
                // a Miss breaks the lane's ring (shortcut: the trigger is a guess from the animation's name and its
                // ring-sized shards; the game's use is not traced)
                for (int l = 0; l < kLanes; ++l) {
                    const auto& p = field.lane[size_t(l)];
                    field.DrawFad(renderer, field.ring_break, now - missed[size_t(l)], p[0], p[1]);
                }
                // notes fly from the centre circle to their ring, growing (far notes first, nearer ones on top)
                const auto at = [&](int lane, double t, float& x, float& y, float& size) {
                    field.Path(lane, t, now, approach_ms, x, y, size);
                };
                field.DrawBands(renderer);  // hold bands and their end discs, under every note
                for (size_t i = chart.notes.size(); i-- > 0;) {
                    const live::Note& n = chart.notes[i];
                    if (!judge.Active(i) || n.time_ms - now > approach_ms) continue;
                    float x, y, size;
                    at(n.lane, judge.Held(i) ? now : n.time_ms, x, y, size);
                    field.DrawNote(renderer, n.lane, x, y, size, 1.0f, judge.finale && n.time_ms >= chart.finale_ms);
                    // a simultaneous note wears the brackets, orange and blue by turns per group; a held one keeps them
                    // on its ring (the user's "vertical line"), drawn kBracketScale x the note
                    if (n.icon) field.Draw(renderer, field.icon_mark, field.atlas, x, y, size, {1, 1, 1, 1});
                    if (n.group >= 0) {
                        field.Draw(renderer, field.bracket[size_t(n.group % 2)], field.atlas, x, y,
                                   size * Playfield::kBracketScale, {1, 1, 1, 1});
                    }
                    if (judge.Held(i)) {  // a held long note: a glowing ring over it, pulsing (pulse rate is a guess)
                        const auto& p = field.lane[size_t(n.lane)];
                        const float pulse = float(0.7 + 0.3 * std::sin(now / 60.0));
                        field.Draw(renderer, unit_quad, field.hold_glow, p[0], p[1], field.ring_size * 1.58f, {1, 1, 1, pulse});
                    }
                }
            }
            // the HUD and the centre circle (gauge track, fan, mic) over the notes: notes come out from under the mic
            if (has_hud && !opt.view) hud.DrawLayout(renderer, "uil0001_Ingame00", 1.0f);
            if (has_hud && !banners.empty()) hud.DrawLayout(renderer, kSkillHud, 1.0f);
            if (field.loaded) {
                field.Draw(renderer, field.gauge_fan, field.center_gauge, field.center[0], field.center[1], Playfield::kGaugeSize, {1, 1, 1, 1});
                field.Draw(renderer, unit_quad, field.center_mic, field.center[0], field.center[1], field.center_size * 0.55f, {1, 1, 1, 1});
                field.DrawCombo(renderer, judge.combo, 960, 722, 0.4f);
                // each lane's latest judgement at its own ring: the animation's canvas centre (its ring bursts' centre)
                // on the ring centre, so the text arches over the ring as in the game
                for (int l = 0; l < kLanes; ++l) {
                    const auto& p = field.lane[size_t(l)];
                    field.DrawFad(renderer, field.judge_fad[size_t(flash_grade[size_t(l)])], now - flash[size_t(l)], p[0], p[1]);
                }
                // the ending texts on the full-combo banner's spot, over the combo (Hypothesis)
                if (end_ticks) {
                    const double since = double(SDL_GetTicksNS() - end_ticks) / 1e6;
                    if (award >= 0) field.DrawFad(renderer, field.live_clear, since, field.banner[0], field.banner[1], award);
                    field.DrawFad(renderer, field.live_clear, since - clear_at, field.banner[0], field.banner[1]);
                }
            } else if (!opt.view) {  // placeholder lanes when the game's play field cannot be loaded
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
                const float p = float(std::clamp(1.0 - (t - now) / approach_ms, 0.0, 1.0));
                float lx, ly;
                layout.Lane(lane, lx, ly);
                x = layout.cx + (lx - layout.cx) * p;
                y = layout.cy + (ly - layout.cy) * p;
                r = layout.button * (0.2f + 0.75f * p);
            };
            // far notes first, so nearer ones are drawn on top
            for (size_t i = chart.notes.size(); i-- > 0;) {
                const live::Note& n = chart.notes[i];
                if (!judge.Active(i) || n.time_ms - now > approach_ms) continue;
                float x, y, r;
                if (n.IsHold()) {
                    const float trail[4] = {0.95f, 0.95f, 1.0f, 0.35f};
                    const double head = judge.Held(i) ? now : n.time_ms;
                    for (double t = n.end_ms; t > head; t -= 40) {  // trail: discs from the tail to the head
                        if (t - now > approach_ms) continue;
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
            if (end_ticks) {  // the ending's fade to white
                const float white[4] = {1, 1, 1, float(std::clamp((double(SDL_GetTicksNS() - end_ticks) / 1e6 - clear_at -
                                                                   kClearToWhite) / kWhiteMs, 0.0, 1.0))};
                const float w = float(renderer.Width()), h = float(renderer.Height());
                if (white[3] > 0) renderer.DrawCircle(w / 2, h / 2, std::hypot(w, h), 0, white);
            }
            fade.Draw(renderer);
            renderer.EndFrame();
            LiveResult::Photo shot;
            if (take_photo && renderer.TakeCapture(shot.rgba, shot.width, shot.height)) {
                // the HUD thumbnail: every 4th pixel (shortcut: no filtering; it is 164 px wide on screen)
                const uint32_t tw = std::max(1u, shot.width / 4), th = std::max(1u, shot.height / 4);
                std::vector<uint8_t> small(size_t(tw) * th * 4);
                for (uint32_t y = 0; y < th; ++y) {
                    for (uint32_t x = 0; x < tw; ++x) {
                        std::memcpy(&small[(size_t(y) * tw + x) * 4], &shot.rgba[(size_t(y) * 4 * shot.width + x * 4) * 4], 4);
                    }
                }
                const size_t k = photos.size();
                if (renderer.CreateTexture(tw, th, thumbs[k], nullptr) && renderer.UpdateTexture(thumbs[k], small) && has_hud) {
                    hud.SetMaterialTexture(kLiveViewHud + "/uip0016_ScreenShot00", "screen_shot_0" + std::to_string(k) + "_M",
                                           &thumbs[k]);
                }
                LogInfo("live view: photo {} taken ({}x{}, camera {})", k + 1, shot.width, shot.height,
                        kCameraModeNames[rig.mode]);
                shot.camera_member = rig.mode == 1 && size_t(rig.member) < dancing.size() ? dancing[size_t(rig.member)] : 0;
                photos.push_back(std::move(shot));
                if (has_hud) ApplyLiveViewHud(hud, rig.mode, int(photos.size()), dancing, rig.member);
            }
            take_photo = false;
            const double cpu_ms = double(SDL_GetTicksNS() - draw_start) / 1e6;  // CPU cost of one frame, without the vsync wait
            frame_cpu_ms.push_back(cpu_ms);
        } else {
            SDL_Delay(5);
        }

        if (frame % 15 == 0 && !opt.view) {
            char title[256];
            std::snprintf(title, sizeof(title), "SIF port - %s [%02d]  combo %d  P %d / Gr %d / Go %d / B %d / M %d  score %.0f",
                          song.title.c_str(), opt.variant, judge.combo, judge.counts[0], judge.counts[1], judge.counts[2],
                          judge.counts[3], judge.counts[4], judge.score);
            SDL_SetWindowTitle(window, title);
        }
        // after the last judgement the ending plays (wall clock: the audio clock stops with the music), then the
        // screen fades out
        if (opt.view && !view_ending && !audio.Playing()) {  // the song is over: back to the song select (P4d: End screen)
            view_ending = true;
            LogInfo("live view end");
            fade.Leave(hud);
        }
        if (!opt.view && judge.Finished() && !end_ticks) {
            end_ticks = SDL_GetTicksNS();
            const bool full_combo = judge.counts[2] + judge.counts[3] + judge.counts[4] == 0;  // as main.cpp's records
            award = full_combo ? (judge.counts[1] == 0 ? 1 : 0) : -1;
            const double music_left = audible_end_ms - audio.PlayheadMs(end_ticks);
            clear_at = std::max(award >= 0 ? kAwardToClear : 0.0, music_left);
            LogInfo("live end: {}LIVE CLEAR in {:.1f} s", award == 1 ? "ALL PERFECT, " : award == 0 ? "FULL COMBO, " : "",
                    clear_at / 1000);
        }
        if (end_ticks && double(SDL_GetTicksNS() - end_ticks) / 1e6 > clear_at + kClearToWhite + kWhiteMs) fade.Leave(hud);
    }

    audio.StopEffects();  // hit_sound is about to go away
    for (Texture& t : thumbs) renderer.Release(t);
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
        if (scene.screen_updates) {  // decoded outside the measured draw: reported on its own, per frame shown
            LogInfo("live stage screens: {} pictures, decode + upload {:.2f} ms each, {:.2f} ms per frame on average",
                    scene.screen_updates, scene.screen_ms / double(scene.screen_updates),
                    scene.screen_ms / double(frame_cpu_ms.size()));
        }
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
    SetPadLayout(PadLayout::Menu);
    SetLiveHome(false);
    LogInfo("live result: Perfect {} Great {} Good {} Bad {} Miss {}, max combo {}, provisional score {:.0f}; "
            "press offset mean {:+.1f} ms, sd {:.1f} ms over {} hits",
            judge.counts[0], judge.counts[1], judge.counts[2], judge.counts[3], judge.counts[4], judge.max_combo,
            judge.score, mean, sd, n);
    if (result) {
        *result = {opt.song, opt.variant, {}, judge.max_combo, judge.score, judge.Finished(), closed, judge.ClearRate(),
                   judge.Rank()};
        std::copy(judge.counts.begin(), judge.counts.end(), result->counts.begin());
        result->costume = dressed.costume;
        result->photos = std::move(photos);
        result->camera_seconds = std::move(camera_seconds);
    }
    return 0;
}

bool RunCalibration(SDL_Window* window, Renderer& renderer, AudioOutput& audio, AppSettings& settings, long max_frames) {
    route::SetScreen("calibrate");
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
        while (PollEvent(e)) {
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
