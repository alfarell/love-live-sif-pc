#pragma once
// Skill card screens (M11 S4, analysis/202611151530-skills-and-cards/): Skill Card Selection (the pop-up of Prepare for
// the Live Show, ui2/40_collection uil0002_PopColl00) and Collection -> Skill Card (uil0001_CollLayout00 with
// uil0012_GallerySkillWin00), as captured in shadPS4 (S2: work/m11/shad/s07, s08, s14-s17).

#include <string>

#include "game/card/card.h"
#include "game/save/save.h"

struct SDL_Window;

namespace sif {

class Renderer;
class UiScreen;
class Vfs;
class Messages;

// What the skill screens need: the card tables, the save (owned copies and the set slots) and the played member,
// whose three slots the live uses (S2: the member being played; the port plays the navigator, member 1 unless --member).
struct SkillSetup {
    const card::Data* cards = nullptr;
    save::SaveData* save = nullptr;
    int member = 1;
};

// The skill card picture: "ui/ui_skill_card_thumbnail[_s][_<lang>].pac/texture.pac/skc[_s]_<id>".
std::string SkillThumbPath(int card, const std::string& lang, bool small);
// The three "Skill Card in Use" rows of a uip0001_SkillList00 / uip0002_SkillList01 instance: name and "Lv.N" on a
// plate coloured by the card's series, or "Skill not set." on the grey bar of the background.
void ApplySkillRows(UiScreen& ui, const std::string& instance, const SkillSetup& setup, const Messages& messages, int language);

// Skill Card Selection: the member's owned cards and the three slots. Space on a slot moves to the list, Space on a
// card sets it in that slot; R1 (Page Down) removes the focused slot's card; OK or Esc returns. Changes go straight
// into setup.save->skill_slots. Returns false when the window was closed.
bool RunSkillSelect(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const SkillSetup& setup,
                    long max_frames);

// Collection -> Skill Card: one tab per member of the group, every card of that member (owned ones face up, the
// others as card backs) and the focused card's Skill Info. L1/R1 change the member, L2 (shortcut: Tab) the group.
bool RunSkillCollection(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang,
                        const SkillSetup& setup, long max_frames);

}
