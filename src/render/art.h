#pragma once

// Art for the remaster renderer.
//
// New art is found through a manifest keyed by the original graphics numbers
// (see docs/ART.md). Anything the manifest does not list, or flags as bad,
// falls back to the original image scaled up 4x, so the renderer always has a
// picture for every tile and sprite.

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace keen4::render {

enum class AssetKind { Tile16, Tile16M, Sprite };

// An RGBA image (0xAARRGGBB). It stands for an original asset of origw x origh
// pixels, drawn offset by (ox, oy) original pixels from the original's top-left.
struct ArtImage {
  int width = 0, height = 0;
  std::vector<std::uint32_t> pixels;
  int origw = 0, origh = 0;
  float ox = 0, oy = 0;
  bool fromManifest = false;
  std::uint32_t revision = 0;  // changes when a live asset is rebuilt
};

enum class Fallback { Scale4x, Nearest };

// The original 16 EGA colours in the default palette, as 0xRRGGBB.
std::uint32_t EgaColor(int index);

// Decodes an original asset to palette indices (16 = transparent).
bool DecodeOriginal(AssetKind kind, int number, int* width, int* height, std::vector<std::uint8_t>* indices);

// Scale2x applied twice: smooths diagonal edges of pixel art without new colours.
std::vector<std::uint8_t> Scale4x(const std::vector<std::uint8_t>& src, int width, int height);

class ArtLibrary {
 public:
  void SetDirectory(const std::string& dir) { dir_ = dir; }
  const std::string& directory() const { return dir_; }
  void SetFallback(Fallback f) { fallback_ = f; }

  // (Re)reads manifest.txt and drops every cached image.
  void Reload();
  // Increases on every Reload, so renderers know to drop their textures.
  std::uint64_t generation() const { return generation_; }
  int manifestEntries() const { return static_cast<int>(entries_.size()); }

  const ArtImage& Get(AssetKind kind, int number);

 private:
  struct Entry {
    std::string file;
    float ax = 0, ay = 0;  // anchor offset in art pixels
    bool bad = false;
  };
  using Key = std::pair<int, int>;
  ArtImage Build(AssetKind kind, int number);
  // Live assets are changed by the game while it runs (the score box has its
  // digits drawn into it), so they always come from the current original data.
  static bool IsLive(AssetKind kind, int number);
  std::map<Key, std::uint32_t> live_checksums_;

  std::string dir_;
  Fallback fallback_ = Fallback::Scale4x;
  std::map<Key, Entry> entries_;
  std::map<Key, ArtImage> cache_;
  std::uint64_t generation_ = 0;
};

}  // namespace keen4::render
