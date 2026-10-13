#pragma once

#include <string>

// Local side of the in-game updater: unpacking a release archive and putting
// its files in place of the running game's.
//
// The game folder may be a portable copy, the Linux installer's
// ~/.local/lib/keen4revamped or the Windows installer's Program Files folder.
// Files are swapped by renaming, which works while the game runs; on Windows
// a folder the player cannot write is updated by a copy of the game started
// with administrator rights (keen4 --apply-update FROM TO).
namespace keen4::update {

// Call first in main(): remembers how the game was started and removes what
// the last update left behind.
void Init(int argc, char** argv);

// The elevated helper. True if argv asked for it; *exit_code is then the
// process's exit code.
bool RunHelper(int argc, char** argv, int* exit_code);

std::string InstallDir();

// Unpacks a .zip (Windows) or .tar.gz archive into `dir` and finds the folder
// in it that holds the game's executable.
bool Extract(const std::string& archive, const std::string& dir, std::string* root, std::string* error);

enum class Apply { Done, Declined, Failed };
// Copies the files under `root` over the game folder.
Apply ApplyUpdate(const std::string& root, std::string* error);

// Starts the (updated) game again once this process exits.
void RelaunchOnExit();

}  // namespace keen4::update
