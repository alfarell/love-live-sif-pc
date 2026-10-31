#pragma once
// CRI @UTF tables (ACB cue sheets, ACF): big-endian, column storage 0x10 none / 0x30 constant / 0x50 per row.
// Layout notes: tools/acb_dump.py, docs/formats/cri.md.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sif::cri {

struct UtfValue {
    enum class Kind { None, Int, Float, String, Data } kind = Kind::None;
    int64_t i = 0;
    double f = 0;
    std::string_view s;
    std::span<const uint8_t> d;
};

class UtfTable {
public:
    bool Parse(std::span<const uint8_t> data, std::string* error = nullptr);
    const std::string& Name() const { return name_; }
    size_t Rows() const { return rows_; }
    const UtfValue* Get(size_t row, std::string_view column) const;
    int64_t Int(size_t row, std::string_view column, int64_t fallback = -1) const;
    std::string_view Str(size_t row, std::string_view column) const;
    std::span<const uint8_t> Data(size_t row, std::string_view column) const;

private:
    std::string name_;
    std::vector<std::string> columns_;
    std::vector<UtfValue> values_;  // rows_ x columns_
    size_t rows_ = 0;
};

}
