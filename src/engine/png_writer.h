#pragma once

#include <cstdint>
#include <string>

namespace keen4 {

// Writes an 8-bit indexed image as an RGB PNG (uncompressed deflate).
// `palette` holds 0xRRGGBB entries indexed by the pixel values.
bool WritePng(const std::string& path, const std::uint8_t* pixels, int width, int height,
              const std::uint32_t* palette);

// 0xXXRRGGBB pixels as RGB, and 0xAARRGGBB as RGBA.
bool WritePngRgb(const std::string& path, const std::uint32_t* pixels, int width, int height);
bool WritePngRgba(const std::string& path, const std::uint32_t* pixels, int width, int height);

}  // namespace keen4
