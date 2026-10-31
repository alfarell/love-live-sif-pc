#include "engine/fs/pac.h"

#include <cstring>

#include <zlib.h>

extern "C" {
#include "LzmaDec.h"
}

namespace sif::pac {
namespace {

uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

void* LzmaAlloc(ISzAllocPtr, size_t size) { return ::operator new(size, std::nothrow); }
void LzmaFree(ISzAllocPtr, void* p) { ::operator delete(p); }

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

}

uint32_t NameHash(std::string_view name) {
    uint32_t h = 0;
    for (unsigned char c : name) h = h * 33 + c;
    return h + (h >> 5);
}

void ApplyBlockHeader(const uint8_t header[8], ArcEntry& entry) {
    entry.size = U32(header);
    entry.offset = entry.block + U16(header + 4);
    entry.compressed = U16(header + 6) == 1;
}

size_t ArcIndexSize(std::span<const uint8_t> head) {
    if (head.size() < 0x34 || std::memcmp(head.data(), "ARC\0", 4) != 0) return 0;
    const uint16_t count = U16(&head[6]);
    if (count == 0) return 0x30;
    const size_t offsets_end = 0x30 + size_t(count) * 4;
    if (head.size() < offsets_end) return offsets_end;  // caller reads more, then asks again
    return U32(&head[0x30]);  // records end where the first entry's data begins
}

bool ParseArc(std::span<const uint8_t> data, std::vector<ArcEntry>& out, std::string* error) {
    out.clear();
    if (data.size() < 0x30 || std::memcmp(data.data(), "ARC\0", 4) != 0) return Fail(error, "not an ARC table");
    const uint16_t count = U16(&data[6]);
    if (U16(&data[8]) != count) return Fail(error, "ARC count mismatch");
    if (count == 0) return true;
    if (data.size() < 0x30 + size_t(count) * 4) return Fail(error, "ARC offsets truncated");
    size_t pos = (0x30 + size_t(count) * 4 + 0x1F) & ~size_t(0x1F);
    for (size_t i = 0; i < count; ++i) {
        if (pos + 0x14 > data.size()) return Fail(error, "ARC record truncated");
        ArcEntry e;
        const char* tag = reinterpret_cast<const char*>(&data[pos]);
        e.tag.assign(tag, strnlen(tag, 4));
        e.hash = U32(&data[pos + 8]);
        const uint16_t name_len = U16(&data[pos + 0x0E]);
        if (pos + 0x14 + name_len > data.size()) return Fail(error, "ARC name truncated");
        const char* name = reinterpret_cast<const char*>(&data[pos + 0x14]);
        e.name.assign(name, strnlen(name, name_len));
        pos = (pos + 0x14 + (name_len ? name_len : 1) + 0x1F) & ~size_t(0x1F);
        e.block = U32(&data[0x30 + i * 4]);
        if (e.block + 8 <= data.size()) {
            ApplyBlockHeader(&data[e.block], e);
            if (e.offset + e.size > data.size()) return Fail(error, "ARC entry outside table: " + e.name);
        }
        out.push_back(std::move(e));
    }
    return true;
}

bool UnpackCmp(std::vector<uint8_t>& data, std::string* error) {
    if (data.size() < 16 || std::memcmp(data.data(), "cmp\0", 4) != 0) return true;
    const uint32_t packed = U32(&data[8]), unpacked = U32(&data[12]);
    if (16 + uint64_t(packed) > data.size()) return Fail(error, "cmp stream truncated");
    std::vector<uint8_t> out(unpacked);
    const uint8_t* src = &data[16];
    if (std::memcmp(&data[4], "zlib", 4) == 0) {
        uLongf out_len = unpacked;
        if (uncompress(out.data(), &out_len, src, packed) != Z_OK || out_len != unpacked) return Fail(error, "zlib error");
    } else if (std::memcmp(&data[4], "lzma", 4) == 0) {
        // LZMA-alone without its 8-byte size field: 5 property bytes, then the coded data
        if (packed < LZMA_PROPS_SIZE) return Fail(error, "lzma stream too short");
        SizeT out_len = unpacked, in_len = packed - LZMA_PROPS_SIZE;
        ELzmaStatus status{};
        const ISzAlloc alloc = {LzmaAlloc, LzmaFree};
        const SRes res = LzmaDecode(out.data(), &out_len, src + LZMA_PROPS_SIZE, &in_len, src, LZMA_PROPS_SIZE,
                                    LZMA_FINISH_ANY, &status, &alloc);
        if (res != SZ_OK || out_len != unpacked) return Fail(error, "lzma error " + std::to_string(res));
    } else {
        return Fail(error, "unknown cmp method");
    }
    data = std::move(out);
    return true;
}

bool EntryMatches(const ArcEntry& entry, size_t index, std::string_view component) {
    if (!component.empty() && component[0] == '#') return component.substr(1) == std::to_string(index);
    if (entry.name.empty()) return false;
    if (component == entry.name) return true;
    return component.size() == entry.name.size() + 1 + entry.tag.size() && component.starts_with(entry.name) &&
           component[entry.name.size()] == '.' && component.substr(entry.name.size() + 1) == entry.tag;
}

}
