#include "platform/headless/headless_platform.h"

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

}  // namespace keen4::platform
