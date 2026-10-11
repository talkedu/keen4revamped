#pragma once

#include "platform/platform.h"

namespace keen4::platform {

// No window, no sound, scripted input. Used by tests and CI.
class HeadlessTimer final : public ITimer {
 public:
  std::uint32_t Tics() const override { return tics_; }
  void Advance(std::uint32_t tics) { tics_ += tics; }

 private:
  std::uint32_t tics_ = 0;
};

class HeadlessInput final : public IInput {
 public:
  bool KeyDown(int scancode) const override;
  void SetKey(int scancode, bool down);

 private:
  bool keys_[128] = {};
};

}  // namespace keen4::platform
