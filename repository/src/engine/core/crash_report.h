// Copied from pt-pc (https://github.com/LoreanXavier/pt-pc) src/engine/core/crash_report.h at commit 8966e89, then renamed
// (namespace pt -> sif, P.T. strings -> SIF port). MIT License, Copyright (c) 2026 LoreanXavier;
// full text: repository/THIRD_PARTY.md.
#pragma once

#include <filesystem>
#include <string>

namespace sif {

void InstallCrashReporting(const std::filesystem::path& dump_dir, const std::string& build);

std::filesystem::path CrashDumpDir();
const std::string& CrashBuildName();

std::filesystem::path WriteCrashDump(const char* reason, void* exception_pointers = nullptr);
std::filesystem::path WriteCrashDumpFor(const char* reason, void* exception_pointers, unsigned long thread_id);

void SetCrashNote(const std::string& note);

[[noreturn]] void FatalError(const std::string& reason, bool show_message, int code = 3);

std::filesystem::path WriteHangDump(const char* reason);

void CheckCrashFilter(const char* after);

void ReserveCrashStack();

bool ExitLogged();

void LogExit(int code, const char* how);

}
