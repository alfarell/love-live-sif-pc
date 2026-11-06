// message.pac (src/game/text/messages.cpp) and its UI lookup (src/game/text/localizer.cpp) against the user's own
// dump; values from the analysis dumps.
#include <cstdio>
#include <string>

#include "engine/fs/vfs.h"
#include "engine/platform/os.h"
#include "game/text/localizer.h"
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
    Messages en, jp, cn, tw;
    std::string error;
    Check(en.Load(vfs, 1, &error), "English: " + error);
    Check(jp.Load(vfs, 0, &error), "Japanese: " + error);
    Check(cn.Load(vfs, 2, &error), "Chinese: " + error);
    Check(tw.Load(vfs, 3, &error), "Traditional Chinese: " + error);
    Check(en.Size() == 816 && jp.Size() == 816 && cn.Size() == 816 && tw.Size() == 816,
          "816 messages per language");
    Check(en.Get(6) == "New Player" && en.Get(9) == "OK" && en.Get(10) == "Cancel" && en.Get(11) == "Yes",
          "English 6/9/10/11: New Player, OK, Cancel, Yes (the whole text, not cut short)");
    Check(en.Get(814) == "Profile Card Support", "English 814: Profile Card Support");
    Check(jp.Get(300) == "START:DASH!!", "Japanese 300: START:DASH!!");
    Check(jp.Get(3).find("\xe3\x80\x80") != std::string::npos && jp.Get(3).find("\\u") == std::string::npos,
          "Japanese 3: \\u3000 becomes the ideographic space");
    Check(jp.Get(500).find('\n') != std::string::npos && jp.Get(500).find("\\n") == std::string::npos,
          "Japanese 500: \\n becomes a line break");
    Check(en.Get(-1).empty() && en.Get(100000).empty(), "out of range: empty");

    // untranslated entries fall back to Japanese (message ids are the same in every language)
    Check(en.Get(2) == jp.Get(2) && !jp.Get(2).empty(), "English 2: 'English Not Defined' falls back to Japanese");
    Check(en.Get(1) == jp.Get(1) && !jp.Get(1).empty(), "English 1: the '■' fragment falls back to Japanese");
    Check(en.Get(300) == "START:DASH!!", "English 300: 'Eng Music Not Defined' falls back to Japanese");
    Check(cn.Get(2) == jp.Get(2) && cn.Get(6) != jp.Get(6), "Chinese: placeholders fall back, translations are kept");
    Check(tw.Get(2) == jp.Get(2) && tw.Get(6) != jp.Get(6),
          "Traditional Chinese: placeholders fall back, translations are kept");

    // header counts: rows, characters, UTF-8 bytes; a line break is a character without a style row
    const MessageFormat& jp1 = jp.Format(1);
    Check(jp1.kind == 1 && jp1.rows == 1 && jp1.characters == 1 && jp1.bytes == 3,
          "Japanese 1: kind 1, counts (1, 1, 3)");
    Check(jp1.layout.size() == 1 && jp1.layout[0].flags == 2 && jp1.layout[0].records == 2 &&
              jp1.layout[0].record[0].type == 0 && jp1.layout[0].record[0].value == 0 &&
              jp1.layout[0].record[1].type == 2 && jp1.layout[0].record[1].value == 0,
          "Japanese 1: a flag-2 row owns a pair, (type 0, 0) then (type 2, 0)");
    const MessageFormat& jp6 = jp.Format(6);
    Check(jp6.rows == 8 && jp6.characters == 8 && jp6.bytes == 24,
          "Japanese 6: counts (8, 8, 24)");
    Check(jp6.layout[0].flags == 0 && jp6.layout[0].records == 0,
          "Japanese 6: a row at the text offset has no style record");
    const MessageFormat& en6 = en.Format(6);
    Check(en6.layout[0].flags == 0 && en6.layout[0].records == 1 && en6.layout[0].record[0].type == 0 &&
              en6.layout[0].record[0].value == 0xFF0A07DFu,
          "English 6: a flag-0 row shares a colour record, 0xff0a07df");
    Check(en6.layout[8].flags == 1 && en6.layout[8].records == 1,
          "English 6: a flag-1 row owns one record");
    const MessageFormat& jp613 = jp.Format(613);
    Check(jp613.layout[0].record[0].type == 1 && jp613.layout[0].record[0].value == 16 &&
              jp613.layout[3].record[0].type == 1 && jp613.layout[3].record[0].value == 24,
          "Japanese 613 'Lv.%d': type-1 sizes 16 ('Lv.') and 24 ('%d')");
    const MessageFormat& jp814 = jp.Format(814);
    Check(jp814.rows == 16 && jp814.characters == 17 && jp814.bytes == 49,
          "Japanese 814: counts (16, 17, 49): the line break has no row");
    const MessageFormat& en814 = en.Format(814);
    Check(en814.rows == 20 && en814.characters == 20 && en814.bytes == 20,
          "English 814: counts (20, 20, 20)");
    Check(en814.layout[17].flags == 2 && en814.layout[17].records == 2 &&
              en814.layout[17].record[0].type == 0 && en814.layout[17].record[0].value == 0xFF0A07DFu &&
              en814.layout[17].record[1].type == 1 && en814.layout[17].record[1].value == 9,
          "English 814: a flag-2 row owns a colour record then its size record");
    Check(jp.Format(100000).rows == 0 && jp.Format(-1).rows == 0, "Format: out of range is empty");

    // message 815 is complete (the earlier cut was the extractor, not the data)
    Check(jp.Get(815) == "(NPC Profile Card)" && en.Get(815) == "(NPC Profile Card)",
          "message 815 is complete in both languages");

    // the UI default lookup: exact Japanese text -> message id -> selected language
    Localizer localizer;
    Check(localizer.Load(vfs, "en", &error), "localizer English: " + error);
    std::string text;
    Check(localizer.ForDefault(jp.Get(706), text) && text == "Menu", "localizer: メニュー -> Menu");
    Check(localizer.ForDefault(jp.Get(718), text) && text == "BGM Volume", "localizer: BGM音量 -> BGM Volume");
    Check(localizer.ForDefault(jp.Get(300), text) && text == jp.Get(300),
          "localizer: an untranslated message stays Japanese");
    Check(!localizer.ForDefault("not a message", text), "localizer: an unknown default is not replaced");

    std::printf("%s\n", failures ? "message_test FAILED" : "message_test ok");
    return failures ? 1 : 0;
}
