#include "engine/audio/audio_output.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "engine/core/log.h"

namespace sif {

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
    voice_ = std::move(pcm);
    cursor_ = 0;
    fed_frames_ = 0;
    SDL_UnlockAudioStream(stream_);
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
    if (n > 0) {
        SDL_PutAudioStreamData(stream, out->voice_.samples.data() + out->cursor_, int(n * sizeof(int16_t)));
        out->cursor_ += n;
        out->fed_frames_ += int64_t(n / size_t(channels));
    }
}

}
