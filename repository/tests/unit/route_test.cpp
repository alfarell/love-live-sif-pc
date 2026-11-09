// M9 R1: the route script parser. Usage: sif_route_test
#include <cstdio>
#include <string>
#include <vector>

#include "app/route.h"

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main() {
    using sif::route::Command;
    std::vector<Command> c;
    std::string error;
    const char* good =
        "# a comment line\n"
        "timeout 90s\n"
        "wait 500ms   # trailing comment\n"
        "\n"
        "key enter\n"
        "key ;\n"
        "press enter until menu 30s\n"
        "expect screen select 20s\n"
        "expect screen menu\n"
        "expect log live result: Perfect\n"
        "expect nolog error\n"
        "shot after_live\n"
        "quit\n";
    Check(sif::route::Parse(good, c, &error), "a good route parses: " + error);
    Check(c.size() == 11, "11 commands (comments and blank lines skipped), got " + std::to_string(c.size()));
    if (c.size() == 11) {
        Check(c[0].op == "timeout" && c[0].ms == 90000, "timeout 90s = 90,000 ms");
        Check(c[1].op == "wait" && c[1].ms == 500 && c[1].line == 3, "wait 500ms on line 3");
        Check(c[2].op == "key" && c[2].what == "enter" && c[3].what == ";", "keys enter and ;");
        Check(c[4].op == "press" && c[4].what == "enter" && c[4].text == "menu" && c[4].ms == 30000, "press enter until menu 30s");
        c.erase(c.begin() + 4);
        Check(c[4].what == "screen" && c[4].text == "select" && c[4].ms == 20000, "expect screen with its own limit");
        Check(c[5].text == "menu" && c[5].ms == 15000, "expect screen default limit 15 s");
        Check(c[6].what == "log" && c[6].text == "live result: Perfect", "expect log keeps the spaces of its text");
        Check(c[7].what == "nolog" && c[7].text == "error", "expect nolog");
        Check(c[8].op == "shot" && c[8].what == "after_live" && c[9].op == "quit", "shot and quit");
    }
    for (const char* bad : {"wait 5", "wait soon", "key x", "expect", "expect sound on", "expect log", "shot", "jump 3",
                            "timeout 5m", "press enter", "press enter until", "press x until menu", ""}) {
        error.clear();
        Check(!sif::route::Parse(bad, c, &error) && !error.empty(), std::string("refused: '") + bad + "' (" + error + ")");
    }
    error.clear();
    sif::route::Parse("wait 1s\nkey nope\n", c, &error);
    Check(error.rfind("line 2:", 0) == 0, "the error names the line: " + error);
    std::printf("%s\n", failures ? "route_test FAILED" : "route_test ok");
    return failures ? 1 : 0;
}
