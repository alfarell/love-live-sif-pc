// The members (src/game/member/member.cpp) against the user's own dump: profile message ids, motions, rooms, costume
// packs and the navigator's idle table; the idle pick and wait follow the eboot (analysis 202611181256 §3).
#include <cstdio>
#include <string>
#include <vector>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/member/member.h"
#include "game/text/messages.h"

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
        std::printf("usage: sif_member_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");

    Check(member::Index(1) == 0 && member::Index(11) == 9 && member::Index(22) == 19 && member::Index(10) == -1,
          "member indexes 0-19");
    Check(member::Group(9) == 0 && member::Group(11) == 1 && member::Group(21) == 2 && member::Mode(22) == 1,
          "groups, and Saint Snow in Aqours mode");

    Messages en;
    std::string error;
    Check(en.Load(vfs, 1, &error), "English: " + error);
    const member::Profile umi = member::ProfileOf(4);  // the N2 capture of Umi's profile
    Check(en.Get(umi.name) == "Umi Sonoda" && en.Get(umi.short_name) == "Umi" && en.Get(umi.year) == "2nd year" &&
              en.Get(umi.birthday) == "March 15 (Pisces)" && en.Get(umi.blood) == "A" &&
              en.Get(umi.favourite) == "The Honoka Family's Manju" && en.Get(umi.hated) == "Carbonated beverages" &&
              en.Get(umi.unit) == "lily white",
          "Umi's profile as the game shows it");
    Check(en.Get(member::ProfileOf(22).name) == "Leah Kazuno" && en.Get(member::ProfileOf(22).year) == "1st year" &&
              en.Get(member::ProfileOf(11).unit) == "CYaRon!",
          "Leah and Chika");
    Check(en.Get(member::kSchoolYear) == "School year" && en.Get(member::kSubUnit) == "Sub-Unit" &&
              en.Get(member::kBloodType) == "Blood type" && en.Get(member::kHated) == "Hated food",
          "profile labels");

    for (const int m : member::kAll) {
        const std::string stand = member::MotionPath(vfs, m, member::kStandMotion);
        const std::string idle = member::MotionPath(vfs, m, 10);
        Check(!stand.empty() && stand.ends_with("_001_stand") && idle.ends_with("_010_idle"),
              "member " + std::to_string(m) + " has 001_stand and 010_idle: " + stand);
    }
    Check(member::MotionPath(vfs, 1, 518).ends_with("_518_dresschk") && member::MotionPath(vfs, 1, 999).empty(),
          "Honoka 518 dresschk; no 999");

    std::vector<uint8_t> file;
    Check(vfs.Read(member::CostumePack(11, 3, true) + "/mod_11_chika_003_school", file) ||
              vfs.Read(member::CostumePack(1, 3, true) + "/mod_01_honoka_003_school", file),
          "a special-colour pac: " + member::CostumePack(1, 3, true));
    Check(member::CostumePack(1, 3, false) == "chr/c01/d003/d003.pac" &&
              member::CostumePack(1, 25, false) == "chr/c01/d000/d000.pac",
          "costume pac paths (25 loads d000)");
    Check(member::RoomTexture(4) == "mainmanu_bg_pic00_00_04" && member::RoomTexture(22, 1) == "mainmanu_bg_pic00_01_22",
          "room texture names");
    std::vector<pac::ArcEntry> rooms;
    Check(vfs.List("ui2/10_top_menu.pac/uip0001_RoomBG00.pac/texture.pac", rooms), "list the rooms");
    for (const int m : member::kAll) {
        bool found = false;
        for (const pac::ArcEntry& e : rooms) found |= e.name.rfind(member::RoomTexture(m), 0) == 0;
        Check(found, "room of member " + std::to_string(m));
    }

    member::IdleTable idle;
    Check(member::LoadIdle(vfs, idle, &error), "idle table: " + error);
    Check(idle.idles.size() == 6 && idle.idles[0].motion == 10 && idle.idles[5].motion == 15 &&
              idle.idles[1].weight[0] == 100 && idle.idles[1].weight[1] == 0 && idle.idles[1].weight[2] == 100,
          "s_idle_animation_datas: 10-15 with the per-group weights");
    Check(idle.wait_min == 120 && idle.wait_range == 180 && idle.blend == 60, "s_common_param 120, 180, 60");
    // µ's: sum 600, k = r16 / 65536 x 601; k 0-100 -> 10 (k == 100 returns, as the eboot), 101-200 -> 11 ...
    Check(member::PickIdle(idle, 0, 0) == 10 && member::PickIdle(idle, 0, 65535) == 15, "µ's picks 10 .. 15");
    Check(member::PickIdle(idle, 1, 0) == 10 && member::PickIdle(idle, 1, 65535) == 10, "Aqours only 10");
    Check(member::PickIdle(idle, 2, 65535) == 11, "Saint Snow 10 or 11");
    Check(member::IdleWait(idle, 0) == 120 && member::IdleWait(idle, 65535) == 299, "wait 120 .. 299 frames");

    std::printf("%s\n", failures ? "member_test FAILED" : "member_test ok");
    return failures ? 1 : 0;
}
