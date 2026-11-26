#pragma once
// Member talk (talk/talk.pac): each member's talk timelines (cNN.pac/talk_NNNN.btlb, compiled "BTL" timelines) and
// the voice cue each one plays (sound/sound.pac voice.acb + voice.awb). See docs/formats/talk.md.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sif {

class Vfs;

namespace talk {

// The talk ids of member `member` (1-9, 11-19, 21-22) in the dump, ascending.
std::vector<int> TalkIds(const Vfs& vfs, int member);
// The voice talk `id` of `member` plays: its CueId in sound/voice.acb (e.g. Honoka's greeting talk 50 -> 10707,
// vo_ps_001_007), -1 if it has none (some talks only move) or the file is missing. M18 U6.
int64_t VoiceId(const Vfs& vfs, int member, int id);
// The navigator motion talk `id` plays (501-507: idle1, idle2, talk1-4, login), -1 if none.
int Motion(const Vfs& vfs, int member, int id);
// The first CueId of `member`'s voices: µ's 10000 + 1000 (m - 1), Aqours 40000 + 1000 (m - 11), Saint Snow
// 50000 + 1000 (m - 21). A talk's voice value is added to it.
int64_t VoiceBase(int member);

// One mouth key of a voice's lip-sync timeline (chr/cNN/cNN.pac/timeline.pac voice_NN_<name>_<CueId>.btlb): from
// `frame` (60 per second, from the voice's start) the mouth moves to `shape` over `blend` frames. Shapes 1-7 are the
// face's a i i_small u e o n; 0 closes the mouth.
struct MouthKey {
    int frame = 0, shape = 0, blend = 0;
    float weight = 0;
};
std::vector<MouthKey> MouthKeys(const Vfs& vfs, int member, int64_t voice_id);
// The mouth shape weights (index 1-7; 0 unused) `frames` after the voice started: each key cross-fades from the
// weights before it to its own shape over its blend frames.
std::array<float, 8> MouthAt(const std::vector<MouthKey>& keys, float frames);

// One row of talk_data.bin (BLBN "ll3::talk::s_talk_datas", 16 bytes from 0x40): when talk `id` may play.
// type 0 = any time; 1 = hour range, param = end << 16 | start (wraps past midnight); the other types (2-28: dates,
// play counts, unlocks; Hypothesis) are kept but not evaluated. weight = the row's last u32 >> 16 (Hypothesis:
// priority, 150 for the greetings, 1 for plain talk).
struct Rule {
    int id = 0, type = 0;
    unsigned param = 0;
    int weight = 0;
};
std::vector<Rule> LoadRules(const Vfs& vfs);
// The greeting for local hour `hour` (0-23): the type-1 row whose range holds it; -1 if none.
int GreetingFor(const std::vector<Rule>& rules, int hour);

}
}
