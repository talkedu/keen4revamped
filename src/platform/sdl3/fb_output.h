#pragma once

#include <SDL3/SDL.h>

namespace keen4::platform {

// The Linux framebuffer (/dev/fb0), for devices SDL3 cannot draw on: the
// stock firmware of the Anbernic H700 handhelds (RG40XX, RG35XX Plus and the
// like) has neither KMS/DRM nor Wayland, only fbdev. The game then renders in
// software into a surface the size of the screen, and each frame is copied
// to the framebuffer in its own pixel format.
class FramebufferOutput {
 public:
  ~FramebufferOutput();

  // Opens the framebuffer (KEEN4_FBDEV names another device). False without one.
  bool Open();
  int width() const { return width_; }
  int height() const { return height_; }

  // A surface the size of the screen to render into (SDL_CreateSoftwareRenderer).
  SDL_Surface* surface() const { return surface_; }

  // Copies the surface to the screen.
  void Show();

 private:
  int fd_ = -1;
  unsigned char* map_ = nullptr;
  size_t map_size_ = 0;
  size_t offset_ = 0;  // the visible page, when the framebuffer is double-buffered
  int pitch_ = 0;
  int bytes_per_pixel_ = 0;
  Uint32 alpha_mask_ = 0;  // set in every pixel written
  bool test_ = false;      // a plain file stands in (KEEN4_FBDEV_MODE)
  bool pan_failed_ = false;
  int width_ = 0, height_ = 0;
  SDL_PixelFormat format_ = SDL_PIXELFORMAT_UNKNOWN;
  SDL_Surface* surface_ = nullptr;
};

}  // namespace keen4::platform
