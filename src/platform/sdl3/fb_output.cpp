#include "platform/sdl3/fb_output.h"

#include <cstdio>
#include <cstdlib>

#if defined(__linux__) && !defined(__ANDROID__)
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#define KEEN4_HAVE_FBDEV 1
#endif

namespace keen4::platform {

FramebufferOutput::~FramebufferOutput() {
  if (surface_) SDL_DestroySurface(surface_);
#ifdef KEEN4_HAVE_FBDEV
  if (map_) munmap(map_, map_size_);
  if (fd_ >= 0) close(fd_);
#endif
}

bool FramebufferOutput::Open() {
#ifdef KEEN4_HAVE_FBDEV
  const char* device = std::getenv("KEEN4_FBDEV");
  if (!device || !*device) device = "/dev/fb0";
  fd_ = open(device, O_RDWR | O_CLOEXEC);
  if (fd_ < 0) return false;
  fb_var_screeninfo var{};
  fb_fix_screeninfo fix{};
  int w = 0, h = 0;
  if (const char* mode = std::getenv("KEEN4_FBDEV_MODE"); mode && std::sscanf(mode, "%dx%d", &w, &h) == 2) {
    // Testing without a device: a plain file of WxH XRGB8888 pixels stands in.
    var.xres = static_cast<__u32>(w);
    var.yres = static_cast<__u32>(h);
    var.bits_per_pixel = 32;
    var.red = {16, 8, 0};
    var.green = {8, 8, 0};
    var.blue = {0, 8, 0};
    fix.line_length = static_cast<__u32>(w * 4);
    fix.smem_len = static_cast<__u32>(w * h * 4);
    if (ftruncate(fd_, fix.smem_len) != 0) return false;
  } else if (ioctl(fd_, FBIOGET_VSCREENINFO, &var) != 0 || ioctl(fd_, FBIOGET_FSCREENINFO, &fix) != 0) {
    std::fprintf(stderr, "video: %s: cannot read the screen mode\n", device);
    return false;
  }
  // The mode is used as it is: these drivers reject most changes.
  auto mask = [](const fb_bitfield& f) -> Uint32 {
    return f.length ? ((f.length >= 32 ? 0xFFFFFFFFu : ((1u << f.length) - 1)) << f.offset) : 0;
  };
  format_ = SDL_GetPixelFormatForMasks(static_cast<int>(var.bits_per_pixel), mask(var.red), mask(var.green),
                                       mask(var.blue), mask(var.transp));
  if (format_ == SDL_PIXELFORMAT_UNKNOWN) {
    std::fprintf(stderr, "video: %s: unsupported pixel format (%u bits)\n", device, var.bits_per_pixel);
    return false;
  }
  width_ = static_cast<int>(var.xres);
  height_ = static_cast<int>(var.yres);
  pitch_ = static_cast<int>(fix.line_length);
  offset_ = static_cast<size_t>(var.yoffset) * fix.line_length + var.xoffset * (var.bits_per_pixel / 8);
  map_size_ = fix.smem_len;
  void* map = mmap(nullptr, map_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (map == MAP_FAILED || offset_ + static_cast<size_t>(pitch_) * height_ > map_size_) {
    std::fprintf(stderr, "video: %s: cannot map the framebuffer\n", device);
    if (map != MAP_FAILED) munmap(map, map_size_);
    return false;
  }
  map_ = static_cast<unsigned char*>(map);
  surface_ = SDL_CreateSurface(width_, height_, SDL_PIXELFORMAT_XRGB8888);
  if (!surface_) return false;
  std::fprintf(stderr, "video: framebuffer %s, %dx%d, %s\n", device, width_, height_,
               SDL_GetPixelFormatName(format_));
  return true;
#else
  return false;
#endif
}

void FramebufferOutput::Show() {
  if (!map_ || !surface_) return;
  // An alpha channel in the framebuffer gets full opacity.
  SDL_ConvertPixels(width_, height_, surface_->format, surface_->pixels, surface_->pitch, format_, map_ + offset_,
                    pitch_);
}

}  // namespace keen4::platform
