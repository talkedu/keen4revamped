#include "platform/sdl3/remaster_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace keen4::platform {

namespace {

constexpr float kViewW = static_cast<float>(render::kWideWidth);
constexpr float kViewH = static_cast<float>(render::kClassicHeight);
constexpr float kMargin = static_cast<float>(render::kWideMargin);
// Overlays covering more than this share of the frame mean a different screen
// (menu, help, map dialog in front of everything): show the classic frame.
constexpr int kOverlayLimit = render::kClassicWidth * render::kClassicHeight * 2 / 5;
// Bigger jumps than this are teleports or new objects, never interpolated.
constexpr int kMaxLerp = 48;

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

double Seconds(Uint64 ns) { return static_cast<double>(ns) * 1e-9; }

}  // namespace

RemasterRenderer::RemasterRenderer(SDL_Renderer* renderer, render::ArtLibrary* art)
    : renderer_(renderer), art_(art) {}

RemasterRenderer::~RemasterRenderer() {
  DropTextures();
  if (classic_) SDL_DestroyTexture(classic_);
}

void RemasterRenderer::DropTextures() {
  for (auto& [key, cached] : textures_)
    if (cached.texture) SDL_DestroyTexture(cached.texture);
  textures_.clear();
}

void RemasterRenderer::Update(const render::Scene* scene, Uint64 now_ns) {
  if (!scene) {
    have_cur_ = have_prev_ = false;
    motion_.Reset();
    return;
  }
  if (have_cur_ && scene->serial == cur_.serial) return;
  if (have_cur_) {
    prev_ = cur_;
    prev_ns_ = cur_ns_;
    have_prev_ = true;
  }
  cur_ = *scene;
  cur_ns_ = now_ns;
  have_cur_ = true;
  motion_.Update(cur_, Seconds(now_ns));
}

SDL_Texture* RemasterRenderer::Texture(render::AssetKind kind, int number, const render::ArtImage** image) {
  if (art_->generation() != art_generation_) {
    DropTextures();
    art_generation_ = art_->generation();
  }
  const render::ArtImage& img = art_->Get(kind, number);
  *image = &img;
  auto key = std::make_pair(static_cast<int>(kind), number);
  auto found = textures_.find(key);
  if (found != textures_.end()) {
    if (found->second.revision == img.revision) return found->second.texture;
    if (found->second.texture) SDL_DestroyTexture(found->second.texture);  // a live asset changed
    textures_.erase(found);
  }
  SDL_Texture* tex = nullptr;
  if (img.width > 0) {
    tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (tex) {
      SDL_UpdateTexture(tex, nullptr, img.pixels.data(), img.width * 4);
      SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
      SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    }
  }
  textures_[key] = CachedTexture{tex, img.revision};
  return tex;
}

void RemasterRenderer::DrawScene(float alpha, double now) {
  const render::Scene& s = cur_;
  const bool lerp = have_prev_ && prev_.map == s.map && std::abs(prev_.viewx - s.viewx) <= kMaxLerp &&
                    std::abs(prev_.viewy - s.viewy) <= kMaxLerp;
  float camx = lerp ? Lerp(static_cast<float>(prev_.viewx), static_cast<float>(s.viewx), alpha) : s.viewx;
  float camy = lerp ? Lerp(static_cast<float>(prev_.viewy), static_cast<float>(s.viewy), alpha) : s.viewy;
  // the wider view must not show the map border the classic camera never shows
  if (s.maxx - s.minx >= render::kWideWidth) {
    camx = std::max(camx, s.minx + kMargin);
    camx = std::min(camx, s.maxx - render::kClassicWidth - kMargin);
  }
  // keep the tile grid on whole device pixels so no seams show between tiles
  camx = std::round(camx * snap_) / snap_;
  camy = std::round(camy * snap_) / snap_;

  struct Outline {
    SDL_FRect art;
    float x, y;  // the original image's top-left in the view
    int chunk, layer;
  };
  std::vector<Outline> outlines;

  auto drawImage = [&](render::AssetKind kind, int number, float wx, float wy) {
    const render::ArtImage* img = nullptr;
    SDL_Texture* tex = Texture(kind, number, &img);
    if (!tex || img->origw == 0) return;
    float density = static_cast<float>(img->width) / static_cast<float>(img->origw);
    if (img->fromManifest) density = 4;
    SDL_FRect dst{wx - camx + kMargin + img->ox, wy - camy + img->oy, img->width / density,
                  img->height / density};
    if (show_hitboxes && kind == render::AssetKind::Sprite)
      outlines.push_back({dst, wx - camx + kMargin, wy - camy, number, img->layer});
    if (dst.x > kViewW || dst.y > kViewH || dst.x + dst.w < 0 || dst.y + dst.h < 0) return;
    // snap both edges to device pixels, so neighbouring tiles meet without gaps
    float x0 = std::round(dst.x * snap_), y0 = std::round(dst.y * snap_);
    float x1 = std::round((dst.x + dst.w) * snap_), y1 = std::round((dst.y + dst.h) * snap_);
    dst = SDL_FRect{x0 / snap_, y0 / snap_, (x1 - x0) / snap_, (y1 - y0) / snap_};
    SDL_RenderTexture(renderer_, tex, nullptr, &dst);
  };

  auto drawTiles = [&](bool frontonly) {
    for (int ty = 0; ty < s.rows; ty++) {
      for (int tx = 0; tx < s.cols; tx++) {
        std::size_t i = static_cast<std::size_t>(ty) * s.cols + tx;
        float wx = static_cast<float>((s.tilex0 + tx) * 16), wy = static_cast<float>((s.tiley0 + ty) * 16);
        if (frontonly) {
          if (s.fgfront[i]) drawImage(render::AssetKind::Tile16M, s.fg[i], wx, wy);
          continue;
        }
        drawImage(render::AssetKind::Tile16, s.bg[i], wx, wy);
        if (s.fg[i]) drawImage(render::AssetKind::Tile16M, s.fg[i], wx, wy);
      }
    }
  };

  drawTiles(false);
  bool frontdone = false;
  for (const render::SceneSprite& sp : s.sprites) {
    if (sp.priority == 3 && !frontdone) {
      drawTiles(true);
      frontdone = true;
    }
    if (sp.inverse) continue;  // drawn from the classic frame
    float x = static_cast<float>(sp.x), y = static_cast<float>(sp.y);
    if (lerp && interpolate) motion_.Position(sp, now, &x, &y);
    drawImage(render::AssetKind::Sprite, sp.chunk, x, y);
  }
  if (!frontdone) drawTiles(true);

  // Art outline: yellow for the first (curated) folder, cyan for later ones,
  // grey for the built-in upscale. Hitbox in red, from the original table.
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
  for (const Outline& o : outlines) {
    if (o.layer == 0)
      SDL_SetRenderDrawColor(renderer_, 255, 220, 0, 220);
    else if (o.layer > 0)
      SDL_SetRenderDrawColor(renderer_, 0, 220, 255, 220);
    else
      SDL_SetRenderDrawColor(renderer_, 160, 160, 160, 160);
    SDL_RenderRect(renderer_, &o.art);
    float x0, y0, x1, y1;
    if (!render::SpriteHitbox(o.chunk, &x0, &y0, &x1, &y1)) continue;
    SDL_FRect box{o.x + x0, o.y + y0, x1 - x0, y1 - y0};
    SDL_SetRenderDrawColor(renderer_, 255, 40, 40, 255);
    SDL_RenderRect(renderer_, &box);
  }
}

