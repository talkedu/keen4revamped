#include "engine/png_writer.h"

#include <cstdio>
#include <vector>

namespace keen4 {

namespace {

std::uint32_t Crc(const std::uint8_t* data, std::size_t len, std::uint32_t crc = 0xFFFFFFFFu) {
  static std::uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (std::uint32_t n = 0; n < 256; n++) {
      std::uint32_t c = n;
      for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[n] = c;
    }
    init = true;
  }
  for (std::size_t i = 0; i < len; i++) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}

void Put32(std::vector<std::uint8_t>* out, std::uint32_t v) {
  out->push_back(static_cast<std::uint8_t>(v >> 24));
  out->push_back(static_cast<std::uint8_t>(v >> 16));
  out->push_back(static_cast<std::uint8_t>(v >> 8));
  out->push_back(static_cast<std::uint8_t>(v));
}

void Chunk(std::vector<std::uint8_t>* out, const char* type, const std::vector<std::uint8_t>& data) {
  Put32(out, static_cast<std::uint32_t>(data.size()));
  std::size_t start = out->size();
  out->insert(out->end(), type, type + 4);
  out->insert(out->end(), data.begin(), data.end());
  Put32(out, Crc(out->data() + start, out->size() - start) ^ 0xFFFFFFFFu);
}


// Writes filtered scanlines (filter byte + pixels) as a stored-deflate PNG.
bool WriteRaw(const std::string& path, const std::vector<std::uint8_t>& raw, int width, int height,
              std::uint8_t colortype) {
  std::vector<std::uint8_t> z = {0x78, 0x01};
  std::size_t pos = 0;
  while (pos < raw.size() || raw.empty()) {
    std::size_t n = raw.size() - pos;
    if (n > 65535) n = 65535;
    bool last = pos + n >= raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back(static_cast<std::uint8_t>(n));
    z.push_back(static_cast<std::uint8_t>(n >> 8));
    z.push_back(static_cast<std::uint8_t>(~n));
    z.push_back(static_cast<std::uint8_t>(~n >> 8));
    z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
    pos += n;
    if (raw.empty()) break;
  }
  std::uint32_t a = 1, b = 0;
  for (std::uint8_t v : raw) {
    a = (a + v) % 65521;
    b = (b + a) % 65521;
  }
  Put32(&z, (b << 16) | a);

  std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<std::uint8_t> ihdr;
  Put32(&ihdr, static_cast<std::uint32_t>(width));
  Put32(&ihdr, static_cast<std::uint32_t>(height));
  ihdr.insert(ihdr.end(), {8, colortype, 0, 0, 0});
  Chunk(&png, "IHDR", ihdr);
  Chunk(&png, "IDAT", z);
  Chunk(&png, "IEND", {});

  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
  std::fclose(f);
  return ok;
}

}  // namespace

bool WritePng(const std::string& path, const std::uint8_t* pixels, int width, int height,
              const std::uint32_t* palette) {
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(height) * (width * 3 + 1));
  for (int y = 0; y < height; y++) {
    raw.push_back(0);
    for (int x = 0; x < width; x++) {
      std::uint32_t c = palette[pixels[y * width + x]];
      raw.push_back(static_cast<std::uint8_t>(c >> 16));
      raw.push_back(static_cast<std::uint8_t>(c >> 8));
      raw.push_back(static_cast<std::uint8_t>(c));
    }
  }
  return WriteRaw(path, raw, width, height, 2);
}

bool WritePngRgb(const std::string& path, const std::uint32_t* pixels, int width, int height) {
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(height) * (width * 3 + 1));
  for (int y = 0; y < height; y++) {
    raw.push_back(0);
    for (int x = 0; x < width; x++) {
      std::uint32_t c = pixels[static_cast<std::size_t>(y) * width + x];
      raw.push_back(static_cast<std::uint8_t>(c >> 16));
      raw.push_back(static_cast<std::uint8_t>(c >> 8));
      raw.push_back(static_cast<std::uint8_t>(c));
    }
  }
  return WriteRaw(path, raw, width, height, 2);
}

bool WritePngRgba(const std::string& path, const std::uint32_t* pixels, int width, int height) {
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(height) * (width * 4 + 1));
  for (int y = 0; y < height; y++) {
    raw.push_back(0);
    for (int x = 0; x < width; x++) {
      std::uint32_t c = pixels[static_cast<std::size_t>(y) * width + x];
      raw.push_back(static_cast<std::uint8_t>(c >> 16));
      raw.push_back(static_cast<std::uint8_t>(c >> 8));
      raw.push_back(static_cast<std::uint8_t>(c));
      raw.push_back(static_cast<std::uint8_t>(c >> 24));
    }
  }
  return WriteRaw(path, raw, width, height, 6);
}

}  // namespace keen4
