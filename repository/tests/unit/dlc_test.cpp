// Add-on ownership (src/game/dlc/dlc.cpp): the catalog against the user's own dump; the add-on scan against a fake
// add-on folder and .pkg header written to a temporary folder (no real add-on needed); optionally the user's folder.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/dlc/dlc.h"

using namespace sif;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static void WriteFile(const std::filesystem::path& p, const std::string& bytes) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write(bytes.data(), std::streamsize(bytes.size()));
}

// A minimal param.sfo with one CONTENT_ID entry.
static std::string Sfo(const std::string& id) {
    const std::string key = std::string("CONTENT_ID") + '\0' + '\0';  // padded to 12
    std::string d(20 + 16, '\0');
    auto put = [&](size_t at, uint32_t v) { std::memcpy(&d[at], &v, 4); };
    put(0, 0x46535000), put(4, 0x101), put(8, 20 + 16), put(12, uint32_t(20 + 16 + key.size())), put(16, 1);
    const uint32_t length = uint32_t(id.size() + 1), fmt_key = 0x0204u << 16;
    put(20, fmt_key), put(24, length), put(28, 48), put(32, 0);
    return d + key + id + std::string(48 - id.size(), '\0');
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sif_dlc_test <CUSA24620 dump> [add-on folder]\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    dlc::Catalog catalog;
    std::string error;
    Check(dlc::LoadCatalog(vfs, catalog, &error), "catalog: " + error);
    Check(catalog.songs.size() == 117 && catalog.costumes.size() == 63, "117 song and 63 costume add-ons");
    Check(catalog.songs.count("LLSIFACHMMUS0001") && catalog.songs.at("LLSIFACHMMUS0001") == 10, "MUS0001 = song 0010");
    Check(catalog.songs.count("LLSIFACHMMUS1049") && catalog.songs.at("LLSIFACHMMUS1049") == 1550, "MUS1049 = song 1550");
    Check(catalog.costumes.count("LLSIFACHMCOS0001") && catalog.costumes.at("LLSIFACHMCOS0001") == 31, "COS0001 = d031");

    const auto tmp = std::filesystem::temp_directory_path() / "sif_dlc_test";
    std::filesystem::remove_all(tmp);
    WriteFile(tmp / "a" / "sce_sys" / "param.sfo", Sfo("JP0082-CUSA24620_00-LLSIFACHMMUS0001"));
    WriteFile(tmp / "b" / "sce_sys" / "param.sfo", Sfo("JP0082-CUSA99999_00-LLSIFACHMMUS0003"));  // another title
    std::string pkg = std::string("\x7F" "CNT", 4) + std::string(0x3C, '\0') + "JP0082-CUSA24620_00-LLSIFACHMCOS0001";
    WriteFile(tmp / "c.pkg", pkg + std::string(0x100, '\0'));
    const auto owned = dlc::ScanOwned({tmp});
    std::filesystem::remove_all(tmp);
    Check(owned == std::set<std::string>{"LLSIFACHMMUS0001", "LLSIFACHMCOS0001"},
          "scan: the extracted add-on and the .pkg, not the other title's");
    const auto own = dlc::Resolve(catalog, owned);
    Check(own.Song(10) && !own.Song(20) && own.Song(120), "song 0010 owned, 0020 locked, 0120 base game");
    Check(own.Costume(31) && !own.Costume(4) && own.Costume(0), "d031 owned, d004 locked, d000 base game");
    Check(own.locked_songs.size() == 116, "116 songs locked");
    if (argc > 2) {  // the user's own add-ons
        const auto mine = dlc::ScanOwned({os::PathFromUtf8(argv[2])});
        std::printf("add-ons in %s: %zu\n", argv[2], mine.size());
    }
    std::printf("%s\n", failures ? "dlc_test FAILED" : "dlc_test ok");
    return failures ? 1 : 0;
}