void RemasterRenderer::DrawClassicFrame(const std::uint8_t* pixels, const std::uint32_t* palette,
                                        bool overlay_only) {
  constexpr int w = render::kClassicWidth, h = render::kClassicHeight;
  if (!classic_) {
    classic_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_SetTextureScaleMode(classic_, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(classic_, SDL_BLENDMODE_BLEND);
    classic_pixels_.resize(static_cast<std::size_t>(w) * h);
  }
  // Overlay pixels are those that differ from the scene's own classic frame.
  // Inside a window some pixels happen to match the scene behind it; work in
  // 8-pixel spans and close small gaps so windows show as solid shapes.
  constexpr int spans = w / 8;
  std::vector<std::uint8_t> mark(static_cast<std::size_t>(spans) * h, overlay_only ? 0 : 1);
  if (overlay_only) {
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        if (pixels[y * w + x] != cur_.classic[static_cast<std::size_t>(y) * w + x]) mark[y * spans + x / 8] = 1;
    for (int s = 0; s < spans; s++) {
      int last = -100;
      for (int y = 0; y < h; y++) {
        if (!mark[y * spans + s]) continue;
        if (y - last > 1 && y - last <= 8)
          for (int g = last + 1; g < y; g++) mark[g * spans + s] = 1;
        last = y;
      }
    }
    for (int y = 0; y < h; y++) {
      int last = -100;
      for (int s = 0; s < spans; s++) {
        if (!mark[y * spans + s]) continue;
        if (s - last > 1 && s - last <= 3)
          for (int g = last + 1; g < s; g++) mark[y * spans + g] = 1;
        last = s;
      }
    }
  }
  for (int i = 0; i < w * h; i++) {
    bool show = mark[(i / w) * spans + (i % w) / 8];
    classic_pixels_[i] = show ? (0xFF000000u | palette[pixels[i] & 15]) : 0;
  }
  SDL_UpdateTexture(classic_, nullptr, classic_pixels_.data(), w * 4);
  SDL_FRect dst{kMargin, 0, static_cast<float>(w), static_cast<float>(h)};
  SDL_RenderTexture(renderer_, classic_, nullptr, &dst);
}

void RemasterRenderer::Draw(const std::uint8_t* pixels, const std::uint32_t* palette, Uint64 now_ns) {
  int outw = 0, outh = 0;
  SDL_GetCurrentRenderOutputSize(renderer_, &outw, &outh);
  snap_ = std::max(1.0f, std::min(outw / kViewW, outh / kViewH));

  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  SDL_RenderClear(renderer_);

  int overlay = 0;
  if (have_cur_) {
    for (std::size_t i = 0; i < cur_.classic.size(); i++)
      if (pixels[i] != cur_.classic[i]) overlay++;
  }

  if (!have_cur_ || overlay > kOverlayLimit) {
    DrawClassicFrame(pixels, palette, false);
    return;
  }

  float alpha = 1;
  if (interpolate && have_prev_ && cur_ns_ > prev_ns_) {
    alpha = static_cast<float>(now_ns - cur_ns_) / static_cast<float>(cur_ns_ - prev_ns_);
    alpha = std::clamp(alpha, 0.0f, 1.0f);
  }
  DrawScene(alpha, Seconds(now_ns));

  // palette fades: darken the new art by how dark the palette is
  float sum = 0, normal = 0;
  for (int i = 0; i < 16; i++) {
    std::uint32_t c = palette[i], d = render::EgaColor(i);
    sum += ((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255);
    normal += ((d >> 16) & 255) + ((d >> 8) & 255) + (d & 255);
  }
  float bright = normal > 0 ? std::min(1.0f, sum / normal) : 1;
  if (bright < 0.999f) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, static_cast<Uint8>((1 - bright) * 255));
    SDL_FRect all{0, 0, kViewW, kViewH};
    SDL_RenderFillRect(renderer_, &all);
  }

  if (overlay) DrawClassicFrame(pixels, palette, true);
}

}  // namespace keen4::platform
