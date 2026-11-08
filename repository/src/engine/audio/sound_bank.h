#pragma once
// Loads cue sheets from the dump (sound/sound.pac/<name>.acb) and decodes a cue's wave, reading streamed waves from
// sound/<bank>.awb or sound/dlc/<bank>.awb (the user's installed song add-ons).

#include <string>
#include <string_view>

#include "engine/audio/cue_sheet.h"
#include "engine/audio/decode.h"
#include "engine/fs/vfs.h"

namespace sif::cri {

bool LoadCueSheet(const Vfs& vfs, std::string_view name, CueSheet& out, std::string* error = nullptr);
bool LoadCue(const Vfs& vfs, const CueSheet& sheet, std::string_view cue, Pcm& out, std::string* error = nullptr);
bool LoadWave(const Vfs& vfs, const CueSheet& sheet, const Wave& wave, Pcm& out, std::string* error = nullptr);
// All layers of cue `cue_id` decoded, converted to `rate` and `channels` (linear resampling; mono is copied to every
// channel, more channels than wanted are averaged) and summed with their gains, clamped to 16 bits. For sound effects
// mixed into a voice of that format (AudioOutput::PlayEffect).
bool LoadCueMix(const Vfs& vfs, const CueSheet& sheet, int64_t cue_id, int rate, int channels, Pcm& out,
                std::string* error = nullptr);

}
