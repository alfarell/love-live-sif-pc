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

}
