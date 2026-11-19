#pragma once
// Friendship (M14): common/common.pac/game.pac/level_datas.bin and the gain after a live, as the eboot
// (analysis/202611190702-friendship-and-boosters/202611190720-analysis.md §1, checked in F2: BEGINNER, D = 197).

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace sif {

class Vfs;

namespace friendship {

struct Tables {
    std::vector<std::pair<int, int>> levels;  // s_level_datas: (level, total points), level 1 = 0 .. 200
    std::array<std::array<int, 7>, 9> gain{};  // s_experience_datas: [difficulty index 0-8][rank D .. SSS]
    std::vector<std::pair<int, int>> emblems;  // s_emblemrank_datas: (rank 1-11, Friendship level)
};
bool Load(const Vfs& vfs, Tables& out, std::string* error = nullptr);

// The level for a total (FUN_00078830: the highest row whose total <= points); 1 at least.
int LevelOf(const Tables& t, int points);
// The total a level needs (0 for level 1); the last level's for anything above.
int PointsFor(const Tables& t, int level);
// The emblem rank for a level (FUN_00078880): 1-11, 0 below the first.
int EmblemRank(const Tables& t, int level);
// The member's emblem picture (ui/ui_yellrank.pac): emblem_<member>_<rank>.
std::string EmblemPath(int member, int rank);

// The skill sums that change the gain (M11 kinds 18-21; percent values as card::SkillSum gives them).
struct GainSkills {
    int more = 0;      // kind 18 More Friendship Points
    int more_plus = 0; // kind 19 More Friendship Points +
    int expanded = 0;  // kind 20 Expanded Friendship Points
    int perfect = 0;   // kind 21 Perfect Points
};
// The points a played live gives its member (FUN_000a68d0): base[difficulty][rank]; each of kinds 18-20 adds
// base x sum / 100 when that is at least 1; kind 21 adds round(sum / 100 x perfects).
int Gain(const Tables& t, int difficulty, int rank, const GainSkills& skills = {}, int perfects = 0);

}
}
