#include "exe/keen4exe.h"

#include <cstdio>
#include <cstring>

namespace keen4::exe {

Image g_exe;

namespace {

std::uint16_t Get16(const std::vector<std::uint8_t>& d, std::size_t p) {
  return static_cast<std::uint16_t>(d[p] | (d[p + 1] << 8));
}

// Size of the unpacked v1.4 load image. Anything else is a different release.
constexpr std::size_t kV14ImageSize = 251712;

}  // namespace

std::vector<std::uint8_t> UnpackLzexe(const std::vector<std::uint8_t>& file) {
  std::vector<std::uint8_t> out;
  if (file.size() < 0x20 || file[0] != 'M' || file[1] != 'Z' ||
      std::memcmp(&file[0x1C], "LZ91", 4) != 0)
    return out;

  const std::size_t hdrsize = Get16(file, 8) * 16u;
  const std::uint16_t cs0 = Get16(file, 0x16);
  if (hdrsize >= file.size()) return out;
  std::vector<std::uint8_t> img(file.begin() + static_cast<std::ptrdiff_t>(hdrsize), file.end());
  const std::size_t lzbase = cs0 * 16u;
  if (lzbase + 16 > img.size()) return out;
  const std::uint16_t cpsize = Get16(img, lzbase + 8);

  std::size_t p = (cs0 - cpsize) * 16u;
  std::uint16_t buf = 0;
  int cnt = 0;
  bool bad = false;
  auto getb = [&]() -> std::uint8_t {
    if (p >= img.size()) { bad = true; return 0; }
    return img[p++];
  };
  auto getw = [&]() -> std::uint16_t {
    std::uint16_t lo = getb();
    return static_cast<std::uint16_t>(lo | (getb() << 8));
  };
  auto bit = [&]() -> int {
    int b = buf & 1;
    if (--cnt == 0) {
      buf = getw();
      cnt = 16;
    } else {
      buf >>= 1;
    }
    return b;
  };

  buf = getw();
  cnt = 16;
  out.reserve(kV14ImageSize);
  while (!bad) {
    if (bit()) {
      out.push_back(getb());
      continue;
    }
    int len;
    int span;
    if (!bit()) {
      len = ((bit() << 1) | bit()) + 2;
      span = getb() | 0xFF00;
    } else {
      span = getb();
      len = getb();
      span |= ((len & ~7) << 5) | 0xE000;
      len = (len & 7) + 2;
      if (len == 2) {
        len = getb();
        if (len == 0) break;
        if (len == 1) continue;
        len++;
      }
    }
    span -= 0x10000;
    for (int i = 0; i < len; i++) {
      std::ptrdiff_t src = static_cast<std::ptrdiff_t>(out.size()) + span;
      if (src < 0) { bad = true; break; }
      out.push_back(out[static_cast<std::size_t>(src)]);
    }
  }
  if (bad) out.clear();
  return out;
}

bool Image::LoadFromMemory(const std::vector<std::uint8_t>& file, std::string* error) {
  image_ = UnpackLzexe(file);
  if (image_.empty()) {
    *error = "not an LZEXE-packed KEEN4E.EXE";
    return false;
  }
  if (image_.size() != kV14ImageSize) {
    image_.clear();
    *error = "unsupported KEEN4E.EXE version (need Keen 4 v1.4 EGA)";
    return false;
  }
  return true;
}

bool Image::Load(const std::string& path, std::string* error) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    *error = "cannot open " + path;
    return false;
  }
  std::vector<std::uint8_t> file;
  std::uint8_t chunk[65536];
  std::size_t n;
  while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) file.insert(file.end(), chunk, chunk + n);
  std::fclose(f);
  return LoadFromMemory(file, error);
}

}  // namespace keen4::exe
