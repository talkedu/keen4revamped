#include "render/motion.h"

#include <cmath>

namespace keen4::render {

void SpriteMotion::Reset() {
  tracks_.clear();
  have_camera_ = false;
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

SpriteMotion::Track SpriteMotion::Follow(const Track* old, const void* id, int x, int y, double now,
                                         double refresh) {
  Track t{id, static_cast<float>(x), static_cast<float>(y), x, y, now, now, now, 0};
  if (!old) return t;
  if (x == old->tox && y == old->toy) return *old;  // still, or still gliding to where it is
  float cx, cy;
  Eval(*old, now, &cx, &cy);
  if (std::abs(cx - static_cast<float>(x)) > kMaxStep || std::abs(cy - static_cast<float>(y)) > kMaxStep)
    return t;
  double step = old->step;
  const double glide = GlideTime(&step, now - old->lastmove, refresh);
  return Track{id, cx, cy, x, y, now, now + glide, now, step};
}

void SpriteMotion::Update(const Scene& scene, double now) {
  if (scene.map != map_) {
    tracks_.clear();
    have_camera_ = false;
    map_ = scene.map;
  }
  const double refresh = last_update_ >= 0 && now > last_update_ ? now - last_update_ : 0;
  std::vector<Track> next;
  next.reserve(scene.sprites.size());
  for (const SceneSprite& sp : scene.sprites)
    next.push_back(Follow(Find(sp.id), sp.id, sp.x - sp.artx, sp.y - sp.arty, now, refresh));
  tracks_.swap(next);
  camera_ = Follow(have_camera_ ? &camera_ : nullptr, nullptr, scene.viewx, scene.viewy, now, refresh);
  have_camera_ = true;
  last_update_ = now;
}

bool SpriteMotion::Camera(double now, float* x, float* y) const {
  if (!have_camera_) return false;
  Eval(camera_, now, x, y);
  return true;
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
