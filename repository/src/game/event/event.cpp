#include "game/event/event.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <span>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"
#include "game/member/member.h"

namespace sif::event {
namespace {

int32_t I32(std::span<const uint8_t> d, size_t off) {
    int32_t v = 0;
    if (off + 4 <= d.size()) std::memcpy(&v, d.data() + off, 4);
    return v;
}

bool Table(const Vfs& vfs, const char* path, const char* symbol, std::vector<uint8_t>& file, std::span<const uint8_t>& data,
           std::string* error) {
    std::vector<blbn::Symbol> symbols;
    const blbn::Symbol* s = nullptr;
    if (!vfs.Read(path, file, error) || !blbn::Parse(file, symbols, error) || !(s = blbn::Find(symbols, symbol))) {
        if (error && error->empty()) *error = std::string(path) + ": no " + symbol;
        return false;
    }
    data = s->data;
    return true;
}

}

Date g_today{};  // set by SetToday; year 0 = the system clock

void SetToday(Date d) { g_today = d; }

Date Today() {
    if (g_today.year) return g_today;
    const std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &t);
    return {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

Date AddDays(Date d, int days) {
    using namespace std::chrono;
    const sys_days s = sys_days{year{d.year} / month{unsigned(d.month)} / day{unsigned(d.day)}} + std::chrono::days{days};
    const year_month_day y{s};
    return {int(y.year()), int(unsigned(y.month())), int(unsigned(y.day()))};
}

bool ParseDate(const std::string& text, Date& out) {
    Date d;
    if (std::sscanf(text.c_str(), "%d-%d-%d", &d.year, &d.month, &d.day) != 3) return false;
    using namespace std::chrono;
    if (!year_month_day{year{d.year} / month{unsigned(d.month)} / day{unsigned(d.day)}}.ok()) return false;
    out = d;
    return true;
}

bool Load(const Vfs& vfs, Data& out, std::string* error) {
    out = {};
    std::vector<uint8_t> file;
    std::span<const uint8_t> t;
    if (!Table(vfs, "common/common.pac/character_common.pac/character_params.bin", "s_birthdayParams", file, t, error)) {
        return false;
    }
    const std::vector<uint8_t> dates(t.begin(), t.end());  // (month, day) by member id 0-22
    if (!Table(vfs, "common/common.pac/bd_event.pac/bd_event_data.bin", "ll3::bd_event::s_bd_event_datas", file, t, error)) {
        return false;
    }
    for (size_t r = 0; r + 64 <= t.size(); r += 64) {
        const int index = I32(t, r);
        if (index < 0 || index >= int(member::kAll.size())) continue;
        Birthday b;
        b.member = member::kAll[size_t(index)];
        if (size_t(b.member) * 2 + 1 < dates.size()) b.month = dates[size_t(b.member) * 2], b.day = dates[size_t(b.member) * 2 + 1];
        b.goal = I32(t, r + 4);
        for (size_t k = 2; k <= 3; ++k) {
            if (const int c = I32(t, r + 4 * k)) b.skill_cards.push_back(c);
        }
        for (size_t k = 4; k <= 9; ++k) {
            if (const int i = I32(t, r + 4 * k)) b.items.push_back(i);
        }
        out.events.push_back(std::move(b));
    }
    if (!Table(vfs, "common/common.pac/bd_event.pac/bd_event_point_data.bin", "ll3::bd_event::s_bdEventPointDatas", file, t, error)) {
        return false;
    }
    for (size_t o = 0; o + 2 <= t.size(); o += 2) out.points.push_back(t[o] | t[o + 1] << 8);
    return out.events.size() == 20;
}

const Birthday* Find(const Data& data, int member) {
    for (const Birthday& b : data.events) {
        if (b.member == member) return &b;
    }
    return nullptr;
}

void Window(const Birthday& b, int year, Date& start, Date& end) {
    end = {year, b.month, b.day};
    start = AddDays(end, -7);
}

bool Active(const Birthday& b, Date today, Date* start, Date* end) {
    if (!b.month) return false;
    for (const int year : {today.year, today.year + 1}) {  // a birthday on Jan 1-7 starts in the December before
        Date s, e;
        Window(b, year, s, e);
        if (s <= today && today <= e) {
            if (start) *start = s;
            if (end) *end = e;
            return true;
        }
    }
    return false;
}

int EventYear(const Birthday& b, Date today) {
    Date start, end;
    return Active(b, today, &start, &end) ? end.year : 0;
}

int LivePoints(const Data& data, int variant, int rank) {
    const int d = variant >= 10 ? variant - 5 : variant;  // the eboot: v % 1000 < 10 ? v : v - 5
    const auto at = [&](int i) { return i >= 0 && i < int(data.points.size()) ? data.points[size_t(i)] : 0; };
    return at(2) + at(3 + d) + at(12 + rank);
}

}
