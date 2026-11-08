#include "engine/audio/cue_sheet.h"

#include <cstring>

namespace sif::cri {
namespace {

uint64_t Le(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; --i) v = v << 8 | p[i];
    return v;
}

uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

}

size_t Afs2TableSize(std::span<const uint8_t> b) {
    if (b.size() < 16 || std::memcmp(b.data(), "AFS2", 4) != 0) return 0;
    return 16 + size_t(Le(&b[8], 4)) * b[6] + (size_t(Le(&b[8], 4)) + 1) * b[5];
}

bool ParseAfs2(std::span<const uint8_t> b, std::vector<Afs2Wave>& out, std::string* error, bool table_only) {
    out.clear();
    if (b.size() < 16 || std::memcmp(b.data(), "AFS2", 4) != 0) return Fail(error, "not an AFS2 bank");
    const int off_size = b[5], id_size = b[6];
    const uint32_t count = uint32_t(Le(&b[8], 4));
    const uint32_t align = uint32_t(Le(&b[12], 2));
    const size_t ids = 16, offs = ids + size_t(count) * id_size;
    if (off_size < 2 || off_size > 8 || id_size < 1 || id_size > 4 || align == 0 ||
        offs + size_t(count + 1) * off_size > b.size()) {
        return Fail(error, "AFS2 header out of range");
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t start = Le(&b[offs + size_t(i) * off_size], off_size);
        const uint64_t end = Le(&b[offs + size_t(i + 1) * off_size], off_size);
        start = (start + align - 1) / align * align;
        if (start > end || (!table_only && end > b.size())) return Fail(error, "AFS2 wave out of range");
        out.push_back({uint32_t(Le(&b[ids + size_t(i) * id_size], id_size)), start, end - start});
    }
    return true;
}

std::span<const uint8_t> FindAfs2Wave(std::span<const uint8_t> bank, uint32_t id) {
    std::vector<Afs2Wave> waves;
    if (!ParseAfs2(bank, waves)) return {};
    for (const Afs2Wave& w : waves) {
        if (w.id == id) return bank.subspan(size_t(w.offset), size_t(w.size));
    }
    return {};
}

bool CueSheet::Load(std::vector<uint8_t> acb, std::string* error) {
    data_ = std::move(acb);
    if (!header_.Parse(data_, error) || header_.Rows() != 1) return Fail(error, "bad ACB header table");
    name_ = std::string(header_.Str(0, "Name"));
    const auto sub = [&](const char* column, UtfTable& t) {
        const auto d = header_.Data(0, column);
        // absent, or not a table (sys.acb's StreamAwbHash is a 16-byte blob: it has no stream banks)
        return d.size() < 4 || std::memcmp(d.data(), "@UTF", 4) != 0 || t.Parse(d);
    };
    if (!sub("CueTable", cues_) || !sub("CueNameTable", names_) || !sub("SequenceTable", sequences_) ||
        !sub("TrackTable", tracks_) || !sub("TrackEventTable", events_) || !sub("SynthTable", synths_) ||
        !sub("WaveformTable", waves_) || !sub("StreamAwbHash", banks_) || !sub("TrackCommandTable", track_commands_)) {
        return Fail(error, "bad ACB sub-table");
    }
    memory_bank_ = header_.Data(0, "AwbFile");
    return true;
}

std::vector<std::string> CueSheet::CueNames() const {
    std::vector<std::string> out;
    for (size_t r = 0; r < names_.Rows(); ++r) out.emplace_back(names_.Str(r, "CueName"));
    return out;
}

std::string_view CueSheet::StreamBankName(int port) const { return banks_.Str(size_t(port), "Name"); }

bool CueSheet::WaveFromReference(int type, int64_t index, Wave& out, int depth) const {
    if (depth > 8 || index < 0) return false;
    if (type == 1) {  // waveform
        const size_t r = size_t(index);
        if (r >= waves_.Rows()) return false;
        out.encode = int(waves_.Int(r, "EncodeType"));
        out.streaming = waves_.Int(r, "Streaming", 0) != 0;
        out.port = int(waves_.Int(r, "StreamAwbPortNo", 0));
        out.stream_id = int(waves_.Int(r, "StreamAwbId"));
        out.memory_id = int(waves_.Int(r, "MemoryAwbId"));
        out.channels = int(waves_.Int(r, "NumChannels", 0));
        out.rate = int(waves_.Int(r, "SamplingRate", 0));
        out.samples = waves_.Int(r, "NumSamples", 0);
        out.loop = waves_.Int(r, "LoopFlag", 0) != 0;
        return true;
    }
    if (type == 2) {  // synth: (u16 type, u16 index) pairs
        const auto items = synths_.Data(size_t(index), "ReferenceItems");
        for (size_t i = 0; i + 4 <= items.size(); i += 4) {
            if (WaveFromReference(Be16(&items[i]), Be16(&items[i + 2]), out, depth + 1)) return true;
        }
        return false;
    }
    if (type == 3) {  // sequence -> tracks -> track event commands
        const auto track_list = sequences_.Data(size_t(index), "TrackIndex");
        for (size_t t = 0; t + 2 <= track_list.size(); t += 2) {
            const int64_t event = tracks_.Int(Be16(&track_list[t]), "EventIndex");
            if (event < 0) continue;
            const auto cmd = events_.Data(size_t(event), "Command");
            for (size_t p = 0; p + 3 <= cmd.size();) {  // u16 opcode, u8 size, data
                const uint16_t op = Be16(&cmd[p]);
                const size_t n = cmd[p + 2];
                if (op == 0x07D0 && n >= 4 && p + 3 + n <= cmd.size() &&  // note on
                    WaveFromReference(Be16(&cmd[p + 3]), Be16(&cmd[p + 5]), out, depth + 1)) {
                    return true;
                }
                p += 3 + n;
            }
        }
    }
    return false;
}

