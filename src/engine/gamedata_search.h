#pragma once

#include <string>
#include <vector>

// Finding the player's Keen 4 files without being told where they are (no
// --data or KEEN4_DATA_DIR): the current and executable folders, then the
// "Commander Keen" install in any Steam library on Windows, Linux and macOS.
namespace keen4::files {

// Folder holding the running executable, or empty if unknown.
std::string ExecutableDir();

// Steam install folders that may exist on this machine (not checked).
std::vector<std::string> DefaultSteamRoots();

// The "path" values of a steamapps/libraryfolders.vdf file.
std::vector<std::string> ParseLibraryFolders(const std::string& vdf);

// Existing Steam library folders: each root plus the libraries its
// libraryfolders.vdf lists, without duplicates.
std::vector<std::string> SteamLibraries(const std::vector<std::string>& roots);

// steamapps/common/Commander Keen/base4 inside a library, any case, or empty.
std::string KeenInLibrary(const std::string& library);

// Folder with KEEN4E.EXE: ".", "gamedata", "base4" under
// the current and executable folders, then the Steam libraries. Every folder
// tried is appended to `searched`. Returns empty if none has the game.
std::string LocateGameData(std::vector<std::string>* searched);

}  // namespace keen4::files
