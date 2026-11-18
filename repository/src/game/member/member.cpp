#include "game/member/member.h"

#include <cstdio>
#include <cstring>
#include <span>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"

namespace sif::member {
namespace {

// School year from each member's age message (31-50): 15 = 1st, 16 = 2nd, 17 = 3rd (true for all 20)
constexpr std::array<int, 20> kYear = {2, 3, 2, 2, 1, 1, 3, 1, 3, 2, 2, 3, 3, 2, 1, 1, 3, 1, 3, 1};
// The sub-unit message (24 Printemps, 25 BiBi, 26 lily white, 27 CYaRon!, 28 AZALEA, 29 Guilty Kiss, 22 Saint
// Snow). shortcut: the game's own member -> unit table is not traced; these are the units' published members (Umi's
// "lily white" checked in N2)
constexpr std::array<int, 20> kUnit = {24, 25, 24, 26, 26, 25, 26, 24, 25, 27, 29, 28, 28, 27, 29, 28, 29, 27, 22, 22};

int32_t I32(std::span<const uint8_t> d, size_t off) {
    int32_t v = 0;
    if (off + 4 <= d.size()) std::memcpy(&v, d.data() + off, 4);
    return v;
}

}

int Index(int member) {
    for (size_t i = 0; i < kAll.size(); ++i) {
        if (kAll[i] == member) return int(i);
    }
    return -1;
}

int Group(int member) { return member <= 9 ? 0 : member < 20 ? 1 : 2; }

int DefaultCostume(int group) { return group == 1 ? 105 : group == 2 ? 200 : 0; }

Profile ProfileOf(int member) {
    const int i = Index(member);
    if (i < 0) return {};
    return {197 + i, 217 + i, 257 + kYear[size_t(i)], 52 + i, 73 + i, 136 + i, 157 + i, kUnit[size_t(i)], 177 + i};
}

std::string CostumePack(int member, int costume, bool special) {
    const int d = costume == 25 ? 0 : costume;
    char s[64];
    std::snprintf(s, sizeof(s), special ? "chr/c%02d/d%03d/d%03d_v1.pac" : "chr/c%02d/d%03d/d%03d.pac", member, d, d);
    return s;
}

std::string RoomTexture(int member, int variant) {
    char s[48];
    std::snprintf(s, sizeof(s), "mainmanu_bg_pic00_%02d_%02d", variant, member);
    return s;
}

std::string MotionPath(const Vfs& vfs, int member, int number) {
    char folder[64], prefix[16], tag[16];
    std::snprintf(folder, sizeof(folder), "chr/c%02d/c%02d.pac/motion.pac", member, member);
    std::snprintf(prefix, sizeof(prefix), "mot_%02d_", member);
    std::snprintf(tag, sizeof(tag), "_%03d_", number);
    std::vector<pac::ArcEntry> entries;
    if (!vfs.List(folder, entries)) return {};
    for (const pac::ArcEntry& e : entries) {
        // mot_01_honoka_010_idle: the number follows the name; facial clips (mot_01_honoka_facial_...) never match
        if (e.name.rfind(prefix, 0) != 0) continue;
        const size_t at = e.name.find('_', std::strlen(prefix));
        if (at != std::string::npos && e.name.compare(at, std::strlen(tag), tag) == 0) return std::string(folder) + "/" + e.name;
    }
    return {};
}

bool LoadIdle(const Vfs& vfs, IdleTable& out, std::string* error) {
    out = {};
    constexpr const char* kPath = "common/common.pac/character_common.pac/character_params.bin";
    std::vector<uint8_t> file;
    std::vector<blbn::Symbol> symbols;
    if (!vfs.Read(kPath, file, error) || !blbn::Parse(file, symbols, error)) return false;
    const blbn::Symbol* idles = blbn::Find(symbols, "s_idle_animation_datas");
    const blbn::Symbol* common = blbn::Find(symbols, "s_common_param");
    if (!idles || !common) {
        if (error) *error = std::string(kPath) + ": no idle table";
        return false;
    }
    for (size_t o = 0; o + 16 <= idles->data.size(); o += 16) {
        out.idles.push_back({I32(idles->data, o), {I32(idles->data, o + 4), I32(idles->data, o + 8), I32(idles->data, o + 12)}});
    }
    out.wait_min = I32(common->data, 0), out.wait_range = I32(common->data, 4), out.blend = I32(common->data, 8);
    return true;
}

int PickIdle(const IdleTable& table, int group, uint32_t r16) {
    if (table.idles.empty() || group < 0 || group > 2) return 10;
    int sum = 0;
    for (const Idle& i : table.idles) sum += i.weight[size_t(group)];
    int k = int(float(r16) * (1.0f / 65536.0f) * float(sum + 1));
    for (const Idle& i : table.idles) {
        const int w = i.weight[size_t(group)];
        if (k - w == 0 || k < w) return i.motion;  // as FUN_0001dee0, including its "== 0" edge
        k -= w;
    }
    return 10;
}

int IdleWait(const IdleTable& table, uint32_t r16) {
    return table.wait_min + int(float(r16) * (1.0f / 65536.0f) * float(table.wait_range));
}

}
