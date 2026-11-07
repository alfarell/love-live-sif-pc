#include "game/dlc/dlc.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <system_error>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"

namespace sif::dlc {
namespace {

constexpr const char* kTitle = "CUSA24620";

std::string Str(const std::vector<uint8_t>& f, uint64_t at) {
    if (at >= f.size()) return {};
    const char* p = reinterpret_cast<const char*>(&f[size_t(at)]);
    return std::string(p, strnlen(p, f.size() - size_t(at)));
}

// Rows: +0x00 id, +0x08 u64 label prefix ("LLSIFACH"), +0x10 u64 label rest ("MMUS0001"), `field` the song id or
// costume number.
bool Table(const Vfs& vfs, const char* file, const char* symbol, size_t stride, size_t field,
           std::map<std::string, int>& out, std::string* error) {
    std::vector<uint8_t> f;
    std::vector<blbn::Symbol> symbols;
    if (!vfs.Read(std::string("common/common.pac/dlc.pac/") + file, f, error) || !blbn::Parse(f, symbols, error)) return false;
    const blbn::Symbol* s = blbn::Find(symbols, symbol);
    if (!s) {
        if (error) *error = std::string("no ") + symbol;
        return false;
    }
    for (size_t o = 0; o + stride <= s->data.size(); o += stride) {
        uint64_t a = 0, b = 0;
        int32_t value = 0;
        std::memcpy(&a, &s->data[o + 8], 8);
        std::memcpy(&b, &s->data[o + 16], 8);
        std::memcpy(&value, &s->data[o + field], 4);
        out[Str(f, a) + Str(f, b)] = value;
    }
    return true;
}

std::vector<uint8_t> ReadHead(const std::filesystem::path& p, size_t n) {
    std::ifstream in(p, std::ios::binary);
    std::vector<uint8_t> b(n);
    in.read(reinterpret_cast<char*>(b.data()), std::streamsize(n));
    b.resize(size_t(in.gcount()));
    return b;
}

// param.sfo: "\0PSF", u32 version, u32 key table, u32 data table, u32 count; 16-byte entries from 0x14
// (u16 key offset, u16 format, u32 length, u32 max length, u32 data offset).
std::string SfoContentId(const std::vector<uint8_t>& d) {
    if (d.size() < 20 || std::memcmp(d.data(), "\0PSF", 4) != 0) return {};
    uint32_t keys = 0, data = 0, count = 0;
    std::memcpy(&keys, &d[8], 4), std::memcpy(&data, &d[12], 4), std::memcpy(&count, &d[16], 4);
    for (uint32_t i = 0; i < count && 20 + 16 * size_t(i) + 16 <= d.size(); ++i) {
        const uint8_t* e = &d[20 + 16 * size_t(i)];
        uint16_t key = 0;
        uint32_t length = 0, at = 0;
        std::memcpy(&key, e, 2), std::memcpy(&length, e + 4, 4), std::memcpy(&at, e + 12, 4);
        if (Str(d, uint64_t(keys) + key) != "CONTENT_ID") continue;
        const size_t from = size_t(data) + at;
        if (from + length > d.size()) return {};
        return Str(d, from).substr(0, length);
    }
    return {};
}

void Add(const std::string& content_id, std::set<std::string>& out) {
    // "JP0082-CUSA24620_00-LLSIFACHMMUS0001": the label is the part after the second '-'
    const size_t dash = content_id.rfind('-');
    if (content_id.find(kTitle) != std::string::npos && dash != std::string::npos) out.insert(content_id.substr(dash + 1));
}

}

bool LoadCatalog(const Vfs& vfs, Catalog& out, std::string* error) {
    out = {};
    return Table(vfs, "dlc_music_data.bin", "ll3::dlc::s_dlc_music_datas", 0x60, 0x1C, out.songs, error) &&
           Table(vfs, "dlc_dress_data.bin", "ll3::dlc::s_dlc_dress_datas", 0x68, 0x20, out.costumes, error);
}

std::set<std::string> ScanOwned(const std::vector<std::filesystem::path>& folders) {
    std::set<std::string> owned;
    for (const auto& folder : folders) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            const auto& p = entry.path();
            if (entry.is_directory(ec)) {
                const auto sfo = p / "sce_sys" / "param.sfo";
                if (std::filesystem::exists(sfo, ec)) Add(SfoContentId(ReadHead(sfo, 64 * 1024)), owned);
            } else if (p.extension() == ".pkg") {
                const auto head = ReadHead(p, 0x64);  // "\x7FCNT", content id at 0x40 (36 characters)
                if (head.size() == 0x64 && std::memcmp(head.data(), "\x7F" "CNT", 4) == 0) {
                    Add(std::string(reinterpret_cast<const char*>(&head[0x40]), strnlen(reinterpret_cast<const char*>(&head[0x40]), 0x24)), owned);
                }
            }
        }
    }
    return owned;
}

Ownership Resolve(const Catalog& catalog, const std::set<std::string>& owned) {
    Ownership o;
    for (const auto& [label, song] : catalog.songs) {
        if (!owned.count(label)) o.locked_songs.insert(song);
    }
    for (const auto& [label, costume] : catalog.costumes) {
        if (!owned.count(label)) o.locked_costumes.insert(costume);
    }
    return o;
}

}
