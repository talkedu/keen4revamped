#pragma once

// Smooth sprite motion for the remaster renderer.
//
// Many actors move in steps: the poison slug moves 4 pixels every 8 tics and
// the inchworm 8 pixels every 30, standing still in between. Blending only
// between the last two scenes moves them in quick hops. Instead each sprite
// glides from where it is drawn to its new position over the time its steps
// usually take, so step movers walk at a steady speed one step behind the
// game, and actors that move every frame trail by one frame as before.
//
// What glides is the object's own position; each frame's art offset goes on
// top, so frames whose art sits at different offsets (the inchworm's stretch
// and hump) don't slide the image back and forth.
//
// The camera glides by the same rules. While Keen runs, he and the camera move
// in the same scenes; on different clocks he would wobble against the
// scrolling background, a few pixels back and forth every frame.

#include <vector>

#include "render/scene.h"

namespace keen4::render {

class SpriteMotion {
 public:
  // Bigger jumps than this are teleports or new objects and are not smoothed.
  static constexpr int kMaxStep = 48;
  // No glide takes longer than this; slower steppers hop.
  static constexpr double kMaxGlide = 0.5;

  void Reset();
  // A new scene arrived at time `now` (seconds).
  void Update(const Scene& scene, double now);
  // Where to draw sp's image top-left at time `now`. Returns false, leaving x
  // and y alone, for a sprite the last Update did not see.
  bool Position(const SceneSprite& sp, double now, float* x, float* y) const;
  // Where to put the camera (the classic view's top-left) at time `now`.
  // Returns false, leaving x and y alone, before the first Update.
  bool Camera(double now, float* x, float* y) const;

 private:
  struct Track {
    const void* id;
    float fromx, fromy;  // object position the glide starts from
    int tox, toy;        // object position in the latest scene
    double t0, t1;       // glide start and end
    double lastmove;     // when the object last moved in the game
    double step;         // usual time between its moves, 0 if not known yet
  };
  const Track* Find(const void* id) const;
  static void Eval(const Track& t, double now, float* x, float* y);
  // The track of an object now at (x, y), going on from `old` if it had one.
  static Track Follow(const Track* old, const void* id, int x, int y, double now, double refresh);

  std::vector<Track> tracks_;
  Track camera_{};
  bool have_camera_ = false;
  int map_ = -1;
  double last_update_ = -1;
};

}  // namespace keen4::render
