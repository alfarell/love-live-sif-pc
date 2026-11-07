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
