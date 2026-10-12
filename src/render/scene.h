#pragma once

// The per-frame scene list.
//
// Every RF_Refresh records what it is about to draw: the camera, the tiles
// around the view and the sprites in draw order. The classic renderer keeps
// drawing the 320x200 frame exactly as before; the remaster renderer redraws
// the same scene with new art at any resolution, a wider 16:9 view and smooth
// motion between frames. Nothing here feeds back into the game.

#include <array>
#include <cstdint>
#include <vector>

namespace keen4::render {

inline constexpr int kClassicWidth = 320;
inline constexpr int kClassicHeight = 200;
// The remaster view: 16:9 at the original height, centred on the classic view.
inline constexpr int kWideWidth = 356;
inline constexpr int kWideMargin = (kWideWidth - kClassicWidth) / 2;

struct SceneSprite {
  const void* id;        // stays the same while an object keeps its sprite
  int chunk;             // sprite chunk number
  int x, y;              // image top-left in world pixels, before the classic shift rounding
  int artx, arty;        // image top-left relative to the object's own position (the art's origin)
  int priority;          // 0..3
  bool inverse;          // drawn as an inverse mask (maskdraw)
};

struct Scene {
  std::uint64_t serial = 0;  // increases with every captured frame
  double time = 0;           // game time the scene shows, in seconds of the 70 Hz clock
  int map = -1;              // map number, to tell levels apart
  int viewx = 0, viewy = 0;  // camera: world pixel at the classic view's top-left
  // World x range the classic camera can show (the map border stays hidden).
  int minx = 0, maxx = 0;
  // Tiles covering the wide view, cols x rows starting at tile (tilex0, tiley0).
  int tilex0 = 0, tiley0 = 0, cols = 0, rows = 0;
  std::vector<std::uint16_t> bg, fg;
  std::vector<std::uint8_t> fgfront;  // foreground tile drawn over sprites below priority 3
  std::vector<SceneSprite> sprites;   // in draw order
  // Art the view may need soon: background and foreground tiles in a band
  // around the view, and every sprite the level has loaded. Building new art
  // the moment it first shows stalls the frame, so the renderer prepares these
  // a little at a time.
  std::vector<std::uint16_t> nearbg, nearfg, loadedsprites;
  // The classic frame of this scene before anything else is drawn over it, to
  // find overlays (status window, dialogs) in later frames.
  std::array<std::uint8_t, kClassicWidth * kClassicHeight> classic{};
};

// Called by ID_RF after it has drawn the tiles and sprites of a frame.
Scene& BeginCapture();
void EndCapture();

// The most recent complete scene, or nullptr before the first one.
const Scene* LatestScene();
// Forget the scene, e.g. when a screen without the play view is shown.
void InvalidateScene();

}  // namespace keen4::render
