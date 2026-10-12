#include "engine/datafiles.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <system_error>

namespace keen4::files {

namespace {

std::string g_data_dir = ".";
std::string g_write_dir = ".";

bool EqualNoCase(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

}  // namespace

void SetDataDir(const std::string& dir) { g_data_dir = dir.empty() ? "." : dir; }
const std::string& DataDir() { return g_data_dir; }

std::string Find(const std::string& name) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path exact = fs::path(g_data_dir) / name;
  if (fs::is_regular_file(exact, ec)) return exact.string();
  for (const auto& entry : fs::directory_iterator(g_data_dir, ec)) {
    if (entry.is_regular_file(ec) && EqualNoCase(entry.path().filename().string(), name))
      return entry.path().string();
  }
  return {};
}

bool ReadAll(const std::string& name, std::vector<std::uint8_t>* out) {
  std::string path = Find(name);
  if (path.empty()) return false;
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  out->clear();
  std::uint8_t chunk[65536];
  std::size_t n;
  while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) out->insert(out->end(), chunk, chunk + n);
  std::fclose(f);
  return true;
}

void SetWriteDir(const std::string& dir) {
  g_write_dir = dir.empty() ? "." : dir;
  std::error_code ec;
  std::filesystem::create_directories(g_write_dir, ec);
}

std::string WritePath(const std::string& name) {
  return (std::filesystem::path(g_write_dir) / name).string();
}

}  // namespace keen4::files