float CueSheet::TrackGain(size_t track) const {
    const int64_t c = tracks_.Int(track, "CommandIndex", -1);
    if (c < 0 || size_t(c) >= track_commands_.Rows()) return 1;
    const auto cmd = track_commands_.Data(size_t(c), "Command");
    for (size_t p = 0; p + 3 <= cmd.size();) {  // u16 opcode, u8 size, data
        const size_t n = cmd[p + 2];
        if (Be16(&cmd[p]) == 0x0057 && n == 2 && p + 5 <= cmd.size()) return float(Be16(&cmd[p + 3])) / 100.0f;
        p += 3 + n;
    }
    return 1;
}

void CueSheet::CollectLayers(int type, int64_t index, float gain, std::vector<Layer>& out, int depth) const {
    if (depth > 8 || index < 0) return;
    if (type == 1) {
        Layer l;
        l.gain = gain;
        if (WaveFromReference(1, index, l.wave, depth)) out.push_back(l);
    } else if (type == 2) {
        if (size_t(index) >= synths_.Rows()) return;
        const bool all = synths_.Int(size_t(index), "Type", 0) == 0;  // 0: polyphonic
        const auto items = synths_.Data(size_t(index), "ReferenceItems");
        for (size_t i = 0; i + 4 <= items.size(); i += 4) {
            CollectLayers(Be16(&items[i]), Be16(&items[i + 2]), gain, out, depth + 1);
            if (!all) break;
        }
    } else if (type == 3) {
        if (size_t(index) >= sequences_.Rows()) return;
        const auto track_list = sequences_.Data(size_t(index), "TrackIndex");
        for (size_t t = 0; t + 2 <= track_list.size(); t += 2) {
            const size_t track = Be16(&track_list[t]);
            const int64_t event = tracks_.Int(track, "EventIndex");
            if (event < 0) continue;
            const auto cmd = events_.Data(size_t(event), "Command");
            for (size_t p = 0; p + 3 <= cmd.size();) {
                const size_t n = cmd[p + 2];
                if (Be16(&cmd[p]) == 0x07D0 && n >= 4 && p + 3 + n <= cmd.size()) {  // note on
                    CollectLayers(Be16(&cmd[p + 3]), Be16(&cmd[p + 5]), gain * TrackGain(track), out, depth + 1);
                }
                p += 3 + n;
            }
        }
    }
}

bool CueSheet::FindLayersById(int64_t cue_id, std::vector<Layer>& out, std::string* error) const {
    out.clear();
    for (size_t c = 0; c < cues_.Rows(); ++c) {
        if (cues_.Int(c, "CueId") != cue_id) continue;
        CollectLayers(int(cues_.Int(c, "ReferenceType")), cues_.Int(c, "ReferenceIndex"), 1.0f, out, 0);
        return !out.empty() || Fail(error, "cue id " + std::to_string(cue_id) + " has no waveform reference");
    }
    return Fail(error, "no cue id " + std::to_string(cue_id) + " in " + name_);
}

bool CueSheet::FindWaveById(int64_t cue_id, Wave& out, std::string* error) const {
    for (size_t c = 0; c < cues_.Rows(); ++c) {
        if (cues_.Int(c, "CueId") != cue_id) continue;
        if (WaveFromReference(int(cues_.Int(c, "ReferenceType")), cues_.Int(c, "ReferenceIndex"), out, 0)) return true;
        return Fail(error, "cue id " + std::to_string(cue_id) + " has no waveform reference");
    }
    return Fail(error, "no cue id " + std::to_string(cue_id) + " in " + name_);
}

std::string_view CueSheet::CueNameById(int64_t cue_id) const {
    for (size_t c = 0; c < cues_.Rows(); ++c) {
        if (cues_.Int(c, "CueId") != cue_id) continue;
        for (size_t r = 0; r < names_.Rows(); ++r) {
            if (names_.Int(r, "CueIndex") == int64_t(c)) return names_.Str(r, "CueName");
        }
    }
    return {};
}

bool CueSheet::FindWave(std::string_view cue, Wave& out, std::string* error) const {
    for (size_t r = 0; r < names_.Rows(); ++r) {
        if (names_.Str(r, "CueName") != cue) continue;
        const int64_t c = names_.Int(r, "CueIndex");
        if (c < 0 || size_t(c) >= cues_.Rows()) return Fail(error, "bad cue index");
        if (WaveFromReference(int(cues_.Int(size_t(c), "ReferenceType")), cues_.Int(size_t(c), "ReferenceIndex"), out, 0)) {
            return true;
        }
        return Fail(error, "cue '" + std::string(cue) + "' has no waveform reference");
    }
    return Fail(error, "no cue '" + std::string(cue) + "' in " + name_);
}

}
