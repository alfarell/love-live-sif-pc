#pragma once
// The 20 members: groups, profile texts, model and motion files, rooms, and the navigator's idle table
// (analysis/202611172106-navigator-and-try-on/: N1 eboot trace, N2 shadPS4 run, N3).

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sif {

class Vfs;

namespace member {

// Member ids in the game's order (chr/cNN, message order): µ's 1-9, Aqours 11-19, Saint Snow 21-22.
constexpr std::array<int, 20> kAll = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22};

// 0-19 in kAll order (the eboot's table at 0x5E3690); -1 for anything else.
int Index(int member);
// 0 µ's, 1 Aqours, 2 Saint Snow (eboot FUN_0001e460).
int Group(int member);
// The L2 mode a member belongs to: 0 µ's, 1 Aqours (Saint Snow is chosen in Aqours mode, N2).
inline int Mode(int member) { return Group(member) == 0 ? 0 : 1; }
// A new save's member of each mode (eboot FUN_00070840): Honoka, Chika.
inline int DefaultOf(int mode) { return mode == 0 ? 1 : 11; }
// The group's costume when none is chosen (eboot table 0x5E3538): d000, d105, d200.
int DefaultCostume(int group);

// The message ids (message.pac) of a member's profile, as the R2 popup shows them.
struct Profile {
    int name = 0;        // "Honoka Kosaka" (197-216)
    int short_name = 0;  // "Honoka" (217-236)
    int year = 0;        // "1st year" .. "3rd year" (258-260)
    int birthday = 0, blood = 0, favourite = 0, hated = 0;
    int unit = 0;        // "Printemps" .. "Guilty Kiss" (24-29), "Saint Snow" (22)
    int bio = 0;         // the long description (177-196)
};
// All zero for an unknown member.
Profile ProfileOf(int member);
constexpr int kSchoolYear = 257, kBirthday = 51, kBloodType = 72, kFavourite = 135, kHated = 156, kSubUnit = 514;

// The costume pac: chr/cNN/dXXX/dXXX.pac, or dXXX_v1.pac for the special colour (eboot FUN_0001bda0; costume 25 is
// loaded as d000).
std::string CostumePack(int member, int costume, bool special);
// The member's room on the top menu (ui2/10_top_menu uip0001_RoomBG00): mainmanu_bg_pic00_<variant>_<member>.
// variant 0 or 1 (Hypothesis: the eboot's room_bg000 / room_bg001 layers).
std::string RoomTexture(int member, int variant = 0);
// The member's menu motion number `number` (mot_NN_<name>_<number>_*, e.g. 1 = 001_stand, 10 = 010_idle) in
// chr/cNN/cNN.pac/motion.pac; "" when there is none.
std::string MotionPath(const Vfs& vfs, int member, int number);
// The member's facial clip `number` (mot_NN_<name>_facial_<number>_*, e.g. 1 = 001_a, 100 = 100_blink; the eboot's
// facial ids are 1000 + number, M18 U1); "" when there is none.
std::string FacialPath(const Vfs& vfs, int member, int number);

// character_params.bin: the navigator's idle loop (eboot FUN_00015970, FUN_00015ae0, FUN_0001dee0).
struct Idle {
    int motion = 0;
    std::array<int, 3> weight{};  // per group
};
struct IdleTable {
    std::vector<Idle> idles;                      // s_idle_animation_datas
    int wait_min = 120, wait_range = 180, blend = 60;  // s_common_param fields 0-2 (frames at 60 fps)
};
bool LoadIdle(const Vfs& vfs, IdleTable& out, std::string* error = nullptr);
// The idle motion for `group` from one random draw `r16` (xorshift128 >> 16), weighted as the eboot does; 10 when the
// table is empty or nothing matches.
int PickIdle(const IdleTable& table, int group, uint32_t r16);
// Frames to stand before the next idle: wait_min + r16 / 65536 x wait_range.
int IdleWait(const IdleTable& table, uint32_t r16);
constexpr int kStandMotion = 1;  // 001_stand, played between idles

}
}
