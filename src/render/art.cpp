#include "render/art.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
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

bool SpriteHitbox(int chunk, float* x0, float* y0, float* x1, float* y1) {
  if (!spritetable || chunk < STARTSPRITES || chunk >= STARTSPRITES + NUMSPRITES) return false;
  const spritetabletype& spr = spritetable[chunk - STARTSPRITES];
  // the hitbox is at ob->x + xl and the image at ob->x + orgx, in 1/16 pixels
  constexpr float kUnit = 1 << G_P_SHIFT;
  *x0 = (spr.xl - spr.orgx) / kUnit;
  *y0 = (spr.yl - spr.orgy) / kUnit;
  *x1 = (spr.xh - spr.orgx) / kUnit;
  *y1 = (spr.yh - spr.orgy) / kUnit;
  return true;
}

std::vector<std::uint8_t> Scale4x(const std::vector<std::uint8_t>& src, int width, int height) {
  return Scale2x(Scale2x(src, width, height), width * 2, height * 2);
}

ArtLibrary::~ArtLibrary() { StopReader(); }

void ArtLibrary::StartReader() {
  stop_ = false;
  next_all_ = 0;
  reader_ = std::thread(&ArtLibrary::ReaderLoop, this);
}

void ArtLibrary::StopReader() {
  if (!reader_.joinable()) return;
  {
    std::lock_guard<std::mutex> lock(files_mutex_);
    stop_ = true;
  }
  files_wake_.notify_all();
  reader_.join();
}

void ArtLibrary::ReaderLoop() {
  for (;;) {
    std::string path;
    {
      std::unique_lock<std::mutex> lock(files_mutex_);
      files_wake_.wait(lock, [&] { return stop_ || !wanted_.empty() || next_all_ < all_.size(); });
      if (stop_) return;
      if (!wanted_.empty()) {
        path = wanted_.front();
        wanted_.pop_front();
      } else {
        path = all_[next_all_++];
      }
      if (files_.count(path)) continue;
    }
    std::vector<std::uint8_t> bytes;
    std::ifstream in(path, std::ios::binary);
    if (in) bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.emplace(path, std::move(bytes));
  }
}

std::vector<std::uint8_t> ArtLibrary::FileBytes(const std::string& path) {
  {
    std::lock_guard<std::mutex> lock(files_mutex_);
    auto found = files_.find(path);
    if (found != files_.end()) return found->second;
  }
  // not read yet: read it now rather than wait behind the queue
  std::vector<std::uint8_t> bytes;
  std::ifstream in(path, std::ios::binary);
  if (in) bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  std::lock_guard<std::mutex> lock(files_mutex_);
  files_.emplace(path, bytes);
  return bytes;
}

bool ArtLibrary::Ready(AssetKind kind, int number) {
  if (IsLive(kind, number) || cache_.count({static_cast<int>(kind), number})) return true;
  Source src;
  if (!Find(kind, number, &src)) return true;  // the built-in upscale needs no file
  std::lock_guard<std::mutex> lock(files_mutex_);
  if (files_.count(src.path)) return true;
  if (asked_.insert(src.path).second) {
    wanted_.push_back(src.path);
    files_wake_.notify_one();
  }
  return false;
}

void ArtLibrary::Reload() {
  StopReader();
  files_.clear();
  wanted_.clear();
  asked_.clear();
  all_.clear();
  layers_.clear();
  cache_.clear();
  generation_++;
  for (const std::string& dir : dirs_) {
    std::map<Key, Entry>& entries = layers_.emplace_back();
    if (dir.empty()) continue;
    std::ifstream in(std::filesystem::path(dir) / "manifest.txt");
    if (!in) continue;
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
      if (!e.bad) all_.push_back((std::filesystem::path(dir) / file).string());
      entries[{k, number}] = e;
    }
  }
  StartReader();
}

int ArtLibrary::manifestEntries() const {
  int n = 0;
  for (const auto& layer : layers_) n += static_cast<int>(layer.size());
  return n;
}

