#pragma once

// The remaster renderer: draws the recorded scene with new art at the
// window's native resolution, in a 356x200 (16:9) view, moving the camera and
// sprites smoothly between game frames. Whatever the game draws over the play
// view (status window, dialogs) and every screen without one (menus, title,
// help) is shown from the classic 320x200 frame.

#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "render/art.h"
#include "render/motion.h"
#include "render/scene.h"

namespace keen4::platform {

class RemasterRenderer {
 public:
  RemasterRenderer(SDL_Renderer* renderer, render::ArtLibrary* art);
  ~RemasterRenderer();

  // Takes the latest scene if the game produced a new one since the last call.
  void Update(const render::Scene* scene, Uint64 now_ns);
  // Draws a frame for time `now_ns`. `pixels`/`palette` are the classic frame.
  void Draw(const std::uint8_t* pixels, const std::uint32_t* palette, Uint64 now_ns);

  void DropTextures();
  bool interpolate = true;
  // Debug overlay for artists: each sprite's original hitbox (red) over the
  // outline of its art, coloured by the art folder it came from.
  bool show_hitboxes = false;
  // Softer picture: blurs single-pixel detail that flickers in motion on some
  // displays.
  bool soft = false;

 private:
  SDL_Texture* Texture(render::AssetKind kind, int number, const render::ArtImage** image);
  void DrawScene(double now);
  // Builds textures for art the scene may need soon, for up to `budget_ns`.
  void Prepare(Uint64 budget_ns);
  void DrawClassicFrame(const std::uint8_t* pixels, const std::uint32_t* palette, bool overlay_only);

  SDL_Renderer* renderer_;
  render::ArtLibrary* art_;
  std::uint64_t art_generation_ = 0;
  struct CachedTexture {
    SDL_Texture* texture;
    std::uint32_t revision;
  };
  std::map<std::pair<int, int>, CachedTexture> textures_;
  SDL_Texture* classic_ = nullptr;
  SDL_Texture* view_ = nullptr;  // the scene at art density, scaled to the window
  float view_density_ = 0;
  std::vector<std::uint32_t> classic_pixels_;

  render::Scene prev_, cur_;
  bool have_prev_ = false, have_cur_ = false;
  render::SpriteMotion motion_;
  float snap_ = 1;  // device pixels per view pixel, to keep tiles on whole pixels
};

}  // namespace keen4::platform
