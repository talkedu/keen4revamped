#include "render/art.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "id/id_ca.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_FAILURE_STRINGS
#include "stb_image.h"

namespace keen4::render {

namespace {

constexpr int kDensity = 4;
constexpr std::uint8_t kClear = 16;

std::uint32_t Rgba(std::uint8_t index) {
  return index == kClear ? 0x00000000u : (0xFF000000u | EgaColor(index));
}

// Planar EGA bytes to indices. `mask` may be null; a set mask bit is see-through.
void Unplane(const std::uint8_t* planes, const std::uint8_t* mask, int wbytes, int height,
             std::vector<std::uint8_t>* out) {
  const int planesize = wbytes * height;
  out->assign(static_cast<std::size_t>(wbytes) * 8 * height, kClear);
  for (int row = 0; row < height; row++) {
    for (int b = 0; b < wbytes; b++) {
      int idx = row * wbytes + b;
      for (int bit = 0; bit < 8; bit++) {
        std::uint8_t sel = static_cast<std::uint8_t>(0x80 >> bit);
        if (mask && (mask[idx] & sel)) continue;
        std::uint8_t c = 0;
        for (int p = 0; p < 4; p++)
          if (planes[p * planesize + idx] & sel) c = static_cast<std::uint8_t>(c | (1 << p));
        (*out)[static_cast<std::size_t>(row) * wbytes * 8 + b * 8 + bit] = c;
      }
    }
  }
}

std::vector<std::uint8_t> Scale2x(const std::vector<std::uint8_t>& s, int w, int h) {
  std::vector<std::uint8_t> d(static_cast<std::size_t>(w) * h * 4);
  auto at = [&](int x, int y) {
    x = x < 0 ? 0 : (x >= w ? w - 1 : x);
    y = y < 0 ? 0 : (y >= h ? h - 1 : y);
    return s[static_cast<std::size_t>(y) * w + x];
  };
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      std::uint8_t e = at(x, y), b = at(x, y - 1), dd = at(x - 1, y), f = at(x + 1, y), hh = at(x, y + 1);
      std::uint8_t e0 = e, e1 = e, e2 = e, e3 = e;
      if (b != hh && dd != f) {
        e0 = dd == b ? dd : e;
        e1 = b == f ? f : e;
        e2 = dd == hh ? dd : e;
        e3 = hh == f ? f : e;
      }
      std::size_t o = static_cast<std::size_t>(y) * 2 * w * 2 + x * 2;
      d[o] = e0;
      d[o + 1] = e1;
      d[o + w * 2] = e2;
      d[o + w * 2 + 1] = e3;
    }
  }
  return d;
}

const char* KindName(AssetKind kind) {
  switch (kind) {
    case AssetKind::Tile16: return "tile16";
    case AssetKind::Tile16M: return "tile16m";
    default: return "sprite";
  }
}

}  // namespace

std::uint32_t EgaColor(int index) {
  static const std::uint32_t colors[16] = {0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA,
                                           0xAA5500, 0xAAAAAA, 0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
                                           0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF};
  return colors[index & 15];
}

bool DecodeOriginal(AssetKind kind, int number, int* width, int* height, std::vector<std::uint8_t>* indices) {
  switch (kind) {
    case AssetKind::Tile16: {
      if (number < 0 || number >= NUMTILE16) return false;
      CA_CacheGrChunk(STARTTILE16 + number);
      const std::uint8_t* data = grsegs[STARTTILE16 + number];
      if (!data) return false;
      *width = *height = 16;
      Unplane(data, nullptr, 2, 16, indices);
      return true;
    }
    case AssetKind::Tile16M: {
      if (number <= 0 || number >= NUMTILE16M) return false;
      CA_CacheGrChunk(STARTTILE16M + number);
      const std::uint8_t* data = grsegs[STARTTILE16M + number];
      if (!data) return false;
      *width = *height = 16;
      Unplane(data + 32, data, 2, 16, indices);
      return true;
    }
    case AssetKind::Sprite: {
      if (number < STARTSPRITES || number >= STARTSPRITES + NUMSPRITES) return false;
      CA_CacheGrChunk(number);
      const std::uint8_t* data = grsegs[number];
      if (!data) return false;
      const spritetabletype& spr = spritetable[number - STARTSPRITES];
      *width = spr.width * 8;
      *height = spr.height;
      Unplane(data + spr.width * spr.height, data, spr.width, spr.height, indices);
      return true;
    }
  }
  return false;
}

