#pragma once
// Read-only view of the user's SIF dump. Paths use '/' and may go into archives:
//   "sound/music.awb"                                  plain file
//   "live/0010/0010.pac/score.pac/score_0010_00_bokurara_ch1.scd"   entry inside nested ARC tables
// Entry components match the entry name, the name plus ".<tag>" (as pac_extract.py writes it), or "#<index>".
// cmp\0 wrappers are unpacked transparently. Thread-safe: every Read opens its own file handle.

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "engine/fs/pac.h"

namespace sif {

class Vfs {
public:
    bool Mount(const std::filesystem::path& game_root);
    const std::filesystem::path& Root() const { return root_; }

    bool Read(std::string_view path, std::vector<uint8_t>& out, std::string* error = nullptr) const;
    // `size` bytes at `offset` of a plain file (no archive path, no unpacking), e.g. one wave of a large .awb bank.
    bool ReadRange(std::string_view path, uint64_t offset, size_t size, std::vector<uint8_t>& out,
                   std::string* error = nullptr) const;
    // Entries of the ARC table at `path` (a .pac file or an archive entry inside one).
    bool List(std::string_view path, std::vector<pac::ArcEntry>& out, std::string* error = nullptr) const;
    bool Exists(std::string_view path) const;

private:
    std::filesystem::path root_;
};

// True if `folder` looks like the user's CUSA24620 dump: sce_sys/param.sfo names CUSA24620 and the core archives
// exist. `why` explains a failure.
bool IsSifDump(const std::filesystem::path& folder, std::string* why = nullptr);

}
