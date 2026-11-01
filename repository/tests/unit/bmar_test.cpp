// BMAR model/motion reader checks against the user's CUSA24620 dump (first argument). Expected counts come from
// tools/bmar_dump.py on the same files (M2/M5).
#include <cmath>
#include <cstdio>
#include <string>

#include "engine/formats/bmar.h"
#include "engine/formats/bscm.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"

using namespace sif;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", what.c_str());
    }
}

// largest |world(node) x inverse bind - identity| over the palette
static float PaletteError(const bmar::Model& m, const std::vector<Mat4>& world) {
    float worst = 0;
    for (const bmar::PaletteEntry& e : m.palette) {
        const Mat4 p = world[e.node] * e.inverse_bind;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) worst = std::max(worst, std::abs(p(r, c) - (r == c ? 1.0f : 0.0f)));
        }
    }
    return worst;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sif_bmar_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    std::string error;
    std::vector<uint8_t> file;

    bmar::Model honoka;
    Check(vfs.Read("chr/c01/c01.pac/model.pac/mod_01_honoka", file, &error) && bmar::Parse(file, honoka, &error),
          "parse mod_01_honoka: " + error);
    Check(honoka.nodes.size() == 218 && honoka.meshes.size() == 11 && honoka.draws.size() == 11 &&
              honoka.materials.size() == 6 && honoka.palette.size() == 114,
          "mod_01_honoka: 218 nodes, 11 meshes, 11 draws, 6 materials, 114 palette entries");
    Check(honoka.textures.size() == 46 && honoka.textures[0].name == "Honoka_face_default", "embedded BMT: 46 textures");
    std::vector<Mat4> world;
    bmar::Pose(honoka, nullptr, 0, world);
    const float bind_error = PaletteError(honoka, world);
    Check(bind_error < 1e-3f, "bind pose: palette = identity (error " + std::to_string(bind_error) + ")");

    std::vector<uint8_t> mot_file;
    bmar::Motion dance;
    Check(vfs.Read("live/0010/c01_0010.pac/mot_01_honoka_0010_borarara", mot_file, &error) &&
              bmar::ParseMotion(mot_file, dance, &error),
          "parse dance motion: " + error);
    Check(dance.tracks.size() == 83 && dance.fps == 60 && dance.frames == 9060, "dance: 83 tracks, 9060 frames at 60 fps");
    bmar::Pose(honoka, &dance, 1800, world);
    Check(PaletteError(honoka, world) > 0.1f, "frame 1800 moves the skeleton");

    bmar::Model stage;
    Check(vfs.Read("stage/st017.pac/model.pac/017_livehall.bmarc", file, &error) && bmar::Parse(file, stage, &error),
          "parse 017_livehall: " + error);
    Check(stage.nodes.size() == 98 && stage.meshes.size() == 66 && stage.materials.size() == 35 && stage.palette.empty(),
          "017_livehall: 98 nodes, 66 meshes, 35 materials, no skin");

    // song 0010 camera: BSCM keys (Y-up) and the BMAR look-at node (Z-up) must agree through ViewMatrix
    const std::string cam = "live/0010/0010.pac/scene.pac/camera_anim_0010_00_main";
    std::vector<uint8_t> cam_file, interest_file;
    bscm::Camera camera;
    bmar::Model interest_model;
    bmar::Motion interest_motion;
    Check(vfs.Read(cam + ".bscam", cam_file, &error) && bscm::Parse(cam_file, camera, &error), "parse bscam: " + error);
    Check(vfs.Read(cam + ".bmarc", interest_file, &error) && bmar::Parse(interest_file, interest_model, &error) &&
              bmar::ParseMotion(interest_file, interest_motion, &error),
          "parse camera bmarc: " + error);
    Check(camera.frames == 9060 && camera.position.size() == 9060 && camera.fov.size() == 9060, "bscam: 9060 keys");
    const bscm::Pose p0 = bscm::Sample(camera, 0);
    Check(std::abs(p0.position.z - 11.048f) < 1e-3f && std::abs(p0.fov_deg - 30.169f) < 1e-2f, "bscam frame 0: z 11.05, fov 30.17");
    for (float frame : {0.0f, 1311.0f, 4500.0f, 8000.0f}) {
        bmar::Pose(interest_model, &interest_motion, frame, world);
        const Mat4& w = world.back();  // Camera_Interest, the last node
        const Vec3 v = TransformPoint(bscm::ViewMatrix(bscm::Sample(camera, frame)), {w(0, 3), w(1, 3), w(2, 3)});
        const float off_axis = std::sqrt(v.x * v.x + v.y * v.y) / std::max(1e-6f, -v.z);
        Check(v.z < 0 && off_axis < 0.01f, "camera looks at Camera_Interest at frame " + std::to_string(int(frame)));
    }

    Check(bmar::HalfToFloat(0x3C00) == 1.0f && bmar::HalfToFloat(0xC000) == -2.0f && bmar::HalfToFloat(0x3555) > 0.333f,
          "half floats");
    std::printf("%s\n", failures ? "bmar_test FAILED" : "bmar_test ok");
    return failures ? 1 : 0;
}
