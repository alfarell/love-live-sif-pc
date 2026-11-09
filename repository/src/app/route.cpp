#include "app/route.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#include "engine/core/log.h"
#include "engine/platform/os.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace sif::route {
namespace {

const std::map<std::string, SDL_Scancode>& Keys() {
    static const std::map<std::string, SDL_Scancode> keys = {
        {"enter", SDL_SCANCODE_RETURN}, {"esc", SDL_SCANCODE_ESCAPE},       {"left", SDL_SCANCODE_LEFT},
        {"right", SDL_SCANCODE_RIGHT},  {"up", SDL_SCANCODE_UP},            {"down", SDL_SCANCODE_DOWN},
        {"pageup", SDL_SCANCODE_PAGEUP}, {"pagedown", SDL_SCANCODE_PAGEDOWN}, {"t", SDL_SCANCODE_T},
        {"space", SDL_SCANCODE_SPACE},  {"a", SDL_SCANCODE_A},              {"s", SDL_SCANCODE_S},
        {"d", SDL_SCANCODE_D},          {"f", SDL_SCANCODE_F},              {"j", SDL_SCANCODE_J},
        {"k", SDL_SCANCODE_K},          {"l", SDL_SCANCODE_L},              {";", SDL_SCANCODE_SEMICOLON},
    };
    return keys;
}

bool Duration(const std::string& word, double& ms) {
    char* end = nullptr;
    const double v = std::strtod(word.c_str(), &end);
    const std::string unit = end ? end : "";
    if (end == word.c_str() || v < 0) return false;
    if (unit == "ms") ms = v;
    else if (unit == "s") ms = v * 1000;
    else return false;
    return true;
}

struct State {
    std::vector<Command> cmds;
    size_t next = 0;
    bool active = false, done = false, failed = false;
    uint64_t start_ns = 0, step_ns = 0, last_quit_ns = 0, up_at = 0, press_at = 0;
    double limit_ms = 180000;
    SDL_Scancode up = SDL_SCANCODE_UNKNOWN;
    std::string screen;
    SDL_Window* window = nullptr;
    std::filesystem::path shots;
    std::mutex lines_lock;
    std::vector<std::string> lines;
};

State& S() {
    static State s;
    return s;
}

void Observe(LogLevel, std::string_view text) {
    State& s = S();
    std::lock_guard g(s.lines_lock);
    s.lines.emplace_back(text);
}

bool Logged(const std::string& text) {
    State& s = S();
    std::lock_guard g(s.lines_lock);
    return std::any_of(s.lines.begin(), s.lines.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; });
}

void Fail(const std::string& why) {
    State& s = S();
    const int line = s.next < s.cmds.size() ? s.cmds[s.next].line : 0;
    s.failed = s.done = true;
    LogError("route: FAIL line {}: {}", line, why);
}

void KeyEvent(SDL_Event& e, SDL_Scancode code, bool down, uint64_t now) {
    e = {};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.timestamp = now;
    e.key.windowID = S().window ? SDL_GetWindowID(S().window) : 0;
    e.key.scancode = code;
    e.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
    e.key.down = down;
}

bool Shot(const std::string& name) {
#ifdef _WIN32
    State& s = S();
    const auto hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(s.window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    RECT r{};
    if (!hwnd || !GetClientRect(hwnd, &r) || r.right <= 0 || r.bottom <= 0) return false;
    const int w = r.right, h = r.bottom;
    HDC window_dc = GetDC(hwnd);
    HDC dc = CreateCompatibleDC(window_dc);
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (bmp) {
        HGDIOBJ old = SelectObject(dc, bmp);
        ok = PrintWindow(hwnd, dc, 0x1 | 0x2) != 0;  // PW_CLIENTONLY | PW_RENDERFULLCONTENT: this window only
        if (ok) {
            std::error_code ec;
            std::filesystem::create_directories(s.shots, ec);
            SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_XRGB8888, bits, w * 4);
            ok = surface && SDL_SaveBMP(surface, os::PathToUtf8(s.shots / (name + ".bmp")).c_str());
            SDL_DestroySurface(surface);
        }
        SelectObject(dc, old);
        DeleteObject(bmp);
    }
    DeleteDC(dc);
    ReleaseDC(hwnd, window_dc);
    return ok;
#else
    (void)name;
    return false;
#endif
}

}  // namespace

