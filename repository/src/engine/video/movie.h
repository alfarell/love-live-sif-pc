#pragma once
// CRI Sofdec2 USM movies (the dump's movie/*.usm, M8): demux of the video stream and MPEG-1 decoding with pl_mpeg.
// USM chunk (big-endian): char[4] signature (CRID, @SFV video, @SFA audio, ...), u32 size after these 8 bytes, then
// u8 ?, u8 payload offset (from byte 8), u16 padding at the end, u8 channel, u8 ?, u8 ?, u8 type (0 data, 1 header
// @UTF table, 2 end/metadata, 3 seek). SIF's movies have one @SFV stream, MPEG-1 (mpeg_codec 1), no audio
// (analysis/202611091206-m8-video-and-polish).

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/video/pl_mpeg_source.h"

struct plm_video_t;

namespace sif {

class Vfs;

namespace video {

struct UsmInfo {
    int width = 0, height = 0;
    double fps = 0;   // framerate_n / framerate_d
    int frames = 0;   // total_frames from the header
    int dc_precision = 8;  // mpeg_dcprec: Sofdec's intra DC precision (11 in SIF; MPEG-1 itself has 8)
};

// The video header and the MPEG-1 elementary stream (the @SFV channel 0 data payloads, joined).
bool ParseUsm(std::span<const uint8_t> usm, UsmInfo& info, std::vector<uint8_t>& stream, std::string* error = nullptr);

// One movie, decoded forward on demand.
class Movie {
public:
    Movie() = default;
    Movie(const Movie&) = delete;
    Movie& operator=(const Movie&) = delete;
    ~Movie();

    bool Open(const Vfs& vfs, std::string_view path, std::string* error = nullptr);
    bool OpenMemory(std::span<const uint8_t> usm, std::string* error = nullptr);
    void Close();
    bool IsOpen() const { return video_ != nullptr; }
    const UsmInfo& Info() const { return info_; }

    // The RGBA8 picture of frame `n` (wrapped into 0 .. frames-1): decodes forward from the current frame, rewinding
    // first when `n` is behind it. Returns nullptr if the stream cannot be decoded. Width x height x 4 bytes.
    const std::vector<uint8_t>* Frame(int n);
    int Current() const { return current_; }

private:
    UsmInfo info_;
    std::vector<uint8_t> stream_;
    sif_plm_memory source_{};  // pl_mpeg's view of stream_
    plm_video_t* video_ = nullptr;
    int current_ = -1;  // the frame in rgba_, -1 none
    std::vector<uint8_t> rgba_;
};

}  // namespace video
}  // namespace sif
