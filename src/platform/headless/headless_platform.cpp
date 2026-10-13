#include "platform/headless/headless_platform.h"

#include <algorithm>
#include <cstring>

namespace keen4::platform {

void HeadlessPlatform::Present(const std::uint8_t* pixels, int width, int height,
                               const std::uint32_t* palette) {
  frame_.assign(pixels, pixels + width * height);
  std::memcpy(palette_, palette, sizeof palette_);
  width_ = width;
  height_ = height;
  frames_++;
}

bool HeadlessPlatform::PollKey(KeyEvent* event) {
  if (keys_.empty()) return false;
  *event = keys_.front();
  keys_.pop_front();
  return true;
}

void HeadlessPlatform::ReadGamepad(GamepadState* state) {
  if (order_.empty())
    *state = GamepadState{};
  else
    *state = pads_[order_.back()];
}

void HeadlessPlatform::ReadGamepadAt(int index, GamepadState* state) {
  if (index < 0 || index >= static_cast<int>(order_.size()))
    *state = GamepadState{};
  else
    *state = pads_[order_[index]];
}

void HeadlessPlatform::SetGamepadAt(int slot, const GamepadState& state) {
  if (slot < 0 || slot >= kSlots) return;
  GamepadState& pad = pads_[slot];
  bool was = pad.connected;
  std::uint32_t id = pad.id;
  pad = state;
  if (!state.connected) {
    pad = GamepadState{};
    order_.erase(std::remove(order_.begin(), order_.end(), slot), order_.end());
    return;
  }
  pad.id = was ? id : nextid_++;
  if (!was) order_.push_back(slot);
}

}  // namespace keen4::platform
