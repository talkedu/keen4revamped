#pragma once

// Smooth sprite motion for the remaster renderer.
//
// Motion follows the game's own clock, not the time scenes reach the screen.
// The game moves its actors by the tics that passed, and on a 60 Hz display it
// draws every second refresh with 2 or 3 tics between frames. Gliding each
// move over the wall-clock time between frames plays equal stretches of time
// at two speeds, and the whole view shudders. Here each move glides over the
// game time it covers, and the view is drawn kLag behind the game so the next
// move is nearly always known before it is needed.
//
// Many actors move in steps: the poison slug moves 4 pixels every 8 tics and
// the inchworm 8 pixels every 30, standing still in between. Those glide from
// where they are drawn to their new position over the time their steps usually
// take, so step movers walk at a steady speed one step behind the game.
// Anything that moved in the previous scene too (Keen running, the camera
// following him) glides over exactly that scene's tics, so they stay in step.
//
// What glides is the object's own position; each frame's art offset goes on
// top, so frames whose art sits at different offsets (the inchworm's stretch
// and hump) don't slide the image back and forth.

#include <vector>

#include "render/scene.h"

namespace keen4::render {

class SpriteMotion {
 public:
  // Bigger jumps than this are teleports or new objects and are not smoothed.
  static constexpr int kMaxStep = 48;
  // No glide takes longer than this; slower steppers hop.
  static constexpr double kMaxGlide = 0.5;
  // How far behind the game the view is drawn: the longest usual time between
  // frames (3 tics) plus the tic the game clock is rounded to.
  static constexpr double kLag = 4.0 / 70;

  void Reset();
  // A new scene, showing the game at scene.time, arrived at wall time `now`
  // (seconds).
  void Update(const Scene& scene, double now);
  // Where to draw sp's image top-left at wall time `now`. Returns false,
  // leaving x and y alone, for a sprite the last Update did not see.
  bool Position(const SceneSprite& sp, double now, float* x, float* y) const;
  // Where to put the camera (the classic view's top-left) at wall time `now`.
  // Returns false, leaving x and y alone, before the first Update.
  bool Camera(double now, float* x, float* y) const;

 private:
  struct Glide {
    float fromx, fromy;  // object position the glide starts from
    int tox, toy;        // where it ends
    double t0, t1;       // game time of start and end
  };
  struct Track {
    const void* id;
    Glide cur, prev;  // prev covers game times before cur.t0
    double lastmove;  // game time the object last moved
    double step;      // usual time between its moves, 0 if not known yet
  };
  const Track* Find(const void* id) const;
  static void Eval(const Track& t, double time, float* x, float* y);
  // The track of an object at (x, y) in the scene at game time `time`.
  Track Follow(const Track* old, const void* id, int x, int y, double time, double refresh) const;
  // The game time to draw at wall time `now`.
  double ViewTime(double now) const { return now - offset_ - kLag; }

  std::vector<Track> tracks_;
  Track camera_{};
  bool have_camera_ = false;
  int map_ = -1;
  double last_time_ = -1;  // game time of the last scene
  // Wall time minus game time. The game clock counts whole tics and is held
  // back after a long frame, so keep the smallest difference seen and start
  // over when it grows by more than a couple of tics.
  double offset_ = 0;
  bool have_offset_ = false;
};

}  // namespace keen4::render
