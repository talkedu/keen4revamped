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

 private:
  SDL_Texture* Texture(render::AssetKind kind, int number, const render::ArtImage** image);
  // A sprite drawn from the game's own graphics data (Tom's score box).
  SDL_Texture* DataTexture(int chunk, const std::uint8_t* data, const render::ArtImage** image);
  void DrawScene(float alpha, double now);
  // One view: the whole screen, or a split-screen half (co-op).
  struct View {
    int index = -1;            // the split-screen view, -1 for the whole screen
    float camx = 0, camy = 0;  // world pixel at the classic view's top-left
    int classicw = 0;          // the classic view's width; it is centred in rect
    SDL_FRect rect{};          // where it goes on the remaster view
    int tilex0 = 0, tiley0 = 0, cols = 0, rows = 0;
    const std::vector<std::uint16_t>* bg = nullptr;
    const std::vector<std::uint16_t>* fg = nullptr;
    const std::vector<std::uint8_t>* fgfront = nullptr;
  };
  void DrawView(const View& view, bool lerp, double now);
  static float WideX(int classicx);
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
  std::vector<std::uint32_t> classic_pixels_;

  render::Scene prev_, cur_;
  bool have_prev_ = false, have_cur_ = false;
  Uint64 prev_ns_ = 0, cur_ns_ = 0;
  render::SpriteMotion motion_;
  float snap_ = 1;  // device pixels per view pixel, to keep tiles on whole pixels
};

}  // namespace keen4::platform
