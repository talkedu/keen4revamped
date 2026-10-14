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
  void ReadGamepad(GamepadState* state) override;
  int GamepadCount() override { return static_cast<int>(order_.size()); }
  void ReadGamepadAt(int index, GamepadState* state) override;
  void ReadMouse(MouseState* state) override {
    *state = mouse_;
    mouse_.pressed = 0;
  }
  bool QuitRequested() override { return quit_; }

  int StartAudio(AudioCallback, void*) override { return 0; }
  void LockAudio() override {}
  void UnlockAudio() override {}

  // Test helpers.
  void PushKey(int scancode, bool down) { keys_.push_back({scancode, down, false}); }
  void RequestQuit() { quit_ = true; }
  // Plugs in, updates or (connected false) unplugs the pad in `slot` (0..3);
  // SetGamepad is slot 0. A slot's pad keeps its id while plugged in.
  void SetGamepad(const GamepadState& state) { SetGamepadAt(0, state); }
  void SetGamepadAt(int slot, const GamepadState& state);
  // Moves the mouse to (x, y) of the classic frame with `held` buttons down;
  // buttons not down before count as pressed.
  void SetMouse(int x, int y, std::uint32_t held) {
    mouse_.inside = x >= 0 && x < 320 && y >= 0 && y < 200;
    mouse_.x = x;
    mouse_.y = y;
    mouse_.pressed |= held & ~mouse_.held;
    mouse_.held = held;
  }
  int frames() const { return frames_; }
  const std::vector<std::uint8_t>& last_frame() const { return frame_; }
  const std::uint32_t* last_palette() const { return palette_; }
  int frame_width() const { return width_; }
  int frame_height() const { return height_; }

 private:
  std::uint64_t millis_ = 0;
  std::deque<KeyEvent> keys_;
  static constexpr int kSlots = 4;
  GamepadState pads_[kSlots];
  std::vector<int> order_;  // connected slots, oldest first
  std::uint32_t nextid_ = 1;
  MouseState mouse_;
  bool quit_ = false;
  int frames_ = 0;
  std::vector<std::uint8_t> frame_;
  std::uint32_t palette_[16] = {};
  int width_ = 0, height_ = 0;
};

}  // namespace keen4::platform

