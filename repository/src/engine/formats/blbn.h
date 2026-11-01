#pragma once
// SIF `BLBN` data tables (`.bin` entries, e.g. common/common.pac/live_common.pac/live_params.bin): "BLBN", 8 bytes,
// then SYM chunks (char[4] "SYM\0", u32 0x18, u32 flags, u64 data offset, u64 name offset, u32 name hash, u32 size)
// until the first non-SYM chunk. Names are C++ symbol names such as "s_EvaluateRangeDatas".

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sif::blbn {

struct Symbol {
    std::string name;
    std::span<const uint8_t> data;
};

bool Parse(std::span<const uint8_t> file, std::vector<Symbol>& out, std::string* error = nullptr);
const Symbol* Find(const std::vector<Symbol>& symbols, std::string_view name);

}
