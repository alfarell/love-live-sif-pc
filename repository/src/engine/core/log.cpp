// Copied from pt-pc (https://github.com/LoreanXavier/pt-pc) src/engine/core/log.cpp at commit 8966e89, then renamed
// (namespace pt -> sif, P.T. strings -> SIF port). MIT License, Copyright (c) 2026 LoreanXavier;
// full text: repository/THIRD_PARTY.md.
#include "engine/core/log.h"
#include "engine/platform/os.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace sif {
namespace {

std::mutex g_log_mutex;
FILE* g_log_file = nullptr;
uint64_t g_tick = 0;
void (*g_observer)(LogLevel, std::string_view) = nullptr;
const bool g_log_ticks = [] {
    const char* value = std::getenv("SIF_LOG_TICKS");
    return value && *value == '1';
}();

const char* LevelTag(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
    }
    return "?";
}

}

void LogSetFile(const std::filesystem::path& path) {
    std::lock_guard lock(g_log_mutex);
    if (g_log_file) {
        std::fclose(g_log_file);
    }
    g_log_file = os::OpenFile(path, "w");
}

double LogClock() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration<double>(steady_clock::now() - start).count();
}

void LogWrite(LogLevel level, std::string_view text) {
    const double seconds = LogClock();
    std::lock_guard lock(g_log_mutex);
    char tick[32] = "";
    if (g_log_ticks) {
        std::snprintf(tick, sizeof(tick), "#%llu ", static_cast<unsigned long long>(g_tick));
    }
    std::fprintf(stderr, "[%9.3f] %-5s %s%.*s\n", seconds, LevelTag(level), tick, static_cast<int>(text.size()), text.data());
    if (g_log_file) {
        std::fprintf(g_log_file, "[%9.3f] %-5s %s%.*s\n", seconds, LevelTag(level), tick, static_cast<int>(text.size()), text.data());
        std::fflush(g_log_file);
    }
    if (g_observer) g_observer(level, text);
}

void LogSetObserver(void (*observer)(LogLevel, std::string_view)) {
    std::lock_guard lock(g_log_mutex);
    g_observer = observer;
}

bool LogTryWrite(LogLevel level, std::string_view text) {
    const double seconds = LogClock();
    std::unique_lock lock(g_log_mutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    if (g_log_file) {
        std::fprintf(g_log_file, "[%9.3f] %-5s %.*s\n", seconds, LevelTag(level), static_cast<int>(text.size()), text.data());
        std::fflush(g_log_file);
    }
    return true;
}

void LogSetTick(uint64_t tick) {
    std::lock_guard lock(g_log_mutex);
    g_tick = tick;
}

}
