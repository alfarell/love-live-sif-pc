#pragma once
// The profile card itself (M12 P6): ui2/40_collection uip0019_ProfileCardCustom00 filled from a save::ProfileCard,
// portrait (layout00) or landscape (layout01). Used by the Profile Card menu and the editor's "Completed Image".

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "app/top_menu.h"

namespace sif {

class Renderer;
class UiScreen;
struct Texture;
namespace friendship {
struct Tables;
}
namespace card {
struct Data;
}
namespace live {
struct SongInfo;
struct DressInfo;
}
namespace save {
struct ProfileCard;
struct SaveData;
}
class Messages;

// What the card shows, resolved to pictures and texts
struct ProfileCardArt {
    bool landscape = false;
    int frame_member = 1;          // the frame's member: pfc_frame[_h]_000_<member> (the 20 initial frames)
    std::string accolade_plate;    // texture paths ("" = the layout's own)
    std::string badge_plate, name_plate;
    std::string song, costume, badge_text, player, member;
    int level = 1;                 // the Friendship heart: the card member's Friendship level (M14)
    std::array<std::string, 3> skills;  // "Converting Threads Lv. 1", or "Skill not set."
};

// The tables the texts come from
struct ProfileCardTexts {
    const Nameplate* plate = nullptr;  // accolades, badges and the player's name
    const std::vector<live::SongInfo>* songs = nullptr;
    const std::vector<live::DressInfo>* dresses = nullptr;
    const card::Data* cards = nullptr;
    const Messages* messages = nullptr;
    int language = 1;
    const save::SaveData* save = nullptr;            // M14: the member's Friendship points ...
    const friendship::Tables* friendship = nullptr;  // ... and the levels; level 1 without them
};

// The heart shows the card member's Friendship level now (the eboot keeps it in the card's "favorability" when the card is
// made; shortcut: the port shows the current level)
ProfileCardArt MakeProfileCardArt(const save::ProfileCard& card, const ProfileCardTexts& texts);

// Fills the placed uip0019_ProfileCardCustom00 at `instance` (its other layout hidden). `photo` (optional) is a
// texture made by MakeCardPhoto for the same orientation.
void ApplyProfileCard(UiScreen& ui, const std::string& instance, const ProfileCardArt& art, const Texture* photo);

// A photo at a quarter of its size as a texture (every 4th pixel; shortcut: no filtering), for lists
bool MakeThumb(Renderer& renderer, const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, Texture& out);

// The card's photo as a texture: the 16:9 photo (rows top first, RGBA) shifted `crop` pixels to the left and made
// transparent outside the frame's window (the game's photo_stencil00/01; the port draws no stencils). `crop` is
// clamped so the window stays on the photo; the result says the value used.
int MakeCardPhoto(Renderer& renderer, const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, bool landscape,
                  int crop, Texture& out);

}
