#pragma once
// Add-on (DLC) ownership: which songs and costumes the user's own installed add-ons unlock. The add-ons carry no
// game data (only sce_sys: param.sfo, license); the content is in the base dump, and the game's tables
// (common/common.pac/dlc.pac) name the add-on label that unlocks each item. See docs/formats/dlc.md.

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace sif {

class Vfs;

namespace dlc {

// label ("LLSIFACHMMUS0001") -> song id (s_dlc_music_datas, 117 rows) or costume number (s_dlc_dress_datas, 63 rows).
struct Catalog {
    std::map<std::string, int> songs, costumes;
};
bool LoadCatalog(const Vfs& vfs, Catalog& out, std::string* error = nullptr);

// The add-on labels found in `folders`: each subfolder with sce_sys/param.sfo (an extracted add-on) and each .pkg
// file (its header), read from the CONTENT_ID "JP0082-CUSA24620_00-<label>". Only CUSA24620 add-ons count.
std::set<std::string> ScanOwned(const std::vector<std::filesystem::path>& folders);

struct Ownership {
    std::set<int> locked_songs, locked_costumes;  // in the catalog, with no add-on found
    std::set<int> addon_costumes;                 // in the catalog, with the add-on found (the add-on gives it)
    bool Song(int id) const { return !locked_songs.count(id); }
    bool Costume(int id) const { return !locked_costumes.count(id); }
};
// Items the catalog does not list are base game, always owned.
Ownership Resolve(const Catalog& catalog, const std::set<std::string>& owned);

}
}