bool Parse(std::string_view script, std::vector<Command>& out, std::string* error) {
    out.clear();
    std::istringstream in{std::string(script)};
    std::string raw;
    for (int line = 1; std::getline(in, raw); ++line) {
        if (const size_t hash = raw.find('#'); hash != std::string::npos) raw.resize(hash);
        while (!raw.empty() && std::isspace(static_cast<unsigned char>(raw.back()))) raw.pop_back();
        std::istringstream words(raw);
        Command c;
        c.line = line;
        if (!(words >> c.op)) continue;
        const auto bad = [&](const std::string& why) {
            if (error) *error = "line " + std::to_string(line) + ": " + why;
            return false;
        };
        std::string rest;
        std::getline(words >> std::ws, rest);
        std::istringstream args(rest);
        if (c.op == "wait" || c.op == "timeout") {
            std::string d, extra;
            if (!(args >> d) || !Duration(d, c.ms) || (args >> extra)) return bad(c.op + " needs a duration like 500ms or 2s");
        } else if (c.op == "key") {
            if (!(args >> c.what) || !Keys().contains(c.what)) return bad("unknown key '" + c.what + "'");
        } else if (c.op == "press") {
            std::string until, d;
            c.ms = 20000;
            if (!(args >> c.what) || !Keys().contains(c.what)) return bad("unknown key '" + c.what + "'");
            if (!(args >> until) || until != "until" || !(args >> c.text)) return bad("press needs: press KEY until SCREEN");
            if (args >> d && !Duration(d, c.ms)) return bad("press: bad time limit '" + d + "'");
        } else if (c.op == "expect") {
            if (!(args >> c.what)) return bad("expect needs screen, log or nolog");
            c.ms = 15000;
            if (c.what == "screen") {
                std::string d;
                if (!(args >> c.text)) return bad("expect screen needs a name");
                if (args >> d && !Duration(d, c.ms)) return bad("expect screen: bad time limit '" + d + "'");
            } else if (c.what == "log" || c.what == "nolog") {
                std::getline(args >> std::ws, c.text);
                if (c.text.empty()) return bad("expect " + c.what + " needs a text");
            } else {
                return bad("expect " + c.what + ": unknown check");
            }
        } else if (c.op == "shot") {
            if (!(args >> c.what)) return bad("shot needs a name");
        } else if (c.op != "quit") {
            return bad("unknown command '" + c.op + "'");
        }
        out.push_back(std::move(c));
    }
    if (out.empty() && error) *error = "the route is empty";
    return !out.empty();
}

bool Start(const std::filesystem::path& file, SDL_Window* window, const std::filesystem::path& shots, std::string* error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot read " + os::PathToUtf8(file);
        return false;
    }
    std::stringstream text;
    text << in.rdbuf();
    State& s = S();
    if (!Parse(text.str(), s.cmds, error)) return false;
    s.window = window;
    s.shots = shots;
    s.start_ns = SDL_GetTicksNS();
    s.active = true;
    LogSetObserver(&Observe);
    LogInfo("route: {} ({} commands)", os::PathToUtf8(file), s.cmds.size());
    return true;
}

bool Active() { return S().active; }

void SetScreen(std::string_view name) {
    State& s = S();
    if (!s.active) return;
    s.screen = name;
    LogInfo("route: screen {}", name);
}

