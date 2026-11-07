// Trim, Parse and Read are copied from pt-pc (https://github.com/LoreanXavier/pt-pc) src/engine/platform/settings.cpp
// at commit 8966e89 (namespace pt -> sif, pt::AsciiLower -> std::tolower). MIT License, Copyright (c) 2026
// LoreanXavier; see THIRD_PARTY.md. The SIF settings keys below are new.
#include "engine/platform/settings.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>

#include "engine/core/log.h"

namespace sif {
namespace {

using Values = std::map<std::string, std::string>;

std::string Trim(const std::string& text) {
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

Values Parse(std::istream& in) {
    Values values;
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string value = Trim(line.substr(eq + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        values[section + "." + Trim(line.substr(0, eq))] = value;
    }
    return values;
}

void Read(const Values& values, const char* key, int& out) {
    if (auto it = values.find(key); it != values.end()) {
        try {
            out = std::stoi(it->second);
        } catch (...) {
            LogWarn("settings: {} = '{}' is not a number", key, it->second);
        }
    }
}

void Read(const Values& values, const char* key, float& out) {
    if (auto it = values.find(key); it != values.end()) {
        try {
            out = std::stof(it->second);
        } catch (...) {
            LogWarn("settings: {} = '{}' is not a number", key, it->second);
        }
    }
}

void Read(const Values& values, const char* key, bool& out) {
    if (auto it = values.find(key); it != values.end()) {
        std::string v = it->second;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        out = v == "1" || v == "true" || v == "yes" || v == "on";
    }
}

void Read(const Values& values, const char* key, std::string& out) {
    if (auto it = values.find(key); it != values.end()) {
        out = it->second;
    }
}

}

bool LoadAppSettings(const std::filesystem::path& path, AppSettings& out) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    const Values v = Parse(in);
    Read(v, "display.width", out.display.width);
    Read(v, "display.height", out.display.height);
    Read(v, "display.fullscreen", out.display.fullscreen);
    Read(v, "display.vsync", out.display.vsync);
    Read(v, "audio.volume", out.audio.volume);
    Read(v, "audio.output_device", out.audio.output_device);
    Read(v, "audio.buffer_frames", out.audio.buffer_frames);
    Read(v, "game.folder", out.game.folder);
    Read(v, "game.dlc_folder", out.game.dlc_folder);
    Read(v, "timing.audio_offset_ms", out.timing.audio_offset_ms);
    Read(v, "timing.input_offset_ms", out.timing.input_offset_ms);
    Read(v, "play.approach_ms", out.play.approach_ms);
    out.play.approach_ms = std::clamp(out.play.approach_ms, 400, 3000);
    out.display.width = std::clamp(out.display.width, 320, 16384);
    out.display.height = std::clamp(out.display.height, 240, 16384);
    out.audio.volume = std::clamp(out.audio.volume, 0.0f, 1.0f);
    out.audio.buffer_frames = std::clamp(out.audio.buffer_frames, 32, 8192);
    return true;
}

bool SaveAppSettings(const std::filesystem::path& path, const AppSettings& s) {
    std::ofstream f(path, std::ios::trunc);
    if (!f) {
        return false;
    }
    f << "[display]\nwidth = " << s.display.width << "\nheight = " << s.display.height
      << "\nfullscreen = " << s.display.fullscreen << "\nvsync = " << (s.display.vsync ? 1 : 0) << "\n\n";
    f << "[audio]\nvolume = " << s.audio.volume << "\noutput_device = \"" << s.audio.output_device
      << "\"\nbuffer_frames = " << s.audio.buffer_frames << "\n\n";
    f << "[game]\nfolder = \"" << s.game.folder << "\"\ndlc_folder = \"" << s.game.dlc_folder << "\"\n\n";
    f << "[timing]\naudio_offset_ms = " << s.timing.audio_offset_ms << "\ninput_offset_ms = " << s.timing.input_offset_ms
      << "\n\n";
    f << "[play]\napproach_ms = " << s.play.approach_ms << "\n";
    return static_cast<bool>(f);
}

}
