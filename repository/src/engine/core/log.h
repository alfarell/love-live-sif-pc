// Copied from pt-pc (https://github.com/LoreanXavier/pt-pc) src/engine/core/log.h at commit 8966e89, then renamed
// (namespace pt -> sif, P.T. strings -> SIF port). MIT License, Copyright (c) 2026 LoreanXavier;
// full text: repository/THIRD_PARTY.md.
#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

namespace sif {

enum class LogLevel { Debug, Info, Warn, Error };

void LogWrite(LogLevel level, std::string_view text);
void LogSetFile(const std::filesystem::path& path);
bool LogTryWrite(LogLevel level, std::string_view text);
void LogSetTick(uint64_t tick);
double LogClock();

template <typename... Args>
void LogDebug(std::format_string<Args...> fmt, Args&&... args) {
    LogWrite(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogInfo(std::format_string<Args...> fmt, Args&&... args) {
    LogWrite(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogWarn(std::format_string<Args...> fmt, Args&&... args) {
    LogWrite(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogError(std::format_string<Args...> fmt, Args&&... args) {
    LogWrite(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
}

}
