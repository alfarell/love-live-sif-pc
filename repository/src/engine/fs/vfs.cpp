#include "engine/fs/vfs.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#include "engine/platform/os.h"

namespace sif {
namespace {

namespace fs = std::filesystem;
using File = std::unique_ptr<FILE, int (*)(FILE*)>;

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

std::vector<std::string_view> Split(std::string_view path) {
    std::vector<std::string_view> parts;
    while (!path.empty()) {
        const size_t slash = path.find('/');
        if (slash != 0) parts.push_back(path.substr(0, slash));
        if (slash == std::string_view::npos) break;
        path.remove_prefix(slash + 1);
    }
    return parts;
}

bool ReadAt(FILE* f, uint64_t offset, size_t size, std::vector<uint8_t>& out) {
    out.resize(size);
    return os::SeekFile(f, int64_t(offset), SEEK_SET) == 0 && (size == 0 || std::fread(out.data(), 1, size, f) == size);
}

// Index of a top-level ARC table read from disk: only the header and records, not the entry data.
bool ReadTopIndex(FILE* f, std::vector<pac::ArcEntry>& entries, std::string* error) {
    std::vector<uint8_t> head;
    size_t want = 0x34;
    for (int pass = 0; pass < 3; ++pass) {
        if (!ReadAt(f, 0, want, head)) return Fail(error, "archive header truncated");
        const size_t need = pac::ArcIndexSize(head);
        if (need == 0 && head.size() >= 0x34 && std::memcmp(head.data(), "ARC\0", 4) == 0) break;  // empty table
        if (need == 0) return Fail(error, "not an ARC table");
        if (need <= head.size()) break;
        want = need;
    }
    return pac::ParseArc(head, entries, error);
}

// Follows `parts` through ARC tables held in memory, unpacking cmp wrappers on the way; only the final entry (and
// any nested cmp level) is copied, so reads from a cached package do not copy the whole package.
bool Descend(std::span<const uint8_t> data, std::span<const std::string_view> parts, std::vector<uint8_t>& out,
             std::string* error) {
    std::vector<uint8_t> owned;  // the innermost unpacked cmp level, when there is one
    const auto enter = [&](std::span<const uint8_t> d) {
        if (d.size() < 4 || std::memcmp(d.data(), "cmp\0", 4) != 0) {
            data = d;
            return true;
        }
        std::vector<uint8_t> packed(d.begin(), d.end());  // d may point into `owned`
        if (!pac::UnpackCmp(packed, error)) return false;
        owned = std::move(packed);
        data = owned;
        return true;
    };
    if (!enter(data)) return false;
    for (std::string_view part : parts) {
        std::vector<pac::ArcEntry> entries;
        if (!pac::ParseArc(data, entries, error)) return false;
        size_t i = 0;
        while (i < entries.size() && !pac::EntryMatches(entries[i], i, part)) ++i;
        if (i == entries.size()) return Fail(error, "no entry '" + std::string(part) + "'");
        if (entries[i].offset > data.size() || entries[i].size > data.size() - entries[i].offset) {
            return Fail(error, "entry '" + std::string(part) + "' outside its archive");
        }
        if (!enter(data.subspan(size_t(entries[i].offset), entries[i].size))) return false;
    }
    out.assign(data.begin(), data.end());
    return true;
}

}

bool Vfs::Mount(const fs::path& game_root) {
    std::error_code ec;
    if (!fs::is_directory(game_root, ec)) return false;
    root_ = game_root;
    return true;
}

bool Vfs::Read(std::string_view path, std::vector<uint8_t>& out, std::string* error) const {
    const auto parts = Split(path);
    fs::path disk = root_;
    size_t k = 0;
    std::error_code ec;
    for (; k < parts.size(); ++k) {
        disk /= os::PathFromUtf8(parts[k]);
        if (fs::is_regular_file(disk, ec)) break;
    }
    if (k == parts.size()) return Fail(error, "not found: " + std::string(path));
    File f(os::OpenFile(disk, "rb"), &std::fclose);
    if (!f) return Fail(error, "cannot open " + os::PathToUtf8(disk));
    const std::span<const std::string_view> rest(parts.begin() + ptrdiff_t(k) + 1, parts.end());
    const uint64_t file_size = fs::file_size(disk, ec);

    std::vector<uint8_t> magic;
    if (rest.empty() || !ReadAt(f.get(), 0, 4, magic) || std::memcmp(magic.data(), "ARC\0", 4) != 0) {
        // a plain file, or a cmp-wrapped archive: read it whole
        if (rest.empty()) {  // a plain file; a cmp\0 file is unpacked
            if (!ReadAt(f.get(), 0, size_t(file_size), out)) return Fail(error, "read failed: " + os::PathToUtf8(disk));
            return pac::UnpackCmp(out, error);
        }
        // entries of a cmp-wrapped archive: the unpacked archive may be cached
        std::shared_ptr<const std::vector<uint8_t>> data;
        {
            std::lock_guard lock(cache_->mutex);
            auto& c = cache_->entries;
            for (size_t i = 0; i < c.size(); ++i) {
                if (c[i].first != disk) continue;
                data = c[i].second;
                std::rotate(c.begin(), c.begin() + ptrdiff_t(i), c.begin() + ptrdiff_t(i) + 1);  // to the front
                break;
            }
        }
        if (!data) {
            std::vector<uint8_t> file;
            if (!ReadAt(f.get(), 0, size_t(file_size), file)) return Fail(error, "read failed: " + os::PathToUtf8(disk));
            if (!pac::UnpackCmp(file, error)) return false;
            data = std::make_shared<const std::vector<uint8_t>>(std::move(file));
            std::lock_guard lock(cache_->mutex);
            auto& c = cache_->entries;
            c.insert(c.begin(), {disk, data});
            if (c.size() > Cache::kSize) c.pop_back();
        }
        return Descend(*data, rest, out, error);
    }
    std::vector<pac::ArcEntry> entries;
    if (!ReadTopIndex(f.get(), entries, error)) return false;
    size_t i = 0;
    while (i < entries.size() && !pac::EntryMatches(entries[i], i, rest[0])) ++i;
    if (i == entries.size()) return Fail(error, "no entry '" + std::string(rest[0]) + "' in " + os::PathToUtf8(disk));
    std::vector<uint8_t> header;
    if (!ReadAt(f.get(), entries[i].block, 8, header)) return Fail(error, "entry header truncated");
    pac::ApplyBlockHeader(header.data(), entries[i]);
    if (entries[i].offset + entries[i].size > file_size) return Fail(error, "entry outside file");
    std::vector<uint8_t> entry;
    if (!ReadAt(f.get(), entries[i].offset, entries[i].size, entry)) return Fail(error, "entry read failed");
    if (rest.size() == 1) {
        out = std::move(entry);
        return pac::UnpackCmp(out, error);
    }
    return Descend(entry, rest.subspan(1), out, error);
}

bool Vfs::ReadRange(std::string_view path, uint64_t offset, size_t size, std::vector<uint8_t>& out,
                    std::string* error) const {
    const fs::path disk = root_ / os::PathFromUtf8(path);
    std::error_code ec;
    const uint64_t file_size = fs::file_size(disk, ec);
    if (ec) return Fail(error, "not found: " + std::string(path));
    if (offset > file_size || size > file_size - offset) return Fail(error, "range outside " + std::string(path));
    File f(os::OpenFile(disk, "rb"), &std::fclose);
    if (!f || !ReadAt(f.get(), offset, size, out)) return Fail(error, "read failed: " + std::string(path));
    return true;
}

bool Vfs::List(std::string_view path, std::vector<pac::ArcEntry>& out, std::string* error) const {
    const fs::path disk = root_ / os::PathFromUtf8(path);
    std::error_code ec;
    if (fs::is_regular_file(disk, ec)) {
        File f(os::OpenFile(disk, "rb"), &std::fclose);
        std::vector<uint8_t> magic;
        if (f && ReadAt(f.get(), 0, 4, magic) && std::memcmp(magic.data(), "ARC\0", 4) == 0) {
            if (!ReadTopIndex(f.get(), out, error)) return false;
            std::vector<uint8_t> header;
            for (auto& e : out) {
                if (ReadAt(f.get(), e.block, 8, header)) pac::ApplyBlockHeader(header.data(), e);
            }
            return true;
        }
    }
    std::vector<uint8_t> data;
    if (!Read(path, data, error)) return false;
    return pac::ParseArc(data, out, error);
}

bool Vfs::Exists(std::string_view path) const {
    std::vector<uint8_t> unused;
    std::error_code ec;
    if (fs::exists(root_ / os::PathFromUtf8(path), ec)) return true;
    return Read(path, unused);  // shortcut: reads the entry to test it; add an index-only lookup if this gets hot
}

bool IsSifDump(const fs::path& folder, std::string* why) {
    std::error_code ec;
    for (const char* need : {"sce_sys/param.sfo", "common/common.pac", "sound/sound.pac", "live"}) {
        if (!fs::exists(folder / need, ec)) return Fail(why, std::string("missing ") + need);
    }
    File f(os::OpenFile(folder / "sce_sys/param.sfo", "rb"), &std::fclose);
    std::vector<uint8_t> sfo;
    const uint64_t size = fs::file_size(folder / "sce_sys/param.sfo", ec);
    if (!f || size > (1 << 20) || !ReadAt(f.get(), 0, size_t(size), sfo)) return Fail(why, "cannot read param.sfo");
    const std::string_view text(reinterpret_cast<const char*>(sfo.data()), sfo.size());
    if (text.find("CUSA24620") == std::string_view::npos) return Fail(why, "param.sfo does not name CUSA24620");
    return true;
}

}
