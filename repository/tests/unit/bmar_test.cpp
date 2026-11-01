// BMAR model/motion reader checks against the user's CUSA24620 dump (first argument). Expected counts come from
// tools/bmar_dump.py on the same files (M2/M5).
#include <cmath>
#include <cstdio>
#include <string>

#include "engine/formats/bmar.h"
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

    Check(bmar::HalfToFloat(0x3C00) == 1.0f && bmar::HalfToFloat(0xC000) == -2.0f && bmar::HalfToFloat(0x3555) > 0.333f,
          "half floats");
    std::printf("%s\n", failures ? "bmar_test FAILED" : "bmar_test ok");
    return failures ? 1 : 0;
}
