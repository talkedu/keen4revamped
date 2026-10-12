#include "render/motion.h"

#include <cmath>

namespace keen4::render {

void SpriteMotion::Reset() {
  tracks_.clear();
  map_ = -1;
  last_update_ = -1;
}

const SpriteMotion::Track* SpriteMotion::Find(const void* id) const {
  for (const Track& t : tracks_)
    if (t.id == id) return &t;
  return nullptr;
}

void SpriteMotion::Eval(const Track& t, double now, float* x, float* y) {
  if (now >= t.t1 || t.t1 <= t.t0) {
    *x = static_cast<float>(t.tox);
    *y = static_cast<float>(t.toy);
    return;
  }
  float a = now <= t.t0 ? 0.0f : static_cast<float>((now - t.t0) / (t.t1 - t.t0));
  *x = t.fromx + (static_cast<float>(t.tox) - t.fromx) * a;
  *y = t.fromy + (static_cast<float>(t.toy) - t.fromy) * a;
}

namespace {

// The time between moves is a whole number of refreshes, so a slug stepping
// every 8 tics is seen moving after 6 or 9. Average it, but follow an actor
// that speeds up at once (Keen starting to run), and glide a move after a
// pause (turning round, standing still) at the usual pace.
double GlideTime(double* step, double interval, double refresh) {
  double glide = *step;
  if (*step <= 0) {
    if (interval <= SpriteMotion::kMaxGlide) *step = glide = interval;
  } else if (interval < *step / 2) {
    *step = glide = interval;
  } else if (interval <= *step * 2) {
    *step = glide = (*step + interval) / 2;
  }
  if (glide > SpriteMotion::kMaxGlide) glide = 0;
  return glide > refresh ? glide : refresh;
}

}  // namespace

void SpriteMotion::Update(const Scene& scene, double now) {
  if (scene.map != map_) {
    tracks_.clear();
    map_ = scene.map;
  }
  const double refresh = last_update_ >= 0 && now > last_update_ ? now - last_update_ : 0;
  last_update_ = now;
  std::vector<Track> next;
  next.reserve(scene.sprites.size());
  for (const SceneSprite& sp : scene.sprites) {
    const int ox = sp.x - sp.artx, oy = sp.y - sp.arty;
    Track t{sp.id, static_cast<float>(ox), static_cast<float>(oy), ox, oy, now, now, now, 0};
    if (const Track* old = Find(sp.id)) {
      float cx, cy;
      Eval(*old, now, &cx, &cy);
      if (ox == old->tox && oy == old->toy) {
        t = *old;  // still, or still gliding to where it is
      } else if (std::abs(cx - static_cast<float>(ox)) <= kMaxStep &&
                 std::abs(cy - static_cast<float>(oy)) <= kMaxStep) {
        double step = old->step;
        const double glide = GlideTime(&step, now - old->lastmove, refresh);
        t = Track{sp.id, cx, cy, ox, oy, now, now + glide, now, step};
      }
    }
    next.push_back(t);
  }
  tracks_.swap(next);
}

bool SpriteMotion::Position(const SceneSprite& sp, double now, float* x, float* y) const {
  const Track* t = Find(sp.id);
  if (!t) return false;
  Eval(*t, now, x, y);
  *x += static_cast<float>(sp.artx);
  *y += static_cast<float>(sp.arty);
  return true;
}

}  // namespace keen4::render
