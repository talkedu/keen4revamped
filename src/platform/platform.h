#pragma once

#include <cstdint>

// Platform interfaces. Game and engine code talk only to these; the SDL3 and
// headless backends implement them. See docs/ARCHITECTURE.md, "Platform layer".
namespace keen4::platform {

class ITimer {
 public:
  virtual ~ITimer() = default;
  // Tics at the original 70 Hz rate since startup.
  virtual std::uint32_t Tics() const = 0;
};

class IVideo {
 public:
  virtual ~IVideo() = default;
  // Present an 8-bit indexed frame using a 16-entry RGB palette.
  virtual void Present(const std::uint8_t* pixels, int width, int height,
                       const std::uint32_t* palette) = 0;
};

class IAudio {
 public:
  virtual ~IAudio() = default;
};

class IInput {
 public:
  virtual ~IInput() = default;
  // Original keyboard scancode state, 128 entries.
  virtual bool KeyDown(int scancode) const = 0;
};

class IFileSystem {
 public:
  virtual ~IFileSystem() = default;
};

}  // namespace keen4::platform
