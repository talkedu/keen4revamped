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
    test_ = true;
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
  bytes_per_pixel_ = static_cast<int>(var.bits_per_pixel / 8);
  alpha_mask_ = mask(var.transp);
  surface_ = SDL_CreateSurface(width_, height_, SDL_PIXELFORMAT_XRGB8888);
  if (!surface_) return false;
  std::fprintf(stderr, "video: framebuffer %s, %dx%d (virtual %ux%u at %u,%u), %s\n", device, width_, height_,
               var.xres_virtual, var.yres_virtual, var.xoffset, var.yoffset, SDL_GetPixelFormatName(format_));
  return true;
#else
  return false;
#endif
}

void FramebufferOutput::Show() {
  if (!map_ || !surface_) return;
#ifdef KEEN4_HAVE_FBDEV
  // Draw into the page on screen now, then pan to it: some drivers (Allwinner's
  // among them) only update the display layer on a pan.
  fb_var_screeninfo var{};
  bool real = !test_ && ioctl(fd_, FBIOGET_VSCREENINFO, &var) == 0;
  if (real) {
    size_t offset = static_cast<size_t>(var.yoffset) * pitch_ + var.xoffset * (var.bits_per_pixel / 8);
    if (offset + static_cast<size_t>(pitch_) * height_ <= map_size_) offset_ = offset;
  }
#endif
  unsigned char* dst = map_ + offset_;
  SDL_ConvertPixels(width_, height_, surface_->format, surface_->pixels, surface_->pitch, format_, dst, pitch_);
  // The rendered frame has no alpha; a framebuffer with an alpha channel
  // must get full opacity, or the display layer stays see-through.
  if (alpha_mask_ && bytes_per_pixel_ == 4) {
    for (int y = 0; y < height_; y++) {
      auto* row = reinterpret_cast<Uint32*>(dst + static_cast<size_t>(y) * pitch_);
      for (int x = 0; x < width_; x++) row[x] |= alpha_mask_;
    }
  } else if (alpha_mask_ && bytes_per_pixel_ == 2) {
    for (int y = 0; y < height_; y++) {
      auto* row = reinterpret_cast<Uint16*>(dst + static_cast<size_t>(y) * pitch_);
      for (int x = 0; x < width_; x++) row[x] = static_cast<Uint16>(row[x] | alpha_mask_);
    }
  }
#ifdef KEEN4_HAVE_FBDEV
  if (real && ioctl(fd_, FBIOPAN_DISPLAY, &var) != 0 && !pan_failed_) {
    pan_failed_ = true;
    std::fprintf(stderr, "video: the framebuffer does not pan; drawing without it\n");
  }
#endif
}

}  // namespace keen4::platform
