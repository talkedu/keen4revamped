#pragma once

#include <cstdint>

// Platform interface. Engine and game code reach the operating system only
// through this; the SDL3 and headless backends implement it.
namespace keen4::platform {

// One key transition, in original XT (set 1) scancodes. Extended keys such as
// the arrow keys arrive as the scancode the DOS BIOS produced for them.
struct KeyEvent {
  int scancode;
  bool down;
  bool extended;  // preceded by the 0xE0 prefix on a real keyboard
};

// Fills `samples` mono 16-bit samples at the rate passed to StartAudio.
using AudioCallback = void (*)(std::int16_t* samples, int count, void* user);

class IPlatform {
 public:
  virtual ~IPlatform() = default;

  // Monotonic time in 1/70 s units (the original timer rate) and milliseconds.
  virtual std::uint64_t Ticks70() = 0;
  virtual std::uint64_t Millis() = 0;
  virtual void Delay(int ms) = 0;

  // Shows an 8-bit indexed frame. `palette` has 16 0xRRGGBB entries.
  virtual void Present(const std::uint8_t* pixels, int width, int height,
                       const std::uint32_t* palette) = 0;

  // Next pending key transition; false when there are none.
  virtual bool PollKey(KeyEvent* event) = 0;
  // True once the user closed the window.
  virtual bool QuitRequested() = 0;

  // Starts streaming audio from `callback`. Returns the sample rate, or 0 if
  // there is no audio device.
  virtual int StartAudio(AudioCallback callback, void* user) = 0;
  virtual void LockAudio() = 0;
  virtual void UnlockAudio() = 0;
};

// The active backend.
IPlatform& Get();
void Set(IPlatform* platform);

}  // namespace keen4::platform
