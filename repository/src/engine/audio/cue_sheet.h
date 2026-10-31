#pragma once
// ACB cue sheets and AFS2 wave banks. A cue resolves as: CueNameTable -> CueTable (reference type 3 = sequence,
// 2 = synth, 1 = waveform) -> SequenceTable.TrackIndex -> TrackTable.EventIndex -> TrackEventTable.Command
// (note-on 0x07D0: u16 reference type, u16 index) -> SynthTable.ReferenceItems (u16 type 1 = waveform, u16 index)
// -> WaveformTable. Streamed waves name their bank by StreamAwbPortNo -> StreamAwbHash.Name (e.g. "MUSIC_0010").

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/audio/cri_utf.h"

namespace sif::cri {

enum class Codec { ADX = 0, HCA = 2 };

struct Wave {
    int encode = -1;          // EncodeType: 0 ADX, 2 HCA
    bool streaming = false;   // in a .awb file (else in the cue sheet's embedded AWB)
    int port = 0;             // StreamAwbPortNo: index into StreamBankName
    int stream_id = -1;       // wave id in the stream bank
    int memory_id = -1;       // wave id in the embedded bank
    int channels = 0, rate = 0;
    int64_t samples = 0;
    bool loop = false;
};

struct Afs2Wave {
    uint32_t id = 0;
    uint64_t offset = 0, size = 0;
};

// table_only: `bank` holds just the header and tables (wave data not loaded), so wave ends are not range-checked.
bool ParseAfs2(std::span<const uint8_t> bank, std::vector<Afs2Wave>& out, std::string* error = nullptr,
               bool table_only = false);
// Bytes from the start of an AFS2 bank up to the end of its tables, given its first 16 bytes; 0 if not AFS2.
size_t Afs2TableSize(std::span<const uint8_t> head16);
// The wave with `id` in an AFS2 bank, or an empty span.
std::span<const uint8_t> FindAfs2Wave(std::span<const uint8_t> bank, uint32_t id);

class CueSheet {
public:
    bool Load(std::vector<uint8_t> acb, std::string* error = nullptr);
    std::string_view Name() const { return name_; }
    std::vector<std::string> CueNames() const;
    bool FindWave(std::string_view cue, Wave& out, std::string* error = nullptr) const;
    std::string_view StreamBankName(int port) const;
    std::span<const uint8_t> MemoryBank() const { return memory_bank_; }

private:
    bool WaveFromReference(int type, int64_t index, Wave& out, int depth) const;

    std::vector<uint8_t> data_;
    std::string name_;
    UtfTable header_, cues_, names_, sequences_, tracks_, events_, synths_, waves_, banks_;
    std::span<const uint8_t> memory_bank_;
};

}
