#pragma once
// First run (M8 V5): finding the user's own CUSA24620 dump. The port only reads a dump the user made from their own
// copy; nothing here obtains or unpacks games.

#include <filesystem>
#include <string>

namespace sif {

// The dump folder for a folder the user chose: the folder itself if it is a CUSA24620 dump, else its CUSA24620
// subfolder if that is one (the user picked the folder above it). Empty when neither is; `hint` then says what is
// wrong in words for the user (e.g. a folder holding only the .pkg package).
std::filesystem::path ResolveDumpFolder(const std::filesystem::path& chosen, std::string* hint);

// Explains `problem` in a message box and lets the user pick the dump folder with the system's folder dialog,
// repeating until a dump is chosen or the user quits. Needs SDL's video subsystem. Returns the dump folder (UTF-8),
// or "" if the user quit.
std::string ChooseGameFolder(const std::string& start_folder, std::string problem);

}
