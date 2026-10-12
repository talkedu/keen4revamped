#include "render/scene.h"

namespace keen4::render {

namespace {
Scene scenes[2];
int latest = -1;
std::uint64_t serial;
}  // namespace

Scene& BeginCapture() {
  Scene& s = scenes[latest == 0 ? 1 : 0];
  s.bg.clear();
  s.fg.clear();
  s.fgfront.clear();
  s.sprites.clear();
  s.serial = ++serial;
  return s;
}

void EndCapture() { latest = latest == 0 ? 1 : 0; }

const Scene* LatestScene() { return latest < 0 ? nullptr : &scenes[latest]; }

void InvalidateScene() { latest = -1; }

}  // namespace keen4::render
