// Member talk (src/game/talk/talk.cpp) against the user's own dump: talk ids, the voice cue, and that it decodes.
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
    Check(talk::VoiceCue(vfs, 1, 0) == "vo_ac_001_505", "Honoka talk 0 plays vo_ac_001_505");
    const auto snow = talk::TalkIds(vfs, 21);
    std::string cue;
    for (int id : snow) {
        if (!(cue = talk::VoiceCue(vfs, 21, id)).empty()) break;
    }
    Check(cue.rfind("vo_ac_21_", 0) == 0, "Saint Snow: a cue named with the two-digit member (" + cue + ")");
    const auto rules = talk::LoadRules(vfs);
    Check(rules.size() == 92 && rules[0].id == 0, "talk_data: 92 rows, one per talk");
    Check(talk::GreetingFor(rules, 8) == 50 && talk::GreetingFor(rules, 13) == 51 && talk::GreetingFor(rules, 20) == 52 &&
              talk::GreetingFor(rules, 22) == 53 && talk::GreetingFor(rules, 23) == 6 && talk::GreetingFor(rules, 3) == 6,
          "greetings by hour: 5-13 talk 50, 13-19 51, 19-21 52, 21-23 53, 23-5 talk 6");
    cri::CueSheet voice;
    cri::Pcm pcm;
    std::string error;
    Check(cri::LoadCueSheet(vfs, "voice", voice, &error) && cri::LoadCue(vfs, voice, "vo_ac_001_505", pcm, &error) &&
              pcm.Frames() > 0,
          "vo_ac_001_505 decodes from voice.awb: " + error);
    std::printf("vo_ac_001_505: %.2f s\n", pcm.rate ? double(pcm.Frames()) / pcm.rate : 0.0);
    std::printf("%s\n", failures ? "talk_test FAILED" : "talk_test ok");
    return failures ? 1 : 0;
}
