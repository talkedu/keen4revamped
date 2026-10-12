#pragma once

#include <deque>
#include <vector>

#include "platform/platform.h"

namespace keen4::platform {

// No window, no sound, scripted input and a virtual clock. Used by tests, CI
// and for rendering frames to image files.
class HeadlessPlatform : public IPlatform {
 public:
  std::uint64_t Ticks70() override { return millis_ * 70 / 1000; }
  std::uint64_t Millis() override { return millis_; }
  // Time only moves when the program waits, so runs are deterministic.
  void Delay(int ms) override { millis_ += static_cast<std::uint64_t>(ms > 0 ? ms : 1); }

  void Present(const std::uint8_t* pixels, int width, int height,
               const std::uint32_t* palette) override;

  bool PollKey(KeyEvent* event) override;
  bool QuitRequested() override { return quit_; }

  int StartAudio(AudioCallback, void*) override { return 0; }
  void LockAudio() override {}
  void UnlockAudio() override {}

  // Test helpers.
  void PushKey(int scancode, bool down) { keys_.push_back({scancode, down, false}); }
  void RequestQuit() { quit_ = true; }
  int frames() const { return frames_; }
  const std::vector<std::uint8_t>& last_frame() const { return frame_; }
  const std::uint32_t* last_palette() const { return palette_; }
  int frame_width() const { return width_; }
  int frame_height() const { return height_; }

 private:
  std::uint64_t millis_ = 0;
  std::deque<KeyEvent> keys_;
  bool quit_ = false;
  int frames_ = 0;
  std::vector<std::uint8_t> frame_;
  std::uint32_t palette_[16] = {};
  int width_ = 0, height_ = 0;
};

}  // namespace keen4::platform
