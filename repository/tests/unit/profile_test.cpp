// Photos and profile cards (src/game/profile/profile.cpp) against the user's own dump, and the save's photo and card
// lines (src/game/save/save.cpp).
#include <cstdio>
#include <filesystem>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/profile/profile.h"
#include "game/save/save.h"

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
        std::printf("usage: sif_profile_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    profile::Data data;
    std::string error;
    Check(profile::Load(vfs, data, &error), "load: " + error);
    Check(data.frames.size() == 183, "183 frames: " + std::to_string(data.frames.size()));
    Check(data.presets.size() == 20, "20 presets: " + std::to_string(data.presets.size()));
    int initial = 0;
    for (const profile::Frame& f : data.frames) initial += f.initial ? 1 : 0;
    Check(initial == 20, "20 initial frames: " + std::to_string(initial));
    const profile::Frame* honoka = profile::FindFrame(data, 1001);
    Check(honoka && honoka->initial && honoka->member == 1 &&
              honoka->name[1] == "Honoka Kosaka initial\nProfile Card Frame",
          "frame 1001");

    // presets as decoded in the survey (analysis 202611161811): Maki's fan, frame 1006, song 0120, Wai-Wai! plate
    const profile::Preset& maki = data.presets[5];
    Check(maki.index == 5 && maki.member_card == 60031 && maki.Member() == 6 && maki.frame == 1006 && maki.live == 120 &&
              maki.accolade == 2555 && maki.badge == 901001 && maki.name_plate == 19001 &&
              maki.photo == "preset_0005.jpg" && maki.player[1] == "Maki's fan" && maki.skill[0] == 0,
          "preset 5");
    Check(data.presets[18].Member() == 21 && data.presets[18].live == 1900, "preset 18 (Sarah, Saint Snow)");

    // owned frames: the member's initial frame, plus task-reward frames of that member
    save::SaveData s;
    Check(profile::OwnedFrames(data, s, 1).size() == 1 && profile::OwnedFrames(data, s, 1)[0]->id == 1001,
          "Honoka owns her initial frame only");
    s.owned[{profile::kFrameReward, 5001}] = 1;
    Check(profile::OwnedFrames(data, s, 1).size() == 2, "a task frame of Honoka adds one");
    Check(profile::OwnedFrames(data, s, 2).size() == 1, "Eli does not get Honoka's frame");

    // photo slots: free slots first, then the oldest non-favourite; gallery newest first
    Check(!profile::CanCreateCard(s) && profile::NextPhotoSlot(s) == 0, "no photo: cannot create, slot 0");
    for (int i = 0; i < profile::kPhotoSlots; ++i) s.photos[i] = {1000 + i, 1, 10030, 340011, i < 3};
    Check(profile::CanCreateCard(s), "with photos: can create");
    Check(profile::NextPhotoSlot(s) == 3, "full: the oldest non-favourite (slot 3)");
    s.photos[3].time = 5000;
    Check(profile::NextPhotoSlot(s) == 4, "slot 3 rewritten: next oldest is 4");
    Check(profile::PhotosNewestFirst(s).front() == 3 && profile::PhotosNewestFirst(s).back() == 0, "newest first");
    Check(profile::PhotoFile(7) == "photos/07.png", "photo file name");

    // NPC pick: µ's lives draw two different µ's presets, the other group two from Aqours and Saint Snow
    profile::XorShift128 random;
    for (int round = 0; round < 50; ++round) {
        const auto mu = profile::PickSupport(data, 0, random);
        const auto aq = profile::PickSupport(data, 1, random);
        Check(mu[0] >= 0 && mu[1] >= 0 && mu[0] != mu[1] && data.presets[size_t(mu[0])].Member() <= 9 &&
                  data.presets[size_t(mu[1])].Member() <= 9,
              "µ's pick " + std::to_string(round));
        Check(aq[0] >= 9 && aq[1] >= 9 && aq[0] != aq[1], "Aqours pick " + std::to_string(round));
    }
    profile::XorShift128 a, b;
    Check(profile::PickSupport(data, 0, a) == profile::PickSupport(data, 0, b), "the same start gives the same pick");

    // save round trip of the photo and card lines
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "sif_profile_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    save::SaveData w;
    w.photos[2] = {1763298000, 1, 10030, 340011, true};
    w.has_card = true;
    w.card = {1001, true, 420, 340011, 10030, 5, 901001, 19001, {12016, 0, 0}, {1, 0, 0}};
    Check(save::Write(dir / "save.txt", w, &error), "write: " + error);
    save::SaveData r;
    Check(save::Load(dir / "save.txt", r, &error), "read: " + error);
    Check(r.photos.size() == 1 && r.photos[2].time == 1763298000 && r.photos[2].favourite && r.photos[2].live == 340011,
          "photo line kept");
    Check(r.has_card && r.card.frame == 1001 && r.card.landscape && r.card.crop == 420 && r.card.accolade == 5 &&
              r.card.skill[0] == 12016 && r.card.skill_level[0] == 1 && r.unknown.empty(),
          "profile card line kept");
    std::filesystem::remove_all(dir);

    std::printf(failures ? "%d FAILED\n" : "all profile checks passed\n", failures);
    return failures ? 1 : 0;
}
