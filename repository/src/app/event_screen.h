#pragma once
// Event -> Ongoing Events (M14 F6): the birthday events (ui2/50_event). "Events being held": a plate per member of the
// current mode with her event's dates and banner; Enter opens her page: the status, the points gauge (1000 to reach)
// and the rewards, with "Receive rewards" once reached. shadPS4 work/m14/shad/e7.png, e8.png.

#include <string>

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;
namespace card {
struct Data;
}
namespace costume {
struct Data;
}
namespace event {
struct Data;
}
namespace save {
struct SaveData;
}

struct EventSetup {
    const event::Data* events = nullptr;
    save::SaveData* save = nullptr;           // the event states; the rewards go into it
    const card::Data* cards = nullptr;        // the reward skill cards (null: not given)
    const costume::Data* costumes = nullptr;  // the key-item icons
};

// Shows the events until Esc; false if the window was closed.
bool RunBirthdayEvents(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                       const EventSetup& setup, long max_frames);

}