bool Inject(SDL_Event& e) {
    State& s = S();
    if (!s.active) return false;
    const uint64_t now = SDL_GetTicksNS();
    if (s.up != SDL_SCANCODE_UNKNOWN && now >= s.up_at) {  // the release of the last key
        KeyEvent(e, s.up, false, now);
        s.up = SDL_SCANCODE_UNKNOWN;
        return true;
    }
    if (s.done) {  // ask the app to close, at most every 100 ms so a screen draining events is not held
        if (now - s.last_quit_ns < 100'000'000) return false;
        s.last_quit_ns = now;
        e = {};
        e.type = SDL_EVENT_QUIT;
        e.quit.timestamp = now;
        return true;
    }
    if (double(now - s.start_ns) / 1e6 > s.limit_ms) {
        Fail("the route took longer than " + std::to_string(int(s.limit_ms / 1000)) + " s");
        return false;
    }
    while (s.next < s.cmds.size()) {
        const Command& c = s.cmds[s.next];
        if (!s.step_ns) s.step_ns = now;
        const double step_ms = double(now - s.step_ns) / 1e6;
        const auto advance = [&] {
            ++s.next;
            s.step_ns = 0;
        };
        if (c.op == "wait") {
            if (step_ms < c.ms) return false;
            advance();
        } else if (c.op == "key") {
            if (s.up != SDL_SCANCODE_UNKNOWN) return false;  // the previous key is still down
            const SDL_Scancode code = Keys().at(c.what);
            KeyEvent(e, code, true, now);
            s.up = code;
            s.up_at = now + 60'000'000;
            advance();
            return true;
        } else if (c.op == "press") {
            if (s.screen == c.text) {
                advance();
                continue;
            }
            if (step_ms > c.ms) {
                Fail("pressed " + c.what + " but screen " + c.text + " did not come (still " + s.screen + ")");
                return false;
            }
            if (s.up != SDL_SCANCODE_UNKNOWN || now < s.press_at) return false;
            const SDL_Scancode code = Keys().at(c.what);
            KeyEvent(e, code, true, now);
            s.up = code;
            s.up_at = now + 60'000'000;
            s.press_at = now + 1'000'000'000;
            return true;
        } else if (c.op == "expect" && c.what == "screen") {
            if (s.screen != c.text) {
                if (step_ms > c.ms) Fail("expected screen " + c.text + ", still " + (s.screen.empty() ? "none" : s.screen));
                return false;
            }
            advance();
        } else if (c.op == "expect" && c.what == "log") {
            if (!Logged(c.text)) {
                if (step_ms > c.ms) Fail("no log line with '" + c.text + "'");
                return false;
            }
            advance();
        } else if (c.op == "expect" && c.what == "nolog") {
            if (Logged(c.text)) {
                Fail("a log line has '" + c.text + "'");
                return false;
            }
            advance();
        } else if (c.op == "shot") {
            if (!Shot(c.what)) LogWarn("route: shot {} failed", c.what);
            advance();
        } else if (c.op == "timeout") {
            s.limit_ms = c.ms;
            advance();
        } else {  // quit
            s.next = s.cmds.size();
        }
    }
    if (!s.done) {
        s.done = true;
        LogInfo("route: PASS ({:.1f} s)", double(now - s.start_ns) / 1e9);
    }
    return false;
}

int ExitCode(int code) {
    State& s = S();
    if (!s.active) return code;
    // the app may end on its own (e.g. calibration): the remaining log checks are still decided, anything else fails
    for (; !s.done && s.next < s.cmds.size(); ++s.next) {
        const Command& c = s.cmds[s.next];
        if (c.op == "wait" || c.op == "shot" || c.op == "timeout") continue;
        if (c.op == "quit") break;
        if (c.op == "expect" && c.what == "log" && !Logged(c.text)) Fail("no log line with '" + c.text + "'");
        else if (c.op == "expect" && c.what == "nolog" && Logged(c.text)) Fail("a log line has '" + c.text + "'");
        else if (c.op != "expect" || c.what == "screen") Fail("the app ended before the route finished");
    }
    if (!s.done) {
        s.done = true;
        LogInfo("route: PASS (the app ended; the remaining checks hold)");
    }
    LogSetObserver(nullptr);
    return s.failed ? 3 : 0;
}

}  // namespace sif::route