bool ArtLibrary::Find(AssetKind kind, int number, Source* source, int from_layer) const {
  for (int i = from_layer; i < static_cast<int>(layers_.size()); i++) {
    auto entry = layers_[i].find({static_cast<int>(kind), number});
    if (entry == layers_[i].end() || entry->second.bad) continue;
    source->path = (std::filesystem::path(dirs_[i]) / entry->second.file).string();
    source->ax = entry->second.ax;
    source->ay = entry->second.ay;
    source->layer = i;
    return true;
  }
  return false;
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

std::vector<std::uint32_t> DitherTones(const std::vector<std::uint8_t>& indices, int width, int height) {
  std::vector<std::uint32_t> tones(indices.size(), 0);
  auto at = [&](int x, int y) -> int {
    if (x < 0 || y < 0 || x >= width || y >= height) return -1;
    return indices[static_cast<std::size_t>(y) * width + x];
  };
  auto mix = [](std::uint32_t a, std::uint32_t b) {
    std::uint32_t out = 0;
    for (int shift = 0; shift <= 16; shift += 8) {
      double ca = std::pow(((a >> shift) & 255) / 255.0, 2.2), cb = std::pow(((b >> shift) & 255) / 255.0, 2.2);
      out |= static_cast<std::uint32_t>(std::lround(std::pow((ca + cb) / 2, 1 / 2.2) * 255)) << shift;
    }
    return out;
  };
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const int a = at(x, y);
      if (a == kClear) continue;
      // side neighbours all one other colour, corner neighbours all this one;
      // neighbours past the image edge (the next tile) are not known
      int b = -1, sides = 0, corners = 0;
      bool checker = true, partner = false;
      for (auto [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
        const int c = at(x + dx, y + dy);
        if (c < 0) continue;
        if (b < 0) b = c;
        checker = checker && c == b && c != a && c != kClear;
        sides++;
        // near the edge, make sure a neighbour is part of the pattern too, so
        // the end of a diagonal line is not taken for a dither
        bool around = true;
        for (auto [ex, ey] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
          const int e = at(x + dx + ex, y + dy + ey);
          if (e >= 0) around = around && e == a;
        }
        partner = partner || around;
      }
      checker = checker && partner;
      for (auto [dx, dy] : {std::pair{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}) {
        const int c = at(x + dx, y + dy);
        if (c < 0) continue;
        checker = checker && c == a;
        corners++;
      }
      if (checker && sides >= 2 && corners >= 1)
        tones[static_cast<std::size_t>(y) * width + x] = 0xFF000000u | mix(EgaColor(a), EgaColor(b));
    }
  }
  return tones;
}

namespace {

// Replaces checkerboard dithers in `img` with the flat tone they stand for.
// A dither of single pixels shimmers and looks darker while it scrolls
// smoothly, and lighter again when the view stops. Only art pixels that still
// have one of the dither's two colours change, so drawn-over detail stays.
void SmoothDithers(ArtImage* img, const std::vector<std::uint8_t>& indices, int w, int h, float ax, float ay) {
  const std::vector<std::uint32_t> tones = DitherTones(indices, w, h);
  for (int ty = 0; ty < img->height; ty++) {
    const int oy = static_cast<int>(std::floor((ty - ay) / kDensity));
    if (oy < 0 || oy >= h) continue;
    for (int tx = 0; tx < img->width; tx++) {
      const int ox = static_cast<int>(std::floor((tx - ax) / kDensity));
      if (ox < 0 || ox >= w) continue;
      const std::size_t o = static_cast<std::size_t>(oy) * w + ox;
      if (!tones[o]) continue;
      std::uint32_t& texel = img->pixels[static_cast<std::size_t>(ty) * img->width + tx];
      if ((texel >> 24) != 255) continue;
      // the dither's colours: this pixel's and its side neighbours'
      const std::uint32_t own = EgaColor(indices[o]);
      const std::uint8_t side = ox + 1 < w   ? indices[o + 1]
                                : ox > 0     ? indices[o - 1]
                                : oy + 1 < h ? indices[o + w]
                                             : indices[o - w];
      const std::uint32_t rgb = texel & 0xFFFFFF;
      if (rgb == own || rgb == EgaColor(side)) texel = tones[o];
    }
  }
}

}  // namespace

ArtImage ArtLibrary::Build(AssetKind kind, int number) {
  ArtImage img;
  int w = 0, h = 0;
  std::vector<std::uint8_t> indices;
  if (!DecodeOriginal(kind, number, &w, &h, &indices)) return img;
  img.origw = w;
  img.origh = h;

  // The first layer whose image loads wins; a missing file falls through to
  // the next layer, and finally to the original.
  Source src;
  for (int from = 0; !IsLive(kind, number) && Find(kind, number, &src, from); from = src.layer + 1) {
    int iw, ih, comp;
    const std::vector<std::uint8_t> bytes = FileBytes(src.path);
    stbi_uc* data = bytes.empty() ? nullptr
                                  : stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &iw,
                                                          &ih, &comp, 4);
    if (!data) {
      std::fprintf(stderr, "art: cannot load %s for %s %d\n", src.path.c_str(), KindName(kind), number);
      continue;
    }
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
    img.ox = -src.ax / kDensity;
    img.oy = -src.ay / kDensity;
    img.fromManifest = true;
    img.layer = src.layer;
    SmoothDithers(&img, indices, w, h, src.ax, src.ay);
    return img;
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
  SmoothDithers(&img, indices, w, h, 0, 0);
  return img;
}

}  // namespace keen4::render
