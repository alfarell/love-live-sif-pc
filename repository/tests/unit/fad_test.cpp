// .fad 2D animations (src/engine/formats/fad.cpp) against the user's own dump; values from tools/fad_dump.py.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/formats/fad.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"

using namespace sif;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sif_fad_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    const auto load = [&](const char* name, fad::Animation& a) {
        std::vector<uint8_t> file;
        std::string error;
        const bool ok = vfs.Read(std::string("ui/ui_ingame.pac/anim.pac/") + name, file, &error) && fad::Parse(file, a, &error);
        Check(ok, std::string(name) + ": " + error);
        return ok;
    };
    fad::Animation miss;
    if (load("ui_ingame_judge_text_miss.fad", miss)) {
        Check(miss.width == 400 && miss.height == 400 && miss.fps == 60 && miss.frames.size() == 61, "miss: 400 x 400, 60 fps, 61 frames");
        const fad::Sprite& s = miss.sprites[2];
        Check(s.texture == "ui_ingame_judge_text_miss" && s.width == 140 && s.height == 72 && s.matrix.tx == -70 &&
                  s.matrix.ty == -36, "miss: sprite 2 = the text, centred on its origin");
        const fad::Placed& p0 = miss.frames[0].at(1);
        Check(p0.sprite == 2 && p0.matrix.tx == 203 && p0.matrix.ty == 101, "miss: frame 0 places sprite 2 at (203, 101)");
        Check(std::abs(miss.frames[10].at(1).colour.mul[3] - 255.0f / 256) < 1e-6f && miss.frames[24].at(1).colour.mul[3] == 0,
              "miss: alpha 255/256 at frame 10, 0 at frame 24");
        const fad::Matrix world = p0.matrix * s.matrix;
        Check(world.tx == 133 && world.ty == 65, "miss: the text's top-left on the canvas = (133, 65)");
    }
    // every file decodes and has its header's frame count
    const struct { const char* name; size_t frames; } all[] = {
        {"ui_ingame_judge_text_perfect.fad", 63}, {"ui_ingame_judge_text_great.fad", 60},
        {"ui_ingame_judge_text_good.fad", 61},    {"ui_ingame_judge_text_bad.fad", 60},
        {"ui_ingame_judgering_break.fad", 60},    {"ui_ingame_end_text_liveclear.fad", 200},
    };
    for (const auto& f : all) {
        fad::Animation a;
        if (load(f.name, a)) Check(a.frames.size() == f.frames, std::string(f.name) + ": " + std::to_string(a.frames.size()) + " frames");
    }
    fad::Animation perfect;
    if (load("ui_ingame_judge_text_perfect.fad", perfect)) Check(perfect.sprites.size() == 11, "perfect: 11 sprites");
    std::printf("%s\n", failures ? "fad_test FAILED" : "fad_test ok");
    return failures ? 1 : 0;
}
