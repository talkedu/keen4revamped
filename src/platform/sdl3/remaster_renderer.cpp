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
  if (s.views.empty()) {
    const bool lerp = have_prev_ && prev_.map == s.map && prev_.views.empty() &&
                      std::abs(prev_.viewx - s.viewx) <= kMaxLerp && std::abs(prev_.viewy - s.viewy) <= kMaxLerp;
    View view;
    view.camx = lerp ? Lerp(static_cast<float>(prev_.viewx), static_cast<float>(s.viewx), alpha) : s.viewx;
    view.camy = lerp ? Lerp(static_cast<float>(prev_.viewy), static_cast<float>(s.viewy), alpha) : s.viewy;
    view.classicw = render::kClassicWidth;
    view.rect = SDL_FRect{0, 0, kViewW, kViewH};
    view.tilex0 = s.tilex0;
    view.tiley0 = s.tiley0;
    view.cols = s.cols;
    view.rows = s.rows;
    view.bg = &s.bg;
    view.fg = &s.fg;
    view.fgfront = &s.fgfront;
    DrawView(view, lerp, now);
    return;
  }

  // Split screen (co-op): each view in its rectangle, which is as much wider
  // than its classic one as the whole remaster view is, then the divider.
  const bool sameviews = have_prev_ && prev_.map == s.map && prev_.views.size() == s.views.size();
  for (std::size_t i = 0; i < s.views.size(); i++) {
    const render::SceneView& sv = s.views[i];
    if (sv.w <= 0 || sv.h <= 0) continue;
    const bool lerp = sameviews && std::abs(prev_.views[i].viewx - sv.viewx) <= kMaxLerp &&
                      std::abs(prev_.views[i].viewy - sv.viewy) <= kMaxLerp;
    View view;
    view.index = static_cast<int>(i);
    view.camx = lerp ? Lerp(static_cast<float>(prev_.views[i].viewx), static_cast<float>(sv.viewx), alpha) : sv.viewx;
    view.camy = lerp ? Lerp(static_cast<float>(prev_.views[i].viewy), static_cast<float>(sv.viewy), alpha) : sv.viewy;
    view.classicw = sv.w;
    float x0 = WideX(sv.x), x1 = WideX(sv.x + sv.w);
    view.rect = SDL_FRect{x0, static_cast<float>(sv.y), x1 - x0, static_cast<float>(sv.h)};
    view.tilex0 = sv.tilex0;
    view.tiley0 = sv.tiley0;
    view.cols = sv.cols;
    view.rows = sv.rows;
    view.bg = &sv.bg;
    view.fg = &sv.fg;
    view.fgfront = &sv.fgfront;
    DrawView(view, lerp, now);
  }
  if (s.views.size() == 2 && s.views[0].w > 0 && s.views[1].w > 0 && s.views[0].h > 0 && s.views[1].h > 0) {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_FRect divider = s.views[1].x > s.views[0].x
                            ? SDL_FRect{WideX(s.views[1].x) - 1, 0, 2, kViewH}
                            : SDL_FRect{0, static_cast<float>(s.views[1].y) - 1, kViewW, 2};
    SDL_RenderFillRect(renderer_, &divider);
  }
}

// A classic x on the remaster view (the whole classic width maps to the whole
// remaster width, so the halves of a split screen are 178 pixels wide).
float RemasterRenderer::WideX(int classicx) {
  return static_cast<float>(classicx) * kViewW / static_cast<float>(render::kClassicWidth);
}

