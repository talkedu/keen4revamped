#pragma once

#include <memory>
#include <string>
#include <vector>

#include "platform/platform.h"

namespace keen4::platform {

struct SdlOptions {
  int scale = 3;          // initial window size: 320x240 times this
  bool audio = true;
  // Screen mode, vsync, renderer and resolution (the VIDEO card). Alt+Enter
  // switches full screen and F11 the renderer.
  //
  // The remaster renderer (milestone 6) draws new art in a 16:9 view at
  // native resolution, with motion interpolated between game frames, which
  // hides the uneven frame steps of vsync. The classic renderer shows the
  // 35 fps game on a 70 Hz clock; on a 60 Hz display, vsync makes its frames
  // alternate between 2 and 3 refreshes. F12 reloads the art manifests and
  // Shift+F12 shows sprite hitboxes over the art.
  VideoSettings video;
  bool interpolate = true;
  bool nearest = false;        // fallback art: nearest-neighbour instead of Scale4x
  // Folders with a manifest.txt, curated art first; the first that lists an
  // asset provides it.
  std::vector<std::string> art_dirs;
  bool hitboxes = false;
};

// Creates the SDL3 backend, or returns nullptr (with a message on stderr) if
// SDL cannot open a window.
std::unique_ptr<IPlatform> CreateSdlPlatform(const SdlOptions& options);

}  // namespace keen4::platform
