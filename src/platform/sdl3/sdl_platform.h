#pragma once

#include <memory>
#include <string>

#include "platform/platform.h"

namespace keen4::platform {

struct SdlOptions {
  int scale = 3;          // initial window size: 320x240 times this
  bool fullscreen = false;
  bool audio = true;
  // Off by default: the game runs at 35 frames per second on a 70 Hz clock,
  // and waiting for a 60 Hz vertical blank makes frames alternate between 2
  // and 3 tics, so motion and scrolling stutter.
  bool vsync = false;

  // Remaster renderer (milestone 6): new art in a 16:9 view at native
  // resolution, with motion interpolated between game frames. F11 switches
  // to the classic renderer and back, F12 reloads the art manifest.
  bool remaster = true;
  bool remaster_vsync = true;  // interpolation hides the uneven frame steps
  bool interpolate = true;
  bool nearest = false;        // fallback art: nearest-neighbour instead of Scale4x
  std::string art_dir;         // folder with manifest.txt, empty for none
};

// Creates the SDL3 backend, or returns nullptr (with a message on stderr) if
// SDL cannot open a window.
std::unique_ptr<IPlatform> CreateSdlPlatform(const SdlOptions& options);

}  // namespace keen4::platform
