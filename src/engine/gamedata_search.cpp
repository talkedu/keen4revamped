#include "engine/gamedata_search.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

#include "engine/datafiles.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace keen4::files {

namespace fs = std::filesystem;

namespace {

std::string Env(const char* name) {
  const char* value = std::getenv(name);
  return value ? value : "";
}

#ifdef _WIN32
// A string value from the registry, or empty.
std::string RegistryString(HKEY root, const wchar_t* key, const wchar_t* value) {
  wchar_t buffer[1024];
  DWORD size = sizeof buffer;
  if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) return {};
  return fs::path(buffer).string();
}
#endif

bool HasGame(const std::string& dir) { return !dir.empty() && !FindIn(dir, "KEEN4E.EXE").empty(); }

}  // namespace

std::string ExecutableDir() {
  std::error_code ec;
#ifdef _WIN32
  wchar_t buffer[MAX_PATH * 4];
  DWORD n = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
  if (n == 0 || n >= std::size(buffer)) return {};
  return fs::path(buffer).parent_path().string();
#elif defined(__APPLE__)
  char buffer[4096];
  std::uint32_t size = sizeof buffer;
  if (_NSGetExecutablePath(buffer, &size) != 0) return {};
  fs::path path = fs::weakly_canonical(buffer, ec);
  return ec ? fs::path(buffer).parent_path().string() : path.parent_path().string();
#else
  fs::path path = fs::read_symlink("/proc/self/exe", ec);
  return ec ? std::string() : path.parent_path().string();
#endif
}

std::vector<std::string> DefaultSteamRoots() {
  std::vector<std::string> roots;
#ifdef _WIN32
  // SteamPath is per user and uses forward slashes; InstallPath is the machine-wide one.
  roots.push_back(RegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"));
  roots.push_back(RegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath"));
  roots.push_back(RegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath"));
  for (const char* var : {"ProgramFiles(x86)", "ProgramFiles"}) {
    std::string dir = Env(var);
    if (!dir.empty()) roots.push_back((fs::path(dir) / "Steam").string());
  }
  roots.push_back("C:\\Program Files (x86)\\Steam");
#else
  std::string home = Env("HOME");
#ifdef __APPLE__
  if (!home.empty()) roots.push_back(home + "/Library/Application Support/Steam");
#else
  std::string xdg = Env("XDG_DATA_HOME");
  if (!xdg.empty()) roots.push_back(xdg + "/Steam");
  if (!home.empty()) {
    roots.push_back(home + "/.steam/steam");
    roots.push_back(home + "/.steam/root");
    roots.push_back(home + "/.local/share/Steam");
    roots.push_back(home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam");  // Flatpak
    roots.push_back(home + "/snap/steam/common/.local/share/Steam");                // Snap
  }
#endif
#endif
  std::vector<std::string> nonempty;
  for (const std::string& root : roots) {
    if (!root.empty()) nonempty.push_back(root);
  }
  return nonempty;
}

std::vector<std::string> ParseLibraryFolders(const std::string& vdf) {
  // VDF is nested "key" "value" / "key" { ... } pairs; library entries carry
  // "path" "D:\\SteamLibrary" with backslashes escaped.
  std::vector<std::string> tokens;
  for (std::size_t i = 0; i < vdf.size(); i++) {
    if (vdf[i] != '"') continue;
    std::string token;
    for (i++; i < vdf.size() && vdf[i] != '"'; i++) {
      if (vdf[i] == '\\' && i + 1 < vdf.size()) i++;
      token += vdf[i];
    }
    tokens.push_back(token);
  }
  std::vector<std::string> paths;
  for (std::size_t i = 0; i + 1 < tokens.size(); i++) {
    if (tokens[i] == "path") paths.push_back(tokens[++i]);
  }
  return paths;
}

std::vector<std::string> SteamLibraries(const std::vector<std::string>& roots) {
  std::vector<std::string> libraries;
  std::vector<fs::path> seen;
  auto add = [&](const std::string& dir) {
    std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec)) return;
    fs::path canonical = fs::weakly_canonical(dir, ec);
    if (ec) canonical = dir;
    for (const fs::path& p : seen) {
      if (p == canonical) return;
    }
    seen.push_back(canonical);
    libraries.push_back(dir);
  };
  for (const std::string& root : roots) {
    add(root);
    for (const char* file : {"steamapps/libraryfolders.vdf", "config/libraryfolders.vdf"}) {
      std::ifstream in(fs::path(root) / file, std::ios::binary);
      if (!in) continue;
      std::stringstream text;
      text << in.rdbuf();
      for (const std::string& library : ParseLibraryFolders(text.str())) add(library);
    }
  }
  return libraries;
}

std::string KeenInLibrary(const std::string& library) {
  std::string dir = library;
  for (const char* part : {"steamapps", "common", "Commander Keen", "base4"}) {
    dir = FindIn(dir, part, true);
    if (dir.empty()) return {};
  }
  return dir;
}

std::string LocateGameData(std::vector<std::string>* searched) {
  auto attempt = [&](const std::string& dir) {
    if (dir.empty()) return false;
    searched->push_back(dir);
    return HasGame(dir);
  };
  std::vector<std::string> bases = {"."};
  std::string exe = ExecutableDir();
  std::error_code ec;
  if (!exe.empty() && !fs::equivalent(exe, ".", ec)) bases.push_back(exe);
  for (const std::string& base : bases) {
    for (const char* sub : {"", "gamedata", "base4"}) {
      std::string dir = *sub ? (fs::path(base) / sub).string() : base;
      if (attempt(dir)) return dir;
    }
  }

  for (const std::string& library : SteamLibraries(DefaultSteamRoots())) {
    std::string dir = KeenInLibrary(library);
    if (dir.empty()) {
      searched->push_back((fs::path(library) / "steamapps/common/Commander Keen/base4").string());
      continue;
    }
    if (attempt(dir)) return dir;
  }
  return {};
}

}  // namespace keen4::files
