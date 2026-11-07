#pragma once
// Member talk (talk/talk.pac): each member's talk timelines (cNN.pac/talk_NNNN.btlb, compiled "BTL" timelines) and
// the voice cue each one plays (sound/sound.pac voice.acb + voice.awb). See docs/formats/talk.md.

#include <string>
#include <vector>

namespace sif {

class Vfs;

namespace talk {

// The talk ids of member `member` (1-9, 11-19, 21-22) in the dump, ascending.
std::vector<int> TalkIds(const Vfs& vfs, int member);
// The voice cue talk `id` of `member` plays, e.g. "vo_ac_001_505"; "" if it has no voice event (some talks only
// move or change expression) or the file is missing.
std::string VoiceCue(const Vfs& vfs, int member, int id);

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
