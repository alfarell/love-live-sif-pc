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
    void SetVolume(float volume);  // 0..1, at once (the options screen)

    void Play(cri::Pcm pcm);  // replaces the current voice; plays from the start
    bool Playing() const;
    int64_t FedFrames() const { return fed_frames_.load(); }  // frames of the voice handed to SDL so far

    int DeviceRate() const { return device_rate_; }
    int DeviceBufferFrames() const { return device_frames_; }
    double DeviceLatencyMs() const { return device_rate_ ? 1000.0 * device_frames_ / device_rate_ : 0.0; }

    // Master clock: the voice position (ms) audible at `ticks_ns` (SDL_GetTicksNS time base, as in SDL event
    // timestamps). Built from the last callback: frames fed before it, minus two device periods still to play out,
    // plus the time since the callback. The remaining constant (mixer, driver, speaker) is the user's calibration.
    // Negative before playback starts.
    double PlayheadMs(uint64_t ticks_ns) const;

private:
    static void Callback(void* self, SDL_AudioStream* stream, int additional, int total);

    SDL_AudioStream* stream_ = nullptr;
    cri::Pcm voice_;
    size_t cursor_ = 0;  // in samples (interleaved)
    std::atomic<int64_t> fed_frames_{0};
    // written by the callback, read by PlayheadMs: frames fed before the last callback and its time
    std::atomic<int64_t> clock_frames_{0};
    std::atomic<uint64_t> clock_ns_{0};
    std::atomic<int> clock_rate_{48000};
    int device_rate_ = 0, device_frames_ = 0;
};

}
