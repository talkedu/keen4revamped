#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Locating the player's game files. Releases differ in file-name case
// (Steam ships "Egagraph.ck4", DOS copies "EGAGRAPH.CK4"), so lookups ignore case.
namespace keen4::files {

void SetDataDir(const std::string& dir);
const std::string& DataDir();

// Full path of `name` inside the data directory, matched case-insensitively.
// Returns an empty string if there is no such file.
std::string Find(const std::string& name);

// Reads a whole data file. Returns false if it does not exist.
bool ReadAll(const std::string& name, std::vector<std::uint8_t>* out);

// Directory for files the game writes (config, saved games, high scores).
void SetWriteDir(const std::string& dir);
std::string WritePath(const std::string& name);

}  // namespace keen4::files
