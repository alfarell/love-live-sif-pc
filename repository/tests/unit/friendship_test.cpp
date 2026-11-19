// Friendship and birthday events (src/game/friendship, src/game/event) against the user's own dump: the tables, the
// gain checked in the game (F2: BEGINNER, rank D = 197, level 2 at 250), the event windows seen on Ongoing Events and
// the live points.
#include <cstdio>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/event/event.h"
#include "game/friendship/friendship.h"

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
        std::printf("usage: sif_friendship_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    std::string error;

    friendship::Tables t;
    Check(friendship::Load(vfs, t, &error), "level_datas: " + error);
    Check(t.levels.size() == 200 && t.levels[1].second == 250 && t.emblems.size() == 11, "200 levels, 11 emblem ranks");
    Check(friendship::Gain(t, 5, 0) == 197, "BEGINNER (index 5), rank D: 197, as the game's Items Obtained");
    Check(friendship::Gain(t, 0, 6) == 335 && friendship::Gain(t, 8, 6) == 801, "the table's corners");
    Check(friendship::LevelOf(t, 0) == 1 && friendship::LevelOf(t, 197) == 1 && friendship::LevelOf(t, 250) == 2 &&
              friendship::LevelOf(t, 2199999) == 199 && friendship::LevelOf(t, 2200000) == 200,
          "levels from totals");
    Check(friendship::PointsFor(t, 2) == 250 && friendship::PointsFor(t, 1) == 0, "totals for levels");
    Check(friendship::EmblemRank(t, 1) == 1 && friendship::EmblemRank(t, 19) == 1 && friendship::EmblemRank(t, 20) == 2 &&
              friendship::EmblemRank(t, 200) == 11,
          "emblem ranks by level (1, 20, 30 ... 200)");
    friendship::GainSkills s;
    s.more = 10;  // 197 x 10 / 100 = 19
    Check(friendship::Gain(t, 5, 0, s) == 216, "More Friendship Points 10%");
    s = {};
    s.perfect = 50;  // ceil(0.5 x 3) = 2
    Check(friendship::Gain(t, 5, 0, s, 3) == 199, "Perfect Points: rounded up");
    Check(friendship::EmblemPath(11, 3) == "ui/ui_yellrank.pac/texture.pac/emblem_11_3" &&
              vfs.Exists(friendship::EmblemPath(11, 3)),
          "an emblem picture");

    event::Data e;
    Check(event::Load(vfs, e, &error), "bd_event: " + error);
    const event::Birthday* chika = event::Find(e, 11);
    Check(chika && chika->month == 8 && chika->day == 1 && chika->goal == 1000 && chika->skill_cards.size() == 2 &&
              chika->items.size() == 6,
          "Chika: Aug 1, 1000 points, two skill cards, six key items");
    event::Date start, end;
    Check(chika && event::Active(*chika, {2026, 7, 25}, &start, &end) && start == event::Date{2026, 7, 25} &&
              end == event::Date{2026, 8, 1} && !event::Active(*chika, {2026, 7, 24}) && !event::Active(*chika, {2026, 8, 2}),
          "Chika's window: Jul 25th 0:00 to Aug 1st 23:59 (the game's Ongoing Events)");
    const event::Birthday* dia = event::Find(e, 14);
    Check(dia && event::Active(*dia, {2026, 12, 25}, &start, &end) && end == event::Date{2027, 1, 1} &&
              event::Active(*dia, {2027, 1, 1}),
          "Dia: Dec 25th to Jan 1st, across the year");
    Check(!event::Active(*chika, {2026, 11, 19}), "no event on 2026-11-19 (F2)");
    Check(event::LivePoints(e, 10, 0) == 0 + 5 + 2 && event::LivePoints(e, 4, 6) == 12 + 8 && event::LivePoints(e, 13, 3) == 12 + 5,
          "live points: difficulty + rank");
    event::Date d;
    Check(event::ParseDate("2026-08-01", d) && d == event::Date{2026, 8, 1} && !event::ParseDate("2026-02-30", d),
          "date text");

    std::printf("%s\n", failures ? "friendship_test FAILED" : "friendship_test ok");
    return failures ? 1 : 0;
}
