#include "game/medley/medley.h"

#include <cstdio>
#include <cstring>
#include <span>

#include "engine/formats/blbn.h"
#include "engine/fs/vfs.h"

namespace sif::medley {
namespace {

constexpr size_t kRow = 0x120;

int32_t I32(std::span<const uint8_t> d, size_t off) {
    int32_t v = 0;
    if (off + 4 <= d.size()) std::memcpy(&v, d.data() + off, 4);
    return v;
}

// A string the row points at: a u64 offset into the whole file
std::string Text(const std::vector<uint8_t>& file, std::span<const uint8_t> row, size_t off) {
    uint64_t at = 0;
    if (off + 8 <= row.size()) std::memcpy(&at, row.data() + off, 8);
    std::string s;
    for (size_t i = size_t(at); at && i < file.size() && file[i]; ++i) s += char(file[i]);
    return s;
}

}

bool Load(const Vfs& vfs, Data& out, std::string* error) {
    out = {};
    const char* path = "common/common.pac/medley.pac/medley_data.bin";
    std::vector<uint8_t> file;
    std::vector<blbn::Symbol> symbols;
    const blbn::Symbol* s = nullptr;
    if (!vfs.Read(path, file, error) || !blbn::Parse(file, symbols, error) ||
        !(s = blbn::Find(symbols, "ll3::medley::s_medley_datas"))) {
        if (error && error->empty()) *error = std::string(path) + ": no s_medley_datas";
        return false;
    }
    const std::span<const uint8_t> t = s->data;
    for (size_t r = 0; r + kRow <= t.size(); r += kRow) {
        const std::span<const uint8_t> row = t.subspan(r, kRow);
        SetList l;
        l.id = I32(row, 0);
        for (size_t k = 0; k < 4; ++k) l.name[k] = Text(file, row, 8 + 8 * k);
        l.group = I32(row, 0x28);
        l.stage = I32(row, 0x2c);
        for (size_t i = 0; i < kEntries; ++i) {
            Entry e{I32(row, 0x30 + 4 * i), I32(row, 0x80 + 4 * i), int(row[0xd0 + 4 * i])};
            if (!e.song) break;
            l.entries.push_back(e);
        }
        out.lists.push_back(std::move(l));
    }
    return !out.lists.empty();
}

std::vector<const SetList*> OfGroup(const Data& data, int group) {
    std::vector<const SetList*> lists;
    for (const SetList& l : data.lists) {
        if (l.group == (group == 1 ? 1 : 0)) lists.push_back(&l);
    }
    return lists;
}

std::string StageFolder(int stage) {
    char s[16];
    std::snprintf(s, sizeof s, "st%03d", stage);
    return s;
}

}