std::vector<std::uint8_t> Scale4x(const std::vector<std::uint8_t>& src, int width, int height) {
  return Scale2x(Scale2x(src, width, height), width * 2, height * 2);
}

void ArtLibrary::Reload() {
  entries_.clear();
  cache_.clear();
  generation_++;
  if (dir_.empty()) return;
  std::ifstream in(std::filesystem::path(dir_) / "manifest.txt");
  if (!in) return;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    std::string kind, file, extra;
    int number;
    if (!(ss >> kind >> number >> file)) continue;
    int k;
    if (kind == "tile16")
      k = static_cast<int>(AssetKind::Tile16);
    else if (kind == "tile16m")
      k = static_cast<int>(AssetKind::Tile16M);
    else if (kind == "sprite")
      k = static_cast<int>(AssetKind::Sprite);
    else
      continue;
    Entry e;
    e.file = file;
    while (ss >> extra) {
      if (extra == "bad") {
        e.bad = true;
      } else if (extra == "anchor") {
        ss >> e.ax >> e.ay;
      }
    }
    entries_[{k, number}] = e;
  }
}

bool ArtLibrary::IsLive(AssetKind kind, int number) {
  constexpr int kScoreBoxSprite = 253;
  return kind == AssetKind::Sprite && number == kScoreBoxSprite;
}

const ArtImage& ArtLibrary::Get(AssetKind kind, int number) {
  Key key{static_cast<int>(kind), number};
  auto found = cache_.find(key);
  if (IsLive(kind, number)) {
    std::uint32_t sum = 0;
    if (const std::uint8_t* data = grsegs[number])
      for (std::uint32_t i = 0; i < grsize[number]; i++) sum = sum * 31 + data[i];
    if (found != cache_.end() && live_checksums_[key] == sum) return found->second;
    live_checksums_[key] = sum;
    std::uint32_t revision = found != cache_.end() ? found->second.revision + 1 : 0;
    ArtImage& img = cache_[key] = Build(kind, number);
    img.revision = revision;
    return img;
  }
  if (found != cache_.end()) return found->second;
  return cache_[key] = Build(kind, number);
}

ArtImage ArtLibrary::Build(AssetKind kind, int number) {
  ArtImage img;
  int w = 0, h = 0;
  std::vector<std::uint8_t> indices;
  if (!DecodeOriginal(kind, number, &w, &h, &indices)) return img;
  img.origw = w;
  img.origh = h;

  auto entry = entries_.find({static_cast<int>(kind), number});
  if (entry != entries_.end() && !entry->second.bad && !IsLive(kind, number)) {
    std::string path = (std::filesystem::path(dir_) / entry->second.file).string();
    int iw, ih, comp;
    stbi_uc* data = stbi_load(path.c_str(), &iw, &ih, &comp, 4);
    if (data) {
      img.width = iw;
      img.height = ih;
      img.pixels.resize(static_cast<std::size_t>(iw) * ih);
      for (std::size_t i = 0; i < img.pixels.size(); i++) {
        const stbi_uc* p = data + i * 4;
        img.pixels[i] = (static_cast<std::uint32_t>(p[3]) << 24) | (static_cast<std::uint32_t>(p[0]) << 16) |
                        (static_cast<std::uint32_t>(p[1]) << 8) | p[2];
      }
      stbi_image_free(data);
      // art is drawn at 4x density; the anchor marks the original's top-left
      img.ox = -entry->second.ax / kDensity;
      img.oy = -entry->second.ay / kDensity;
      img.fromManifest = true;
      return img;
    }
    std::fprintf(stderr, "art: cannot load %s for %s %d, using the original\n", path.c_str(), KindName(kind),
                 number);
  }

  std::vector<std::uint8_t> big;
  if (fallback_ == Fallback::Scale4x) {
    big = Scale4x(indices, w, h);
  } else {
    big.resize(static_cast<std::size_t>(w) * h * 16);
    for (int y = 0; y < h * 4; y++)
      for (int x = 0; x < w * 4; x++)
        big[static_cast<std::size_t>(y) * w * 4 + x] = indices[static_cast<std::size_t>(y / 4) * w + x / 4];
  }
  img.width = w * 4;
  img.height = h * 4;
  img.pixels.resize(big.size());
  for (std::size_t i = 0; i < big.size(); i++) img.pixels[i] = Rgba(big[i]);
  return img;
}

}  // namespace keen4::render
