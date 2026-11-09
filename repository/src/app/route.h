#pragma once
// Routes (M9): scripted walkthroughs that play the port without a person, in the style of pt-pc's
// tests/walkthrough. `sif.exe --route FILE` plays one; tools/routes.py runs them all. Keys are injected through
// sif::PollEvent (no OS input). One command per line, '#' starts a comment:
//   wait 500ms | wait 2s          pause
//   key NAME                      press and release: enter esc left right up down pageup pagedown t space a s d f j k l ;
//   press NAME until SCREEN [20s] press the key once a second until that screen is shown, else fail (screens that
//                                 load at their own pace, e.g. the logos before the title)
//   expect screen NAME [10s]      wait until that screen is shown (title menu select live result costumes materials
//                                 missions profile options controls calibrate), else fail
//   expect log TEXT               wait until a log line contains TEXT (since the route started), else fail
//   expect nolog TEXT             fail if a log line so far contains TEXT
//   shot NAME                     save the window's picture as <shots>/NAME.bmp
//   timeout 120s                  the whole route's limit (default 180 s)
//   quit                          end here (pass)
// The route ends the app: exit code 0 = pass, 3 = fail; sif.log gets "route: PASS" or "route: FAIL line N: ...".

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

namespace sif::route {

struct Command {
    int line = 0;
    std::string op;     // wait key press expect shot timeout quit
    std::string what;   // key name; expect kind (screen log nolog); shot name
    std::string text;   // expect argument
    double ms = 0;      // wait / timeout / expect limit
};

bool Parse(std::string_view script, std::vector<Command>& out, std::string* error);

// Starts playing `file` (log lines from now on count for "expect log"); shots go to `shots`.
bool Start(const std::filesystem::path& file, SDL_Window* window, const std::filesystem::path& shots, std::string* error);
bool Active();
// Called by sif::PollEvent before SDL's queue: the next scripted event when one is due.
bool Inject(SDL_Event& e);
// Screens report themselves (for "expect screen"); a no-op without a route.
void SetScreen(std::string_view name);
// The app's exit code when a route ran: 0 pass, 3 fail (also when the app ended before the route did); else `code`.
int ExitCode(int code);

}
