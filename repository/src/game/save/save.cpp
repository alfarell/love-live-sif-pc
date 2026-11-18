#include "game/save/save.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <system_error>

namespace sif::save {
namespace {

constexpr const char* kHeader = "sif-port save 1";

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

}

bool Load(const std::filesystem::path& path, SaveData& out, std::string* error) {
    out = {};
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;  // first run
    std::ifstream f(path);
    std::string line;
    if (!f || !std::getline(f, line) || line.rfind(kHeader, 0) != 0) {
        return Fail(error, "not a port save (first line '" + line + "')");
    }
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream in(line);
        std::string kind;
        in >> kind;
        if (kind == "player") {
            std::getline(in >> std::ws, out.player);
        } else if (kind == "name_plate") {
            in >> out.name_plate;
        } else if (kind == "claimed") {
            for (int id; in >> id;) out.claimed.insert(id);
        } else if (kind == "own") {
            int type = 0, id = 0, count = 0;
            if (in >> type >> id >> count) {
                out.owned[{type, id}] = count;
            } else {
                out.unknown.push_back(line);
            }
        } else if (kind == "wear") {
            int member = 0, dress = 0, special = 0;
            if (in >> member >> dress) {
                out.wear[member] = dress;
                if (in >> special && special) out.special.insert(member);  // optional since M13
            } else {
                out.unknown.push_back(line);
            }
        } else if (kind == "skills") {
            int member = 0;
            std::array<int, 3> slots{};
            if (in >> member >> slots[0] >> slots[1] >> slots[2]) {
                out.skill_slots[member] = slots;
            } else {
                out.unknown.push_back(line);
            }
        } else if (kind == "photo") {
            int slot = 0, favourite = 0;
            Photo p;
            if (in >> slot >> p.time >> p.stage >> p.member_card >> p.live >> favourite) {
                p.favourite = favourite != 0;
                out.photos[slot] = p;
            } else {
                out.unknown.push_back(line);
            }
        } else if (kind == "profile_card") {
            ProfileCard c;
            int landscape = 0;
            if (in >> c.frame >> landscape >> c.crop >> c.live >> c.member_card >> c.accolade >> c.badge >> c.name_plate >>
                c.skill[0] >> c.skill_level[0] >> c.skill[1] >> c.skill_level[1] >> c.skill[2] >> c.skill_level[2]) {
                c.landscape = landscape != 0;
                out.card = c, out.has_card = true;
            } else {
                out.unknown.push_back(line);
            }
        } else if (kind == "mode") {
            in >> out.mode;
            if (out.mode != 1) out.mode = 0;
        } else if (kind == "navigator") {
            int mode = 0, member = 0;
            if (in >> mode >> member) {
                out.navigator[mode] = member;
            } else {
                out.unknown.push_back(line);
            }
        } else if (kind == "notes") {
            in >> out.perfects >> out.greats;
        } else if (kind == "costume") {
            int id = 0, plays = 0;
            if (in >> id >> plays) {
                out.costume_plays[id] = plays;
            } else {
                out.unknown.push_back(line);
            }
        } else if (kind == "accolade") {
            in >> out.accolade;
        } else if (kind == "badge") {
            in >> out.badge;
        } else if (kind == "record") {
            int song = 0, variant = 0;
            ChartRecord r;
            if (in >> song >> variant >> r.best_score >> r.best_combo >> r.best_rank >> r.best_rate >> r.plays >> r.clears >>
                r.full_combos) {
                out.records[{song, variant}] = r;
            } else {
                out.unknown.push_back(line);  // a damaged record is kept, not lost
            }
        } else if (!line.empty()) {
            out.unknown.push_back(line);
        }
    }
    return true;
}

bool Write(const std::filesystem::path& path, const SaveData& data, std::string* error) {
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        f << kHeader << "\n";
        f << "player " << data.player << "\n";
        if (data.accolade) f << "accolade " << data.accolade << "\n";
        if (data.badge) f << "badge " << data.badge << "\n";
        if (data.name_plate) f << "name_plate " << data.name_plate << "\n";
        if (!data.claimed.empty()) {
            f << "claimed";
            for (int id : data.claimed) f << " " << id;
            f << "\n";
        }
        for (const auto& [key, count] : data.owned) f << "own " << key.first << " " << key.second << " " << count << "\n";
        for (const auto& [member, dress] : data.wear) {
            f << "wear " << member << " " << dress << (data.special.contains(member) ? " 1" : "") << "\n";
        }
        if (data.mode) f << "mode " << data.mode << "\n";
        for (const auto& [mode, member] : data.navigator) f << "navigator " << mode << " " << member << "\n";
        for (const auto& [member, s] : data.skill_slots) {
            f << "skills " << member << " " << s[0] << " " << s[1] << " " << s[2] << "\n";
        }
        for (const auto& [slot, p] : data.photos) {
            f << "photo " << slot << " " << p.time << " " << p.stage << " " << p.member_card << " " << p.live << " "
              << (p.favourite ? 1 : 0) << "\n";
        }
        if (data.has_card) {
            const ProfileCard& c = data.card;
            f << "profile_card " << c.frame << " " << (c.landscape ? 1 : 0) << " " << c.crop << " " << c.live << " "
              << c.member_card << " " << c.accolade << " " << c.badge << " " << c.name_plate;
            for (int i = 0; i < 3; ++i) f << " " << c.skill[size_t(i)] << " " << c.skill_level[size_t(i)];
            f << "\n";
        }
        f << "notes " << data.perfects << " " << data.greats << "\n";
        for (const auto& [id, plays] : data.costume_plays) f << "costume " << id << " " << plays << "\n";
        for (const auto& [key, r] : data.records) {
            f << "record " << key.first << " " << key.second << " " << r.best_score << " " << r.best_combo << " "
              << r.best_rank << " " << r.best_rate << " " << r.plays << " " << r.clears << " " << r.full_combos << "\n";
        }
        for (const std::string& line : data.unknown) f << line << "\n";
        f.flush();
        if (!f) return Fail(error, "cannot write " + tmp.string());
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);  // replaces the old save in one step
    if (ec) return Fail(error, "cannot replace " + path.string() + ": " + ec.message());
    return true;
}

int Navigator(const SaveData& data, int mode) {
    const auto it = data.navigator.find(mode);
    if (it != data.navigator.end()) return it->second;
    return mode == 1 ? 11 : 1;  // a new game: Honoka and Chika (eboot FUN_00070840)
}

NewBests Record(SaveData& data, const Play& p) {
    ChartRecord& r = data.records[{p.song, p.variant}];
    const int rate = int(std::lround(p.rate * 10000));
    const NewBests n{p.score > r.best_score, p.combo > r.best_combo, rate > r.best_rate};
    ++r.plays;
    ++r.clears;  // shortcut: every finished live counts as a clear; the game decides by its life gauge, not built yet
    if (p.full_combo) ++r.full_combos;
    data.perfects += p.perfects, data.greats += p.greats;
    if (p.costume >= 0) ++data.costume_plays[p.costume];
    if (n.score) r.best_score = p.score;
    if (n.combo) r.best_combo = p.combo;
    if (n.rate) r.best_rate = rate;
    if (p.rank > r.best_rank) r.best_rank = p.rank;
    return n;
}

}
