#pragma once
// The screen before a played live (M12 P7; ui2/90_ingame uil0010_LiveStart00; shadPS4 work/m12/shad/intro/): the song
// title, the player's panel (member card, three skill cards, nameplate) and "Profile Card Support" with the two NPC
// profile cards (profile::PickSupport).

#include <array>
#include <string>

#include "app/top_menu.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;
class Messages;
namespace profile {
struct Preset;
}

struct LiveIntro {
    std::string song;
    int member_card = 0;                 // the played member's card in her costume (mbc_<id>)
    std::array<int, 3> skills{}, levels{};  // her set skill cards (0 = empty)
    const Nameplate* plate = nullptr;
    std::array<const profile::Preset*, 2> support{};  // the NPC cards (null = none)
    const Messages* messages = nullptr;
    int language = 1;
    std::string lang;
};

// Shown for kLiveIntroMs, or until Enter or Space; false if the window was closed.
constexpr double kLiveIntroMs = 4000;
bool RunLiveIntro(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const LiveIntro& intro, long max_frames);

}
