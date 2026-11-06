// message.pac (src/game/text/messages.cpp) against the user's own dump; values from the analysis dumps.
#include <cstdio>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/text/messages.h"

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
        std::printf("usage: sif_message_test <CUSA24620 dump>\n");
        return 2;
    }
    Vfs vfs;
    Check(vfs.Mount(os::PathFromUtf8(argv[1])), "mount");
    Messages en, jp;
    std::string error;
    Check(en.Load(vfs, 1, &error), "English: " + error);
    Check(jp.Load(vfs, 0, &error), "Japanese: " + error);
    Check(en.Size() == 816 && jp.Size() == 816, "816 messages per language");
    Check(en.Get(6) == "New Player" && en.Get(9) == "OK" && en.Get(10) == "Cancel" && en.Get(11) == "Yes",
          "English 6/9/10/11: New Player, OK, Cancel, Yes (the whole text, not cut short)");
    Check(en.Get(814) == "Profile Card Support", "English 814: Profile Card Support");
    Check(jp.Get(300) == "START:DASH!!", "Japanese 300: START:DASH!!");
    Check(jp.Get(3).find("\xe3\x80\x80") != std::string::npos && jp.Get(3).find("\\u") == std::string::npos,
          "Japanese 3: \\u3000 becomes the ideographic space");
    Check(jp.Get(500).find('\n') != std::string::npos && jp.Get(500).find("\\n") == std::string::npos,
          "Japanese 500: \\n becomes a line break");
    Check(en.Get(-1).empty() && en.Get(100000).empty(), "out of range: empty");
    std::printf("%s\n", failures ? "message_test FAILED" : "message_test ok");
    return failures ? 1 : 0;
}
