#include "engine/audio/audio_output.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "engine/core/log.h"

namespace sif {

void AudioOutput::SetVolume(float volume) {
    if (stream_) SDL_SetAudioStreamGain(stream_, volume);
}

bool AudioOutput::Open(int buffer_frames, float volume, std::string* error) {
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, std::to_string(buffer_frames).c_str());
    // SIF's music and voice are 48 kHz stereo/mono; SDL converts other voices and the device rate
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, 48000};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioOutput::Callback, this);
    if (!stream_) {
        if (error) *error = std::string("SDL_OpenAudioDeviceStream: ") + SDL_GetError();
        return false;
    }
    SDL_SetAudioStreamGain(stream_, volume);
    SDL_AudioSpec device{};
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream_), &device, &device_frames_);
    device_rate_ = device.freq;
    LogInfo("audio: device {} Hz, {} ch, buffer {} frames requested / {} granted = {:.2f} ms output latency",
            device.freq, device.channels, buffer_frames, device_frames_, DeviceLatencyMs());
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void AudioOutput::Close() {
    if (stream_) SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
}

void AudioOutput::Play(cri::Pcm pcm) {
    if (!stream_) return;
    SDL_LockAudioStream(stream_);
    const SDL_AudioSpec spec{SDL_AUDIO_S16, pcm.channels, pcm.rate};
    SDL_SetAudioStreamFormat(stream_, &spec, nullptr);  // input format follows the voice
    SDL_ClearAudioStream(stream_);
    clock_rate_ = pcm.rate > 0 ? pcm.rate : 48000;
    voice_ = std::move(pcm);
    cursor_ = 0;
    fed_frames_ = 0;
    clock_frames_ = 0;
    clock_ns_ = 0;  // no callback yet: PlayheadMs counts from the first one
    effects_ = {};
    SDL_UnlockAudioStream(stream_);
}

void AudioOutput::PlayEffect(const cri::Pcm* pcm) {
    if (!stream_ || !pcm || pcm->samples.empty()) return;
    SDL_LockAudioStream(stream_);
    if (pcm->channels == voice_.channels && pcm->rate == voice_.rate) {
        size_t slot = next_effect_;
        for (size_t i = 0; i < kEffects; ++i) {
            if (!effects_[(next_effect_ + i) % kEffects].pcm) {
                slot = (next_effect_ + i) % kEffects;
                break;
            }
        }
        effects_[slot] = {pcm, 0};
        next_effect_ = (slot + 1) % kEffects;
        if (mix_.capacity() < 16384) mix_.reserve(16384);  // keeps the callback from allocating
    }
    SDL_UnlockAudioStream(stream_);
}

void AudioOutput::StopEffects() {
    if (!stream_) return;
    SDL_LockAudioStream(stream_);
    effects_ = {};
    SDL_UnlockAudioStream(stream_);
}

double AudioOutput::PlayheadMs(uint64_t ticks_ns) const {
    const uint64_t at = clock_ns_.load();
    if (at == 0) return -1000.0 * device_frames_ / std::max(1, device_rate_);
    const double rate = clock_rate_.load();
    const double since_ms = (double(int64_t(ticks_ns - at))) / 1e6;
    // two device periods in flight at a callback (the one playing + the one being filled): measured with sif_sweep,
    // presses on the beat in the system mix read +18 ms with one period subtracted (analysis 202611010048)
    return 1000.0 * double(clock_frames_.load()) / rate - 2 * DeviceLatencyMs() + since_ms;
}

bool AudioOutput::Playing() const {
    if (!stream_) return false;
    SDL_LockAudioStream(stream_);
    const bool playing = cursor_ < voice_.samples.size() || SDL_GetAudioStreamQueued(stream_) > 0;
    SDL_UnlockAudioStream(stream_);
    return playing;
}

void AudioOutput::Callback(void* self, SDL_AudioStream* stream, int additional, int /*total*/) {
    // runs on SDL's audio thread with the stream locked
    auto* out = static_cast<AudioOutput*>(self);
    const int channels = std::max(1, out->voice_.channels);
    const size_t want = size_t(additional) / sizeof(int16_t);
    const size_t left = out->voice_.samples.size() - std::min(out->cursor_, out->voice_.samples.size());
    const size_t n = std::min(want, left) / size_t(channels) * size_t(channels);
    // clock sample: what was fed before this callback starts playing once the device buffer drains
    out->clock_frames_ = out->fed_frames_.load();
    out->clock_ns_ = SDL_GetTicksNS();
    if (n > 0) {
        const int16_t* data = out->voice_.samples.data() + out->cursor_;
        bool mixing = false;
        for (const Effect& e : out->effects_) mixing |= e.pcm != nullptr;
        if (mixing) {  // effects summed onto this chunk of the voice, clamped
            out->mix_.assign(data, data + n);
            for (Effect& e : out->effects_) {
                if (!e.pcm) continue;
                const size_t m = std::min(n, e.pcm->samples.size() - e.cursor);
                for (size_t i = 0; i < m; ++i) {
                    out->mix_[i] = int16_t(std::clamp(int(out->mix_[i]) + e.pcm->samples[e.cursor + i], -32768, 32767));
                }
                e.cursor += m;
                if (e.cursor >= e.pcm->samples.size()) e = {};
            }
            data = out->mix_.data();
        }
        SDL_PutAudioStreamData(stream, data, int(n * sizeof(int16_t)));
        out->cursor_ += n;
        out->fed_frames_ += int64_t(n / size_t(channels));
    }
}

}
