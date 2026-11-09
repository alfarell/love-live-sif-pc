#include "app/first_run.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <mutex>
#include <system_error>

#include "engine/core/log.h"
#include "engine/fs/vfs.h"
#include "engine/platform/os.h"

namespace sif {

std::filesystem::path ResolveDumpFolder(const std::filesystem::path& chosen, std::string* hint) {
    std::string why;
    if (IsSifDump(chosen, &why)) return chosen;
    std::error_code ec;
    if (const auto sub = chosen / "CUSA24620"; std::filesystem::is_directory(sub, ec) && IsSifDump(sub)) return sub;
    bool pkg = false;
    for (const auto& e : std::filesystem::directory_iterator(chosen, ec)) {
        pkg |= e.is_regular_file(ec) && e.path().extension() == ".pkg";
    }
    if (hint) {
        if (pkg) {
            *hint = "This folder holds a .pkg package. The port needs the game as a folder: your own dump of the "
                    "game, the CUSA24620 folder with sce_sys, common, sound and live inside.";
        } else {
            *hint = "This is not a CUSA24620 game folder (" + why + "). Choose the folder that has sce_sys, common, "
                    "sound and live inside.";
        }
    }
    return {};
}

namespace {

// SDL's folder dialog reports from another thread on some systems; the caller waits for `done`
struct DialogResult {
    std::mutex lock;
    std::string folder;
    std::atomic<bool> done{false};
};

void OnFolder(void* user, const char* const* list, int /*filter*/) {
    auto* r = static_cast<DialogResult*>(user);
    {
        std::lock_guard<std::mutex> g(r->lock);
        if (!list) {
            LogWarn("first run: folder dialog: {}", SDL_GetError());
        } else if (list[0]) {
            r->folder = list[0];  // empty list: cancelled
        }
    }
    r->done = true;
}

std::string PickFolder(const std::string& start) {
    DialogResult result;
    SDL_ShowOpenFolderDialog(&OnFolder, &result, nullptr, start.empty() ? nullptr : start.c_str(), false);
    while (!result.done) {
        SDL_Event e;
        SDL_WaitEventTimeout(&e, 50);  // keeps SDL's event loop running while the dialog is open
    }
    std::lock_guard<std::mutex> g(result.lock);
    return result.folder;
}

}

std::string ChooseGameFolder(const std::string& start_folder, std::string problem) {
    std::string start = start_folder;
    for (;;) {
        const std::string text = problem +
                                 "\n\nThe port reads the game from your own dump of Love Live! School Idol Festival "
                                 "(CUSA24620). Choose that folder; it is remembered in settings.ini.";
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose folder..."},
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
        };
        const SDL_MessageBoxData box{SDL_MESSAGEBOX_INFORMATION, nullptr, "SIF port: game folder", text.c_str(), 2,
                                     buttons, nullptr};
        int pressed = 0;
        if (!SDL_ShowMessageBox(&box, &pressed) || pressed != 1) return {};
        const std::string chosen = PickFolder(start);
        if (chosen.empty()) continue;  // cancelled: back to the explanation
        start = chosen;
        std::string hint;
        const auto dump = ResolveDumpFolder(os::PathFromUtf8(chosen), &hint);
        if (!dump.empty()) {
            LogInfo("first run: game folder chosen: {}", os::PathToUtf8(dump));
            return os::PathToUtf8(dump);
        }
        LogWarn("first run: {} is not a dump: {}", chosen, hint);
        problem = "That folder cannot be used:\n" + chosen + "\n\n" + hint;
    }
}

}
