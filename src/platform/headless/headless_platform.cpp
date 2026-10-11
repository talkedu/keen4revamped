#include "platform/headless/headless_platform.h"

namespace keen4::platform {

bool HeadlessInput::KeyDown(int scancode) const {
  return scancode >= 0 && scancode < 128 && keys_[scancode];
}

void HeadlessInput::SetKey(int scancode, bool down) {
  if (scancode >= 0 && scancode < 128) keys_[scancode] = down;
}

}  // namespace keen4::platform
