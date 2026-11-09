#include "engine/video/movie.h"

#include <algorithm>
#include <cstdio>  // pl_mpeg.h names FILE before it includes stdio.h

#include "engine/audio/cri_utf.h"
#include "engine/fs/vfs.h"
#include "engine/video/pl_mpeg_source.h"
#include "pl_mpeg.h"

namespace sif::video {
namespace {

bool Fail(std::string* error, std::string text) {
    if (error) *error = std::move(text);
    return false;
}

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

}

bool ParseUsm(std::span<const uint8_t> usm, UsmInfo& info, std::vector<uint8_t>& stream, std::string* error) {
    info = {};
    stream.clear();
    bool header = false;
    for (size_t pos = 0; pos + 0x20 <= usm.size();) {
        const uint8_t* c = usm.data() + pos;
        const size_t size = Be32(c + 4), offset = c[9], padding = Be16(c + 10);
        if (pos + 8 + size > usm.size() || offset + padding > size) return Fail(error, "USM chunk overruns the file");
        const uint8_t channel = c[12], type = c[15];
        const auto payload = usm.subspan(pos + 8 + offset, size - offset - padding);
        if (std::equal(c, c + 4, "@SFV") && channel == 0) {
            if (type == 0) {
                stream.insert(stream.end(), payload.begin(), payload.end());
            } else if (type == 1 && !header) {
                cri::UtfTable t;
                if (!t.Parse(payload, error)) return false;
                info.width = int(t.Int(0, "width", 0));
                info.height = int(t.Int(0, "height", 0));
                const int64_t n = t.Int(0, "framerate_n", 0), d = t.Int(0, "framerate_d", 0);
                info.fps = d > 0 ? double(n) / double(d) : 0;
                info.frames = int(t.Int(0, "total_frames", 0));
                info.dc_precision = int(t.Int(0, "mpeg_dcprec", 8));
                if (info.dc_precision < 8 || info.dc_precision > 11) return Fail(error, "USM: unknown mpeg_dcprec");
                if (t.Int(0, "mpeg_codec", 0) != 1) return Fail(error, "USM video is not MPEG-1 (mpeg_codec != 1)");
                header = true;
            }
        }
        pos += 8 + size;
    }
    if (!header || info.width <= 0 || info.height <= 0 || info.fps <= 0 || info.frames <= 0) {
        return Fail(error, "USM has no usable @SFV header");
    }
    return !stream.empty() || Fail(error, "USM has no video data");
}

Movie::~Movie() { Close(); }

void Movie::Close() {
    if (video_) plm_video_destroy(video_);  // also destroys its buffer (destroy_when_done)
    video_ = nullptr;
    current_ = -1;
}

bool Movie::Open(const Vfs& vfs, std::string_view path, std::string* error) {
    std::vector<uint8_t> file;
    return vfs.Read(path, file, error) && OpenMemory(file, error);
}

bool Movie::OpenMemory(std::span<const uint8_t> usm, std::string* error) {
    Close();
    if (!ParseUsm(usm, info_, stream_, error)) return false;
    // pl_mpeg copies from stream_ through source_ (pl_mpeg_source.h); both live as long as the decoder
    source_ = {stream_.data(), stream_.size(), 0};
    plm_buffer_t* buffer = sif_plm_buffer_create(&source_);
    if (!buffer) return Fail(error, "pl_mpeg: no buffer");
    video_ = plm_video_create_with_buffer(buffer, 1);
    if (!video_) {
        plm_buffer_destroy(buffer);
        return Fail(error, "pl_mpeg: no decoder");
    }
    plm_video_set_intra_dc_precision(video_, info_.dc_precision);
    rgba_.assign(size_t(info_.width) * size_t(info_.height) * 4, 255);  // pl_mpeg leaves alpha untouched
    return true;
}

const std::vector<uint8_t>* Movie::Frame(int n) {
    if (!video_) return nullptr;
    n %= info_.frames;
    if (n < 0) n += info_.frames;
    if (n == current_) return &rgba_;
    if (n < current_) {
        plm_video_rewind(video_);
        current_ = -1;
    }
    plm_frame_t* frame = nullptr;
    while (current_ < n) {
        frame = plm_video_decode(video_);
        if (!frame) return nullptr;  // the stream has fewer pictures than the header says
        ++current_;
    }
    if (int(frame->width) != info_.width || int(frame->height) != info_.height) return nullptr;
    plm_frame_to_rgba(frame, rgba_.data(), info_.width * 4);
    return &rgba_;
}

}  // namespace sif::video
