#include "engine/audio/cri_utf.h"

#include <cstring>

namespace sif::cri {
namespace {

uint64_t Be(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = v << 8 | p[i];
    return v;
}

bool Fail(std::string* error, const char* text) {
    if (error) *error = text;
    return false;
}

}

bool UtfTable::Parse(std::span<const uint8_t> data, std::string* error) {
    *this = {};
    if (data.size() < 0x20 || std::memcmp(data.data(), "@UTF", 4) != 0) return Fail(error, "not an @UTF table");
    const uint64_t size = Be(&data[4], 4);
    if (8 + size > data.size()) return Fail(error, "@UTF table truncated");
    const uint8_t* t = data.data() + 8;  // offsets below are relative to here
    const uint32_t rows_off = uint32_t(Be(t + 2, 2)), str_off = uint32_t(Be(t + 4, 4)), data_off = uint32_t(Be(t + 8, 4));
    const uint32_t name = uint32_t(Be(t + 12, 4));
    const uint16_t ncol = uint16_t(Be(t + 16, 2)), row_width = uint16_t(Be(t + 18, 2));
    rows_ = size_t(Be(t + 20, 4));
    const auto cstr = [&](uint64_t o) -> std::string_view {
        const uint64_t at = str_off + o;
        if (at >= size) return {};
        const char* s = reinterpret_cast<const char*>(t + at);
        return {s, strnlen(s, size_t(size - at))};
    };
    bool ok = true;
    // reads one value of `kind` at t + pos, returns its byte size (0 if unknown)
    const auto value = [&](uint8_t kind, uint64_t pos, UtfValue& v) -> int {
        static const int kSize[] = {1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 4, 8};
        if (kind > 0xB || pos + kSize[kind] > size) { ok = false; return 0; }
        const uint8_t* p = t + pos;
        switch (kind) {
            case 0: case 2: case 4: case 6: v.kind = UtfValue::Kind::Int; v.i = int64_t(Be(p, kSize[kind])); break;
            case 1: v.kind = UtfValue::Kind::Int; v.i = int8_t(p[0]); break;
            case 3: v.kind = UtfValue::Kind::Int; v.i = int16_t(Be(p, 2)); break;
            case 5: v.kind = UtfValue::Kind::Int; v.i = int32_t(Be(p, 4)); break;
            case 7: v.kind = UtfValue::Kind::Int; v.i = int64_t(Be(p, 8)); break;
            case 8: { uint32_t u = uint32_t(Be(p, 4)); float f; std::memcpy(&f, &u, 4); v.kind = UtfValue::Kind::Float; v.f = f; break; }
            case 9: { uint64_t u = Be(p, 8); double f; std::memcpy(&f, &u, 8); v.kind = UtfValue::Kind::Float; v.f = f; break; }
            case 0xA: v.kind = UtfValue::Kind::String; v.s = cstr(Be(p, 4)); break;
            case 0xB: {
                const uint64_t o = data_off + Be(p, 4), n = Be(p + 4, 4);
                if (o + n > size) { ok = false; break; }
                v.kind = UtfValue::Kind::Data;
                v.d = {t + o, size_t(n)};
                break;
            }
        }
        return kSize[kind];
    };
    struct Column { uint8_t flags; UtfValue constant; };
    std::vector<Column> cols;
    uint64_t pos = 0x18;
    for (uint16_t c = 0; c < ncol && ok; ++c) {
        if (pos + 5 > size) return Fail(error, "@UTF columns truncated");
        Column col{t[pos], {}};
        columns_.emplace_back(cstr(Be(t + pos + 1, 4)));
        pos += 5;
        if ((col.flags & 0xF0) == 0x30) pos += value(col.flags & 0x0F, pos, col.constant);
        cols.push_back(col);
    }
    values_.resize(rows_ * ncol);
    for (size_t r = 0; r < rows_ && ok; ++r) {
        uint64_t p = rows_off + uint64_t(r) * row_width;
        for (size_t c = 0; c < ncol; ++c) {
            if ((cols[c].flags & 0xF0) == 0x50) p += value(cols[c].flags & 0x0F, p, values_[r * ncol + c]);
            else if ((cols[c].flags & 0xF0) == 0x30) values_[r * ncol + c] = cols[c].constant;
        }
    }
    if (!ok) return Fail(error, "@UTF value out of range");
    name_ = std::string(cstr(name));
    return true;
}

const UtfValue* UtfTable::Get(size_t row, std::string_view column) const {
    if (row >= rows_) return nullptr;
    for (size_t c = 0; c < columns_.size(); ++c) {
        if (columns_[c] == column) return &values_[row * columns_.size() + c];
    }
    return nullptr;
}

int64_t UtfTable::Int(size_t row, std::string_view column, int64_t fallback) const {
    const UtfValue* v = Get(row, column);
    return v && v->kind == UtfValue::Kind::Int ? v->i : fallback;
}

std::string_view UtfTable::Str(size_t row, std::string_view column) const {
    const UtfValue* v = Get(row, column);
    return v && v->kind == UtfValue::Kind::String ? v->s : std::string_view{};
}

std::span<const uint8_t> UtfTable::Data(size_t row, std::string_view column) const {
    const UtfValue* v = Get(row, column);
    return v && v->kind == UtfValue::Kind::Data ? v->d : std::span<const uint8_t>{};
}

}
