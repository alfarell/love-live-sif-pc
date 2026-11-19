#pragma once
// Birthday events and the daily login bonus (M14; analysis/202611190702-friendship-and-boosters/202611190720-analysis.md
// §3-4, checked in F2: the Ongoing Events dates and the booster count).

#include <array>
#include <string>
#include <vector>

namespace sif {

class Vfs;

namespace event {

// A calendar date (the in-game clock is the system clock).
struct Date {
    int year = 0, month = 0, day = 0;
    bool operator==(const Date&) const = default;
    auto operator<=>(const Date&) const = default;
};
Date Today();
Date AddDays(Date d, int days);
// "YYYY-MM-DD" -> date; false if not a date
bool ParseDate(const std::string& text, Date& out);

// One member's birthday event: s_bd_event_datas (by member index) and her birthday (s_birthdayParams).
struct Birthday {
    int member = 0;
    int month = 0, day = 0;
    int goal = 1000;
    std::vector<int> skill_cards;  // fields 2-3 (Leah and Sarah have one)
    std::vector<int> items;        // fields 4-9: the group's key items
};
struct Data {
    std::vector<Birthday> events;  // by member index 0-19
    std::vector<int> points;       // s_bdEventPointDatas: 19 values
};
bool Load(const Vfs& vfs, Data& out, std::string* error = nullptr);
const Birthday* Find(const Data& data, int member);

// The event window that holds `today`, if any: from 7 days before her birthday (0:00) to her birthday (23:59). A
// birthday early in January belongs to the window starting the December before. `start`/`end` get the dates.
bool Active(const Birthday& b, Date today, Date* start = nullptr, Date* end = nullptr);
// The window of the event in `year` (the year of her birthday).
void Window(const Birthday& b, int year, Date& start, Date& end);
// Points for a played live with her during her event (FUN_0000f850): points[2] + points[3 + d] + points[12 + rank],
// d = variant 0-4 (arcade) or 5-8 (home variants 10-13).
int LivePoints(const Data& data, int variant, int rank);

// The daily login bonus (eboot FUN_001b94c0, table 0x5F4350): 12 days; 0 = one Wai-Wai! Booster, else 10 of that key
// item, by mode (0 µ's, 1 Aqours and Saint Snow).
constexpr std::array<std::array<int, 2>, 12> kLoginBonus = {{
    {0, 0}, {29, 55}, {0, 0}, {30, 56}, {0, 0}, {31, 57}, {0, 0}, {32, 58}, {0, 0}, {33, 59}, {0, 0}, {34, 60}}};
constexpr int kLoginItemAmount = 10, kMaxBoosters = 9, kBoosterUnits = 10;

}
}
