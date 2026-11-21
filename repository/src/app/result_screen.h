#pragma once
// The result screen (ui2/95_result, layout uil0001_ResultTop00) for one finished live.

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "game/card/card.h"
#include "game/costume/costume.h"
#include "game/friendship/friendship.h"
#include "game/save/save.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

struct LiveResult {
    int song = -1, variant = 0;
    std::array<int, 5> counts{};  // Perfect, Great, Good, Bad, Miss
    int max_combo = 0;
    double score = 0;
    bool finished = false;  // the song played to its end (not quit)
    bool closed = false;    // the window was closed
    double clear_rate = 0;  // score / maximum score
    int rank = 0;           // 0 D, 1 C, 2 B, 3 A, 4 S, 5 SS, 6 SSS (the result atlas's order)
    std::string costume;    // the costume worn, "dNNN" ("" = the base model)
    // what the live gave (filled after the live, shown on the Items Obtained page)
    struct Skill {
        int card = 0, level_before = 0, level = 0;  // level_before 0 = a new card
    };
    std::vector<Skill> skills;                     // skill card drops (M11 S6)
    std::vector<std::pair<int, int>> materials;    // costume materials: item id, amount
    // Friendship (M14 F4): the played member's points before the live and the gain (member 0 = none)
    struct Friendship {
        int member = 0, before = 0, gained = 0;
    };
    Friendship friendship;
    // View Live Show (M12 P4c): the photos taken, RGBA8 at the window's size (saved by the caller)
    struct Photo {
        std::vector<uint8_t> rgba;
        uint32_t width = 0, height = 0;
        int camera_member = 0;  // the member the Member camera was on (M15: mission kind 33); 0 = another camera
    };
    std::vector<Photo> photos;
    std::map<int, double> camera_seconds;  // member -> seconds in the Member camera on her (M15: mission kind 26)
};

// The tables the Items Obtained page needs (card names and pictures, material icons, owned totals); null = not shown.
struct ResultItems {
    const card::Data* cards = nullptr;
    const costume::Data* costumes = nullptr;
    const save::SaveData* save = nullptr;  // after the live's gains
    const friendship::Tables* friendship = nullptr;  // M14: the Friendship panel's levels
};

// View Live Show's "End the Live Show" (M12 P4d; shadPS4 work/m12/shad2/v12-v13): the song, stage and member card,
// "Photos taken this time: n", the focused photo large and the five as thumbnails. Left/Right pick a photo, Square (Q)
// marks it as a favourite, Enter or Space ends. `favourites` gets one flag per photo; false if the window was closed.
// `stage`: the stage folder ("st017"); `member_card`: the played member's card id (its R picture is shown).
bool RunLiveViewEnd(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& r,
                    const std::string& stage, int member_card, long max_frames, std::vector<bool>& favourites);

// Shows the result until a key or pad button (or `max_frames`); false if the window was closed. `news`: which
// bests this play improved (the "NEW" marks). With `items`, a key first turns to the Items Obtained page (the skill
// cards and materials the live gave); Left/Right pick the card shown there, any other key ends.
bool RunResult(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const LiveResult& result,
               long max_frames, const save::NewBests& news = {}, const ResultItems* items = nullptr);

}