void RemasterRenderer::DrawView(const View& v, bool lerp, double now) {
  const render::Scene& s = cur_;
  // the classic view is centred in the rectangle
  const float margin = (v.rect.w - static_cast<float>(v.classicw)) / 2;
  float camx = v.camx, camy = v.camy;
  // the wider view must not show the map border the classic camera never shows
  if (s.maxx - s.minx >= v.rect.w) {
    camx = std::max(camx, s.minx + margin);
    camx = std::min(camx, s.maxx - v.classicw - margin);
  }
  // keep the tile grid on whole device pixels so no seams show between tiles
  camx = std::round(camx * snap_) / snap_;
  camy = std::round(camy * snap_) / snap_;
  const float offx = v.rect.x + margin - camx, offy = v.rect.y - camy;
  const bool clip = v.index >= 0;
  if (clip) {
    SDL_Rect r{static_cast<int>(std::lround(v.rect.x)), static_cast<int>(std::lround(v.rect.y)),
               static_cast<int>(std::lround(v.rect.w)), static_cast<int>(std::lround(v.rect.h))};
    SDL_SetRenderClipRect(renderer_, &r);
  }

  struct Outline {
    SDL_FRect art;
    float x, y;  // the original image's top-left in the view
    int chunk, layer;
  };
  std::vector<Outline> outlines;

  auto drawImage = [&](render::AssetKind kind, int number, float wx, float wy, const std::uint8_t* data = nullptr) {
    const render::ArtImage* img = nullptr;
    SDL_Texture* tex = data ? DataTexture(number, data, &img) : Texture(kind, number, &img);
    if (!tex || img->origw == 0) return;
    float density = static_cast<float>(img->width) / static_cast<float>(img->origw);
    if (img->fromManifest) density = 4;
    SDL_FRect dst{wx + offx + img->ox, wy + offy + img->oy, img->width / density, img->height / density};
    if (show_hitboxes && (kind == render::AssetKind::Sprite || kind == render::AssetKind::Sprite2))
      outlines.push_back({dst, wx + offx, wy + offy, number, img->layer});
    if (dst.x > v.rect.x + v.rect.w || dst.y > v.rect.y + v.rect.h || dst.x + dst.w < v.rect.x ||
        dst.y + dst.h < v.rect.y)
      return;
    // snap both edges to device pixels, so neighbouring tiles meet without gaps
    float x0 = std::round(dst.x * snap_), y0 = std::round(dst.y * snap_);
    float x1 = std::round((dst.x + dst.w) * snap_), y1 = std::round((dst.y + dst.h) * snap_);
    dst = SDL_FRect{x0 / snap_, y0 / snap_, (x1 - x0) / snap_, (y1 - y0) / snap_};
    SDL_RenderTexture(renderer_, tex, nullptr, &dst);
  };

  auto drawTiles = [&](bool frontonly) {
    for (int ty = 0; ty < v.rows; ty++) {
      for (int tx = 0; tx < v.cols; tx++) {
        std::size_t i = static_cast<std::size_t>(ty) * v.cols + tx;
        float wx = static_cast<float>((v.tilex0 + tx) * 16), wy = static_cast<float>((v.tiley0 + ty) * 16);
        if (frontonly) {
          if ((*v.fgfront)[i]) drawImage(render::AssetKind::Tile16M, (*v.fg)[i], wx, wy);
          continue;
        }
        drawImage(render::AssetKind::Tile16, (*v.bg)[i], wx, wy);
        if ((*v.fg)[i]) drawImage(render::AssetKind::Tile16M, (*v.fg)[i], wx, wy);
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
    if (sp.view >= 0 && sp.view != v.index) continue;
    float x = static_cast<float>(sp.x), y = static_cast<float>(sp.y);
    if (lerp && interpolate) motion_.Position(sp, now, &x, &y);
    // Tom (co-op) has sprites of his own
    render::AssetKind kind = sp.variant == 1 && !sp.data ? render::AssetKind::Sprite2 : render::AssetKind::Sprite;
    drawImage(kind, sp.chunk, x, y, sp.data);
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
  if (clip) SDL_SetRenderClipRect(renderer_, nullptr);
}

SDL_Texture* RemasterRenderer::DataTexture(int chunk, const std::uint8_t* data, const render::ArtImage** image) {
  const render::ArtImage& img = art_->GetData(chunk, data);
  *image = &img;
  auto key = std::make_pair(-1, chunk);
  auto found = textures_.find(key);
  if (found != textures_.end()) {
    if (found->second.revision == img.revision) return found->second.texture;
    if (found->second.texture) SDL_DestroyTexture(found->second.texture);
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
