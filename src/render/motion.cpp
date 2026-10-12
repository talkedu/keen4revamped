#include "render/motion.h"

#include <cmath>

namespace keen4::render {

void SpriteMotion::Reset() {
  tracks_.clear();
  have_camera_ = false;
  map_ = -1;
  last_time_ = -1;
  have_offset_ = false;
}

const SpriteMotion::Track* SpriteMotion::Find(const void* id) const {
  for (const Track& t : tracks_)
    if (t.id == id) return &t;
  return nullptr;
}

void SpriteMotion::Eval(const Track& t, double time, float* x, float* y) {
  const Glide& g = time < t.cur.t0 ? t.prev : t.cur;
  if (time >= g.t1 || g.t1 <= g.t0) {
    *x = static_cast<float>(g.tox);
    *y = static_cast<float>(g.toy);
    return;
  }
  float a = time <= g.t0 ? 0.0f : static_cast<float>((time - g.t0) / (g.t1 - g.t0));
  *x = g.fromx + (static_cast<float>(g.tox) - g.fromx) * a;
  *y = g.fromy + (static_cast<float>(g.toy) - g.fromy) * a;
}

namespace {

// The time between moves is a whole number of frames, so a slug stepping
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

SpriteMotion::Track SpriteMotion::Follow(const Track* old, const void* id, int x, int y, double time,
                                         double refresh) const {
  const Glide still{static_cast<float>(x), static_cast<float>(y), x, y, time, time};
  const Track fresh{id, still, still, time, 0};
  if (!old) return fresh;
  if (x == old->cur.tox && y == old->cur.toy) return *old;  // still, or still gliding to where it is
  // the move happened during the tics since the last scene
  const double t0 = time - refresh;
  float cx, cy;
  Eval(*old, t0, &cx, &cy);
  if (std::abs(cx - static_cast<float>(x)) > kMaxStep || std::abs(cy - static_cast<float>(y)) > kMaxStep)
    return fresh;
  double step = old->step, glide;
  if (refresh > 0 && old->lastmove == last_time_) {
    step = glide = refresh;  // moving in every scene: over exactly this scene's tics
  } else {
    glide = GlideTime(&step, time - old->lastmove, refresh);
  }
  const Glide& before = t0 >= old->cur.t0 ? old->cur : old->prev;
  return Track{id, Glide{cx, cy, x, y, t0, t0 + glide}, before, time, step};
}

void SpriteMotion::Update(const Scene& scene, double now) {
  if (scene.map != map_) {
    tracks_.clear();
    have_camera_ = false;
    last_time_ = -1;
    map_ = scene.map;
  }
  const double off = now - scene.time;
  if (!have_offset_ || off < offset_ || off > offset_ + 2.0 / 70) offset_ = off;
  have_offset_ = true;
  const double refresh = last_time_ >= 0 && scene.time > last_time_ ? scene.time - last_time_ : 0;
  std::vector<Track> next;
  next.reserve(scene.sprites.size());
  for (const SceneSprite& sp : scene.sprites)
    next.push_back(Follow(Find(sp.id), sp.id, sp.x - sp.artx, sp.y - sp.arty, scene.time, refresh));
  tracks_.swap(next);
  camera_ = Follow(have_camera_ ? &camera_ : nullptr, nullptr, scene.viewx, scene.viewy, scene.time, refresh);
  have_camera_ = true;
  last_time_ = scene.time;
}

bool SpriteMotion::Camera(double now, float* x, float* y) const {
  if (!have_camera_) return false;
  Eval(camera_, ViewTime(now), x, y);
  return true;
}

bool SpriteMotion::Position(const SceneSprite& sp, double now, float* x, float* y) const {
  const Track* t = Find(sp.id);
  if (!t) return false;
  Eval(*t, ViewTime(now), x, y);
  *x += static_cast<float>(sp.artx);
  *y += static_cast<float>(sp.arty);
  return true;
}

}  // namespace keen4::render
