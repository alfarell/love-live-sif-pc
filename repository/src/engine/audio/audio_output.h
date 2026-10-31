#pragma once
// Audio output on SDL3: one PCM voice fed from the device callback, a fed-frames counter (the basis of M4's master
// clock), and the device buffer size actually granted, logged as output latency.

#include <atomic>
#include <cstdint>
#include <string>

#include "engine/audio/decode.h"

struct SDL_AudioStream;

namespace sif {

class AudioOutput {
public:
    // buffer_frames: requested device buffer (SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES). Needs SDL_INIT_AUDIO.
    bool Open(int buffer_frames, float volume, std::string* error = nullptr);
    void Close();

    void Play(cri::Pcm pcm);  // replaces the current voice; plays from the start
    bool Playing() const;
    int64_t FedFrames() const { return fed_frames_.load(); }  // frames of the voice handed to SDL so far

    int DeviceRate() const { return device_rate_; }
    int DeviceBufferFrames() const { return device_frames_; }
    double DeviceLatencyMs() const { return device_rate_ ? 1000.0 * device_frames_ / device_rate_ : 0.0; }

private:
    static void Callback(void* self, SDL_AudioStream* stream, int additional, int total);

    SDL_AudioStream* stream_ = nullptr;
    cri::Pcm voice_;
    size_t cursor_ = 0;  // in samples (interleaved)
    std::atomic<int64_t> fed_frames_{0};
    int device_rate_ = 0, device_frames_ = 0;
};

}
