// M8 V5: recognising the user's dump folder. Usage: sif_first_run_test [dump folder]
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/first_run.h"
#include "engine/platform/os.h"

namespace fs = std::filesystem;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// folders the user may pick by mistake, in a scratch folder of the system temp folder (removed afterwards)
static void Mistakes() {
    const fs::path root = fs::temp_directory_path() / "sif_first_run_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "with_pkg");
    fs::create_directories(root / "empty");
    std::ofstream(root / "with_pkg" / "game.pkg") << "not a real package";
    std::string hint;
    Check(sif::ResolveDumpFolder(root / "with_pkg", &hint).empty() && hint.find(".pkg") != std::string::npos,
          "a folder with only a .pkg is refused with a .pkg hint: " + hint);
    hint.clear();
    Check(sif::ResolveDumpFolder(root / "empty", &hint).empty() && hint.find("missing sce_sys") != std::string::npos,
          "an empty folder is refused and says what is missing: " + hint);
    fs::remove_all(root, ec);
}

static void Real(const char* root) {
    const fs::path dump = sif::os::PathFromUtf8(root);
    std::string hint;
    Check(sif::ResolveDumpFolder(dump, &hint) == dump, "the dump folder itself is accepted");
    if (dump.filename() == "CUSA24620") {
        Check(sif::ResolveDumpFolder(dump.parent_path(), &hint) == dump,
              "the folder above it is resolved to its CUSA24620 subfolder");
    }
}

int main(int argc, char** argv) {
    Mistakes();
    if (argc > 1) Real(argv[1]);
    std::printf("%s\n", failures ? "first_run_test FAILED" : "first_run_test ok");
    return failures ? 1 : 0;
}
