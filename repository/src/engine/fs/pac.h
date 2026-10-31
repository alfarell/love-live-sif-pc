#pragma once
// SIF `.pac` containers: ARC\0 tables and cmp\0 (zlib / size-less LZMA-alone) wrappers. Spec: docs/formats/arc.md.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sif::pac {

struct ArcEntry {
    std::string tag;     // "tex", "mdl", "ARC"-carrying tags such as "mots", ...
    std::string name;    // may be empty (shader parts, message.pac tables)
    uint32_t hash = 0;   // NameHash(name), 0 for nameless entries
    uint64_t block = 0;  // entry data block (u32 size, u16 header length, u16 flags), relative to the table start
    uint64_t offset = 0; // payload offset, relative to the table start (set by ApplyBlockHeader)
    uint32_t size = 0;   // payload size (set by ApplyBlockHeader)
    bool compressed = false;  // entry flag 1: the payload is a cmp\0 wrapper
};

// Reads the 8-byte block header at `entry.block` and fills offset, size and compressed.
void ApplyBlockHeader(const uint8_t header[8], ArcEntry& entry);

// djb2 without the 5381 seed, then h += h >> 5 (matches all 83,998 named entries).
uint32_t NameHash(std::string_view name);

// Parses the records of an ARC\0 table held in `data` (at least ArcIndexSize bytes). When `data` holds the whole
// table, the block headers are applied too; otherwise offset/size stay 0 until ApplyBlockHeader.
bool ParseArc(std::span<const uint8_t> data, std::vector<ArcEntry>& out, std::string* error = nullptr);

// Bytes of an ARC\0 table needed before its first entry: header, offsets and records. 0 if `head` is too short
// to tell (pass at least 0x34 bytes).
size_t ArcIndexSize(std::span<const uint8_t> head);

// Replaces a cmp\0 buffer with its unpacked contents; leaves other data unchanged. False on a corrupt stream.
bool UnpackCmp(std::vector<uint8_t>& data, std::string* error = nullptr);

// True if `component` names this entry: its name, its name plus ".<tag>", or "#<index>".
bool EntryMatches(const ArcEntry& entry, size_t index, std::string_view component);

}
