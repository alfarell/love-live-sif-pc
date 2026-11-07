#pragma once
// The Task List (ui2/50_event, layout uil0001_MisLayout00): the missions with their progress from the save.

#include <string>
#include <vector>

#include "game/dlc/dlc.h"
#include "game/live/song.h"
#include "game/mission/mission.h"
#include "game/save/save.h"

struct SDL_Window;

namespace sif {

class Renderer;
class Vfs;

struct MissionData {
    const mission::Missions* missions = nullptr;
    const std::vector<mission::Award>* accolades = nullptr;
    const std::vector<live::SongInfo>* songs = nullptr;  // titles for [p1], in the UI language
    const save::SaveData* save = nullptr;
    const dlc::Ownership* owned = nullptr;  // songs the user has no add-on for are left out
};

// Shows the Task List until Esc; false if the window was closed.
bool RunMissions(SDL_Window* window, Renderer& renderer, const Vfs& vfs, const std::string& lang, const MissionData& data,
                 long max_frames);

}
