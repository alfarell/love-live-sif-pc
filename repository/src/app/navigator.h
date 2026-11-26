#pragma once
// The 3D member of the menus (M13 N4): the top menu's navigator, later the costume screen's member. Her model in her
// costume, the game's idle loop (stand, a wait of 2-5 s, a weighted idle clip; N1 §3) and a talk motion with a voice
// line. analysis/202611172106-navigator-and-try-on/.

#include <memory>
#include <string>
#include <vector>

#include "engine/formats/bmar.h"
#include "engine/render/scene_model.h"
#include "game/member/member.h"
#include "game/profile/profile.h"

namespace sif {

class Renderer;
class Vfs;

class Navigator {
public:
    // `costume`: dNNN (-1 = the group's default), `special`: its _v1 colours.
    bool Load(Renderer& renderer, const Vfs& vfs, int member, int costume, bool special, std::string* error = nullptr);
    // Advance by `seconds` (motions run at 60 frames per second, as the game's).
    void Update(double seconds);
    // Play one of the member's talk motions (503-506) once, then back to standing.
    void Talk();
    // Play motion `number` once (e.g. 518 dresschk, 519 dressfix), then back to standing.
    void Play(int number);
    // Holds facial clip `number` (1 "a" .. 252 brows; -1 none) at its last frame over the body motion: its blend
    // shapes at their full weight (M18 U4). `TestFace` (the --face option) gives every navigator that face.
    void Face(int number);
    static void TestFace(int number);
    // `view_proj`: the camera; she is drawn at the origin, facing -Y (Z-up), as in lives.
    void Draw(Renderer& renderer, const Mat4& view_proj);
    void Release(Renderer& renderer);
    int Member() const { return member_; }
    // The top menu's camera: she stands in the right half, head to thighs (fitted to the N2 shots).
    static Mat4 MenuCamera(float aspect);
    // Create/Change Costumes: her whole figure on the left (fitted to the N2 shots cs2.png-cs6.png).
    static Mat4 CostumeCamera(float aspect);

private:
    struct Clip {
        int number = 0;
        std::vector<uint8_t> file;
        bmar::Motion motion;
    };
    const Clip* Get(int number);
    void Start(int number);

    const Vfs* vfs_ = nullptr;
    int member_ = 0;
    SceneModel model_;
    bool loaded_ = false;
    std::vector<std::unique_ptr<Clip>> clips_;  // loaded on first use
    member::IdleTable idle_;
    profile::XorShift128 random_;  // the eboot's generator and seed (its own instance, as the game's idle code)
    const Clip* Facial(int number);
    const Clip* playing_ = nullptr;
    const Clip* face_ = nullptr;
    float frame_ = 0;
    float wait_ = 0;  // frames left standing before the next idle
};

}
