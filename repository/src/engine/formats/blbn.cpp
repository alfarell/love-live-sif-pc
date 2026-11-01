#include "engine/formats/blbn.h"

#include <cstring>

namespace sif::blbn {
namespace {

uint64_t Le(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; --i) v = v << 8 | p[i];
    return v;
}

}

bool Parse(std::span<const uint8_t> f, std::vector<Symbol>& out, std::string* error) {
    out.clear();
    if (f.size() < 12 || std::memcmp(f.data(), "BLBN", 4) != 0) {
        if (error) *error = "not a BLBN table";
        return false;
    }
    for (size_t pos = 12; pos + 36 <= f.size() && std::memcmp(&f[pos], "SYM\0", 4) == 0;) {
        const uint64_t size = Le(&f[pos + 4], 4);
        const uint64_t data = Le(&f[pos + 12], 8), name = Le(&f[pos + 20], 8), data_size = Le(&f[pos + 32], 4);
        if (data + data_size > f.size() || name >= f.size()) {
            if (error) *error = "BLBN symbol out of range";
            return false;
        }
        const char* text = reinterpret_cast<const char*>(&f[name]);
        out.push_back({std::string(text, strnlen(text, f.size() - name)), f.subspan(size_t(data), size_t(data_size))});
        pos += 12 + size;
    }
    return true;
}

const Symbol* Find(const std::vector<Symbol>& symbols, std::string_view name) {
    for (const Symbol& s : symbols) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

}
