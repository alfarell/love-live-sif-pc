// M8 V1: USM demux and MPEG-1 decoding of the dump's movies. Usage: sif_video_test [dump folder [--all]]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "engine/video/movie.h"

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// mean and spread of a frame's luma-ish value, to tell a picture from a blank or garbage buffer
static void Stats(const std::vector<uint8_t>& rgba, double& mean, double& spread) {
    double sum = 0, sum2 = 0;
    const size_t n = rgba.size() / 4;
    for (size_t i = 0; i < n; ++i) {
        const double v = (rgba[i * 4] + rgba[i * 4 + 1] + rgba[i * 4 + 2]) / 3.0;
        sum += v;
        sum2 += v * v;
    }
    mean = n ? sum / double(n) : 0;
    spread = n ? std::sqrt(std::max(0.0, sum2 / double(n) - mean * mean)) : 0;
}

static void Synthetic() {
    sif::video::UsmInfo info;
    std::vector<uint8_t> stream;
    const std::vector<uint8_t> junk(64, 0);
    Check(!sif::video::ParseUsm(junk, info, stream), "zeros are not a movie");
}

// Decodes every picture; checks the count against the header and that rewinding gives the same picture again
static void Full(const sif::Vfs& vfs, const char* path, int width, int height, double fps, int frames) {
    sif::video::Movie m;
    std::string error;
    const bool ok = m.Open(vfs, path, &error);
    Check(ok, std::string("open ") + path + ": " + error);
    if (!ok) return;
    const auto& i = m.Info();
    std::printf("%s: %d x %d, %.3f fps, %d frames\n", path, i.width, i.height, i.fps, i.frames);
    Check(i.width == width && i.height == height && std::abs(i.fps - fps) < 0.01 && i.frames == frames,
          std::string(path) + ": header size, rate and length");
    int decoded = 0;
    double low = 1e9, high = 0;
    for (int f = 0; f < i.frames; ++f) {
        const auto* p = m.Frame(f);
        if (!p) break;
        double mean, spread;
        Stats(*p, mean, spread);
        low = std::min(low, spread);
        high = std::max(high, spread);
        ++decoded;
    }
    std::printf("  decoded %d pictures, spread %.1f .. %.1f\n", decoded, low, high);
    Check(decoded == frames, std::string(path) + ": every picture decodes");
    Check(high > 10, std::string(path) + ": the pictures are not blank");
    const auto* a = m.Frame(3);
    const std::vector<uint8_t> third = a ? *a : std::vector<uint8_t>{};
    m.Frame(10);
    const auto* b = m.Frame(3);  // behind the current frame: rewinds
    Check(b && *b == third, std::string(path) + ": rewinding to frame 3 gives the same picture");
    Check(m.Frame(frames + 3) && *m.Frame(frames + 3) == third, std::string(path) + ": frame numbers wrap");
}

// all: decode every picture of every file (slow in Debug; run it in Release after decoder changes)
static void Real(const char* root, bool all) {
    sif::Vfs vfs;
    Check(vfs.Mount(sif::os::PathFromUtf8(root)), "mount");
    Full(vfs, "movie/monitor_001.usm", 512, 288, 30.0, 150);
    Full(vfs, "movie/music/preview_music_0010.usm", 256, 144, 60000.0 / 1001.0, 1623);
    Full(vfs, "movie/dress/preview_costume_001.usm", 256, 144, 60.0, 720);

    // every movie in the dump: header, stream and its first pictures
    const auto dir = sif::os::PathFromUtf8(root) / "movie";
    int files = 0, ok = 0;
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".usm") continue;
        ++files;
        const std::string rel = "movie/" + std::filesystem::relative(e.path(), dir).generic_string();
        sif::video::Movie m;
        std::string error;
        bool good = m.Open(vfs, rel, &error) && m.Frame(0) && m.Frame(1);
        for (int f = 2; good && all && f < m.Info().frames; ++f) {
            if (!m.Frame(f)) {
                good = false;
                error = "picture " + std::to_string(f) + " of " + std::to_string(m.Info().frames) + " does not decode";
            }
        }
        if (good) {
            ++ok;
        } else {
            std::printf("  %s: %s\n", rel.c_str(), error.c_str());
        }
    }
    std::printf("movies: %d of %d open and decode %s\n", ok, files, all ? "every picture" : "their first pictures");
    Check(files == 202 && ok == files, "all 202 movie files decode");
}

int main(int argc, char** argv) {
    Synthetic();
    if (argc > 1) Real(argv[1], argc > 2 && std::string(argv[2]) == "--all");
    std::printf("%s\n", failures ? "video_test FAILED" : "video_test ok");
    return failures ? 1 : 0;
}
