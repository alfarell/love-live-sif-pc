// Member talk (src/game/talk/talk.cpp) against the user's own dump: talk ids, the voice id, motion and mouth keys, and that a voice decodes.
#include <cstdio>
#include <string>

#include "engine/audio/sound_bank.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/talk/talk.h"

using namespace sif;

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sif_talk_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    const auto ids = talk::TalkIds(vfs, 1);
    Check(ids.size() == 92 && ids.front() == 0, "Honoka: 92 talks from talk_0000");
    // M18 U6: tag 0x0028 is the voice (member base + value), tag 0x0514 the motion
    Check(talk::VoiceId(vfs, 1, 50) == 10707 && talk::Motion(vfs, 1, 50) == 503, "Honoka greeting 50: voice 10707, motion 503");
    Check(talk::VoiceBase(11) == 40000 && talk::VoiceBase(22) == 51000, "voice bases: Aqours 40000, Saint Snow 51000 for 22");
    const auto keys = talk::MouthKeys(vfs, 1, 10707);
    Check(keys.size() > 10 && keys[0].frame == 0 && keys[0].shape == 0 && keys[1].frame == 7 && keys[1].shape == 1 &&
              keys[1].blend == 4 && keys[1].weight == 1.0f,
          "voice 10707 mouth keys: close at 0, a at 7 over 4 frames");
    const auto half = talk::MouthAt(keys, 9), full = talk::MouthAt(keys, 11);
    Check(half[1] == 0.5f && full[1] == 1.0f, "MouthAt: a half open at frame 9, open at 11");
    const auto rules = talk::LoadRules(vfs);
    Check(rules.size() == 92 && rules[0].id == 0, "talk_data: 92 rows, one per talk");
    Check(talk::GreetingFor(rules, 8) == 50 && talk::GreetingFor(rules, 13) == 51 && talk::GreetingFor(rules, 20) == 52 &&
              talk::GreetingFor(rules, 22) == 53 && talk::GreetingFor(rules, 23) == 6 && talk::GreetingFor(rules, 3) == 6,
          "greetings by hour: 5-13 talk 50, 13-19 51, 19-21 52, 21-23 53, 23-5 talk 6");
    cri::CueSheet voice;
    cri::Pcm pcm;
    std::string error;
    Check(cri::LoadCueSheet(vfs, "voice", voice, &error) && voice.CueNameById(10707) == "vo_ps_001_007",
          "voice.acb: cue 10707 is vo_ps_001_007");
    int resolved = 0;
    for (int id : talk::TalkIds(vfs, 21)) {
        const int64_t v = talk::VoiceId(vfs, 21, id);
        resolved += v >= 0 && voice.CueNameById(v).starts_with("vo_");
    }
    Check(resolved > 80, "Saint Snow 21: talks resolve to her cues (" + std::to_string(resolved) + ")");
    Check(cri::LoadCue(vfs, voice, "vo_ac_001_505", pcm, &error) &&
              pcm.Frames() > 0,
          "vo_ac_001_505 decodes from voice.awb: " + error);
    std::printf("vo_ac_001_505: %.2f s\n", pcm.rate ? double(pcm.Frames()) / pcm.rate : 0.0);
    std::printf("%s\n", failures ? "talk_test FAILED" : "talk_test ok");
    return failures ? 1 : 0;
}
