#include "render/scene.h"

#include <cstdlib>

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

bool InterpolateSprite(const Scene& prev, const SceneSprite& sp, float alpha, int maxstep, float* x, float* y) {
  const int objx = sp.x - sp.artx, objy = sp.y - sp.arty;
  for (const SceneSprite& old : prev.sprites) {
    if (old.id != sp.id) continue;
    const int oldx = old.x - old.artx, oldy = old.y - old.arty;
    if (std::abs(oldx - objx) > maxstep || std::abs(oldy - objy) > maxstep) return false;
    *x = static_cast<float>(oldx) + static_cast<float>(objx - oldx) * alpha + static_cast<float>(sp.artx);
    *y = static_cast<float>(oldy) + static_cast<float>(objy - oldy) * alpha + static_cast<float>(sp.arty);
    return true;
  }
  return false;
}

}  // namespace keen4::render
