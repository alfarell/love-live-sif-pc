// Unit test: the log writes leveled lines to its file and LogExit records the exit once.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "engine/core/crash_report.h"
#include "engine/core/log.h"

static int failures = 0;
static void Check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", what);
    }
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / "sif_log_test.log";
    sif::LogSetFile(path);
    sif::LogInfo("hello {}", 42);
    sif::LogError("bad {}", "thing");
    sif::LogExit(7, "test");
    sif::LogExit(8, "second call must not be logged");
    sif::LogSetFile({});
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    Check(text.find("info  hello 42") != std::string::npos, "info line");
    Check(text.find("error bad thing") != std::string::npos, "error line");
    Check(text.find("exit: code 7 (test)") != std::string::npos, "exit logged");
    Check(text.find("code 8") == std::string::npos, "exit logged once");
    std::printf("%s\n", failures ? "log_test FAILED" : "log_test ok");
    return failures ? 1 : 0;
}
