// Unit test for the .pac reader and the Vfs. Synthetic checks always run; pass the CUSA24620 dump folder as the
// first argument to also read real entries (values from tools/pac_list.py, docs/formats/arc.md).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>

#include "engine/fs/pac.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"

static int failures = 0;
static void Check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", what);
    }
}

static void Put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + i] = uint8_t(v >> (8 * i));
}

static void Synthetic() {
    Check(sif::pac::NameHash("viewer_params.bin") == 0x95773AC2u, "name hash viewer_params.bin");
    Check(sif::pac::NameHash("mbc_0001_20_T") == 0xAC585A61u, "name hash mbc_0001_20_T");

    // one-entry ARC: header, offset table, a record at 0x40 named "a.bin", data block at 0x80 with 4 payload bytes
    std::vector<uint8_t> arc(0x80 + 0x80 + 4, 0);
    std::memcpy(arc.data(), "ARC\0\0\1\1\0\1\0", 10);
    Put32(arc, 0x30, 0x80);
    std::memcpy(&arc[0x40], "bin\0", 4);
    Put32(arc, 0x48, sif::pac::NameHash("a.bin"));
    arc[0x4E] = 6;
    std::memcpy(&arc[0x54], "a.bin", 6);
    Put32(arc, 0x80, 4);
    arc[0x84] = 0x80;
    std::memcpy(&arc[0x100], "DATA", 4);
    std::vector<sif::pac::ArcEntry> entries;
    Check(sif::pac::ParseArc(arc, entries) && entries.size() == 1, "parse synthetic ARC");
    Check(!entries.empty() && entries[0].name == "a.bin" && entries[0].offset == 0x100 && entries[0].size == 4,
          "synthetic entry fields");
    Check(!entries.empty() && sif::pac::EntryMatches(entries[0], 0, "#0") && sif::pac::EntryMatches(entries[0], 0, "a.bin"),
          "entry matching");

    // cmp\0zlib round trip
    const std::string text = "hello hello hello hello";
    std::vector<uint8_t> packed(compressBound(uLong(text.size())));
    uLongf packed_len = uLongf(packed.size());
    compress(packed.data(), &packed_len, reinterpret_cast<const Bytef*>(text.data()), uLong(text.size()));
    std::vector<uint8_t> cmp(16 + packed_len);
    std::memcpy(cmp.data(), "cmp\0zlib", 8);
    Put32(cmp, 8, uint32_t(packed_len));
    Put32(cmp, 12, uint32_t(text.size()));
    std::memcpy(&cmp[16], packed.data(), packed_len);
    Check(sif::pac::UnpackCmp(cmp) && std::string(cmp.begin(), cmp.end()) == text, "cmp zlib unpack");
}

static void Real(const char* root) {
    std::string why;
    Check(sif::IsSifDump(sif::os::PathFromUtf8(root), &why), ("dump detected: " + why).c_str());
    sif::Vfs vfs;
    Check(vfs.Mount(sif::os::PathFromUtf8(root)), "mount");
    struct Case { const char* path; const char* magic; size_t size; };
    const Case cases[] = {
        {"sandbox/viewer/viewer.pac/viewer_params.bin", "BLBN", 1976},
        {"ui/member_card/001/common.pac/mbc_0001_20_T", "btx", 129088},
        {"ui/member_card/001/common.pac/mbc_0001_20_T.tex", "btx", 129088},     // pac_extract.py naming
        {"ui/ui_badge.pac/texture.pac/badge_plate_001_0", "btx", 0},             // cmp-wrapped entry
        {"live/0010/0010.pac/score.pac/score_0010_00_bokurara_ch1.scd", "Scor", 3744},
        {"sound/sound.pac/music.acb", "@UTF", 279200},
        {"common/common.pac/#0", "", 0},                                         // top-level cmp\0lzma file
    };
    for (const Case& c : cases) {
        std::vector<uint8_t> data;
        std::string error;
        const bool ok = vfs.Read(c.path, data, &error);
        const bool magic = !*c.magic || (data.size() >= std::strlen(c.magic) && !std::memcmp(data.data(), c.magic, std::strlen(c.magic)));
        const bool size = !c.size || data.size() == c.size;
        if (!(ok && magic && size)) std::fprintf(stderr, "  %s: ok=%d size=%zu %s\n", c.path, ok, data.size(), error.c_str());
        Check(ok && magic && size, c.path);
    }
    std::vector<sif::pac::ArcEntry> list;
    Check(vfs.List("common/common.pac", list) && list.size() == 24, "list common.pac (lzma): 24 entries");
    Check(vfs.List("live/0010/0010.pac", list) && list.size() == 3, "list 0010.pac: 3 entries");
    std::printf("real dump: %zu cases checked\n", std::size(cases) + 2);
}

int main(int argc, char** argv) {
    Synthetic();
    if (argc > 1) Real(argv[1]);
    std::printf("%s\n", failures ? "vfs_test FAILED" : "vfs_test ok");
    return failures ? 1 : 0;
}
