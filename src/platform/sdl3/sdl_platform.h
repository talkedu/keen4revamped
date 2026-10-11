#pragma once

#include <memory>

#include "platform/platform.h"

namespace keen4::platform {

struct SdlOptions {
  int scale = 3;          // initial window size: 320x240 times this
  bool fullscreen = false;
  bool audio = true;
};

// Creates the SDL3 backend, or returns nullptr (with a message on stderr) if
// SDL cannot open a window.
std::unique_ptr<IPlatform> CreateSdlPlatform(const SdlOptions& options);

}  // namespace keen4::platform
