#include "platform/sdl3/sdl_platform.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "audio/remaster_audio.h"
#include "platform/sdl3/remaster_renderer.h"
#include "render/art.h"
#include "render/scene.h"

namespace keen4::platform {

namespace {

// SDL scancode -> XT set 1 scancode. Second value: needs the E0 prefix.
struct XtCode {
  int code;
  bool extended;
};

XtCode ToXt(SDL_Scancode sc) {
  switch (sc) {
    case SDL_SCANCODE_ESCAPE: return {0x01, false};
    case SDL_SCANCODE_1: return {0x02, false};
    case SDL_SCANCODE_2: return {0x03, false};
    case SDL_SCANCODE_3: return {0x04, false};
    case SDL_SCANCODE_4: return {0x05, false};
    case SDL_SCANCODE_5: return {0x06, false};
    case SDL_SCANCODE_6: return {0x07, false};
    case SDL_SCANCODE_7: return {0x08, false};
    case SDL_SCANCODE_8: return {0x09, false};
    case SDL_SCANCODE_9: return {0x0A, false};
    case SDL_SCANCODE_0: return {0x0B, false};
    case SDL_SCANCODE_MINUS: return {0x0C, false};
    case SDL_SCANCODE_EQUALS: return {0x0D, false};
    case SDL_SCANCODE_BACKSPACE: return {0x0E, false};
    case SDL_SCANCODE_TAB: return {0x0F, false};
    case SDL_SCANCODE_Q: return {0x10, false};
    case SDL_SCANCODE_W: return {0x11, false};
    case SDL_SCANCODE_E: return {0x12, false};
    case SDL_SCANCODE_R: return {0x13, false};
    case SDL_SCANCODE_T: return {0x14, false};
    case SDL_SCANCODE_Y: return {0x15, false};
    case SDL_SCANCODE_U: return {0x16, false};
    case SDL_SCANCODE_I: return {0x17, false};
    case SDL_SCANCODE_O: return {0x18, false};
    case SDL_SCANCODE_P: return {0x19, false};
    case SDL_SCANCODE_LEFTBRACKET: return {0x1A, false};
    case SDL_SCANCODE_RIGHTBRACKET: return {0x1B, false};
    case SDL_SCANCODE_RETURN: return {0x1C, false};
    case SDL_SCANCODE_LCTRL: return {0x1D, false};
    case SDL_SCANCODE_A: return {0x1E, false};
    case SDL_SCANCODE_S: return {0x1F, false};
    case SDL_SCANCODE_D: return {0x20, false};
    case SDL_SCANCODE_F: return {0x21, false};
    case SDL_SCANCODE_G: return {0x22, false};
    case SDL_SCANCODE_H: return {0x23, false};
    case SDL_SCANCODE_J: return {0x24, false};
    case SDL_SCANCODE_K: return {0x25, false};
    case SDL_SCANCODE_L: return {0x26, false};
    case SDL_SCANCODE_SEMICOLON: return {0x27, false};
    case SDL_SCANCODE_APOSTROPHE: return {0x28, false};
    case SDL_SCANCODE_GRAVE: return {0x29, false};
    case SDL_SCANCODE_LSHIFT: return {0x2A, false};
    case SDL_SCANCODE_BACKSLASH: return {0x2B, false};
    case SDL_SCANCODE_Z: return {0x2C, false};
    case SDL_SCANCODE_X: return {0x2D, false};
    case SDL_SCANCODE_C: return {0x2E, false};
    case SDL_SCANCODE_V: return {0x2F, false};
    case SDL_SCANCODE_B: return {0x30, false};
    case SDL_SCANCODE_N: return {0x31, false};
    case SDL_SCANCODE_M: return {0x32, false};
    case SDL_SCANCODE_COMMA: return {0x33, false};
    case SDL_SCANCODE_PERIOD: return {0x34, false};
    case SDL_SCANCODE_SLASH: return {0x35, false};
    case SDL_SCANCODE_RSHIFT: return {0x36, false};
    case SDL_SCANCODE_KP_MULTIPLY: return {0x37, false};
    case SDL_SCANCODE_LALT: return {0x38, false};
    case SDL_SCANCODE_SPACE: return {0x39, false};
    case SDL_SCANCODE_CAPSLOCK: return {0x3A, false};
    case SDL_SCANCODE_F1: return {0x3B, false};
    case SDL_SCANCODE_F2: return {0x3C, false};
    case SDL_SCANCODE_F3: return {0x3D, false};
    case SDL_SCANCODE_F4: return {0x3E, false};
    case SDL_SCANCODE_F5: return {0x3F, false};
    case SDL_SCANCODE_F6: return {0x40, false};
    case SDL_SCANCODE_F7: return {0x41, false};
    case SDL_SCANCODE_F8: return {0x42, false};
    case SDL_SCANCODE_F9: return {0x43, false};
    case SDL_SCANCODE_F10: return {0x44, false};
    case SDL_SCANCODE_NUMLOCKCLEAR: return {0x45, false};
    case SDL_SCANCODE_SCROLLLOCK: return {0x46, false};
    case SDL_SCANCODE_KP_7: return {0x47, false};
    case SDL_SCANCODE_KP_8: return {0x48, false};
    case SDL_SCANCODE_KP_9: return {0x49, false};
    case SDL_SCANCODE_KP_MINUS: return {0x4A, false};
    case SDL_SCANCODE_KP_4: return {0x4B, false};
    case SDL_SCANCODE_KP_5: return {0x4C, false};
    case SDL_SCANCODE_KP_6: return {0x4D, false};
    case SDL_SCANCODE_KP_PLUS: return {0x4E, false};
    case SDL_SCANCODE_KP_1: return {0x4F, false};
    case SDL_SCANCODE_KP_2: return {0x50, false};
    case SDL_SCANCODE_KP_3: return {0x51, false};
    case SDL_SCANCODE_KP_0: return {0x52, false};
    case SDL_SCANCODE_KP_PERIOD: return {0x53, false};
    case SDL_SCANCODE_F11: return {0x57, false};
    case SDL_SCANCODE_F12: return {0x58, false};
    case SDL_SCANCODE_KP_ENTER: return {0x1C, true};
    case SDL_SCANCODE_RCTRL: return {0x1D, true};
    case SDL_SCANCODE_KP_DIVIDE: return {0x35, true};
    case SDL_SCANCODE_RALT: return {0x38, true};
    case SDL_SCANCODE_HOME: return {0x47, true};
    case SDL_SCANCODE_UP: return {0x48, true};
    case SDL_SCANCODE_PAGEUP: return {0x49, true};
    case SDL_SCANCODE_LEFT: return {0x4B, true};
    case SDL_SCANCODE_RIGHT: return {0x4D, true};
    case SDL_SCANCODE_END: return {0x4F, true};
    case SDL_SCANCODE_DOWN: return {0x50, true};
    case SDL_SCANCODE_PAGEDOWN: return {0x51, true};
    case SDL_SCANCODE_INSERT: return {0x52, true};
    case SDL_SCANCODE_DELETE: return {0x53, true};
    case SDL_SCANCODE_PAUSE: return {0xE1, false};
    default: return {0, false};
  }
}

class SdlPlatform final : public IPlatform {
 public:
  ~SdlPlatform() override {
    remaster_renderer_.reset();
    if (stream_) SDL_DestroyAudioStream(stream_);
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
  }

  bool Init(const SdlOptions& options) {
    audio_enabled_ = options.audio;
    if (!SDL_Init(SDL_INIT_VIDEO | (options.audio ? SDL_INIT_AUDIO : 0))) {
      std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return false;
    }
    int scale = options.scale > 0 ? options.scale : 3;
    remaster_ = options.remaster;
    // the remaster view is 16:9, the classic one 4:3
    int winw = remaster_ ? 427 * scale : 320 * scale;
    window_ = SDL_CreateWindow("Commander Keen 4", winw, 240 * scale, SDL_WINDOW_RESIZABLE);
    if (!window_) {
      std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
      return false;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
      std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
      return false;
    }
    vsync_ = options.vsync;
    remaster_vsync_ = options.remaster_vsync;
    art_.SetDirectories(options.art_dirs);
    art_.SetFallback(options.nearest ? render::Fallback::Nearest : render::Fallback::Scale4x);
    ReloadArt();
    remaster_renderer_ = std::make_unique<RemasterRenderer>(renderer_, &art_);
    remaster_renderer_->interpolate = options.interpolate;
    remaster_renderer_->show_hitboxes = options.hitboxes;
    SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    const SDL_DisplayMode* mode = display ? SDL_GetCurrentDisplayMode(display) : nullptr;
    float hz = mode && mode->refresh_rate > 1 ? mode->refresh_rate : 60.0f;
    refresh_ns_ = static_cast<Uint64>(1e9 / hz);
    ApplyMode();
    if (options.fullscreen) SDL_SetWindowFullscreen(window_, true);
    start_ns_ = SDL_GetTicksNS();
    return true;
  }

  std::uint64_t Ticks70() override { return (SDL_GetTicksNS() - start_ns_) * 70 / 1000000000ull; }
  std::uint64_t Millis() override { return (SDL_GetTicksNS() - start_ns_) / 1000000ull; }
  void Delay(int ms) override {
    // While the game waits for its next frame, keep drawing in-between frames
    // so motion stays smooth at the display's refresh rate.
    if (remaster_ && have_frame_ && remaster_renderer_->interpolate) {
      Uint64 now = SDL_GetTicksNS();
      if (now - last_draw_ns_ >= refresh_ns_) {
        DrawRemaster(now);
        return;
      }
    }
    SDL_Delay(static_cast<Uint32>(ms > 0 ? ms : 1));
  }

  void Present(const std::uint8_t* pixels, int width, int height,
               const std::uint32_t* palette) override {
    if (remaster_ && width == render::kClassicWidth && height == render::kClassicHeight) {
      frame_.assign(pixels, pixels + width * height);
      std::memcpy(palette_, palette, sizeof palette_);
      have_frame_ = true;
      Uint64 now = SDL_GetTicksNS();
      remaster_renderer_->Update(render::LatestScene(), now);
      DrawRemaster(now);
      Pump();
      return;
    }
    if (!texture_ || tex_w_ != width || tex_h_ != height) {
      if (texture_) SDL_DestroyTexture(texture_);
      texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                   width, height);
      SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
      tex_w_ = width;
      tex_h_ = height;
      rgb_.resize(static_cast<std::size_t>(width) * height);
    }
    if (!vsync_) {
      // Menus redraw in tight loops; keep them to the original 70 Hz.
      const Uint64 period = 1000000000ull / 70;
      Uint64 now = SDL_GetTicksNS();
      if (now - last_present_ns_ < period) SDL_DelayNS(period - (now - last_present_ns_));
      last_present_ns_ = SDL_GetTicksNS();
    }
    for (int i = 0; i < width * height; i++) rgb_[i] = palette[pixels[i] & 15];
    SDL_UpdateTexture(texture_, nullptr, rgb_.data(), width * 4);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    Pump();
  }

  bool PollKey(KeyEvent* event) override {
    Pump();
    if (keys_.empty()) return false;
    *event = keys_.front();
    keys_.pop_front();
    return true;
  }

  bool QuitRequested() override { return quit_; }

  int StartAudio(AudioCallback callback, void* user) override {
    if (!audio_enabled_) return 0;
    callback_ = callback;
    callback_user_ = user;
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = 48000;
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &SdlPlatform::FeedAudio, this);
    if (!stream_) {
      std::fprintf(stderr, "No audio: %s\n", SDL_GetError());
      return 0;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return spec.freq;
  }
  void LockAudio() override { audio_mutex_.lock(); }
  void UnlockAudio() override { audio_mutex_.unlock(); }

 private:
  static void SDLCALL FeedAudio(void* userdata, SDL_AudioStream* stream, int additional, int) {
    auto* self = static_cast<SdlPlatform*>(userdata);
    int frames = additional / static_cast<int>(2 * sizeof(float));
    if (frames <= 0) return;
    self->audio_buf_.resize(static_cast<std::size_t>(frames) * 2);
    {
      std::lock_guard<std::recursive_mutex> lock(self->audio_mutex_);
      self->callback_(self->audio_buf_.data(), frames, self->callback_user_);
    }
    SDL_PutAudioStreamData(stream, self->audio_buf_.data(), frames * static_cast<int>(2 * sizeof(float)));
  }

  void ApplyMode() {
    if (remaster_) {
      SDL_SetRenderVSync(renderer_, remaster_vsync_ ? 1 : 0);
      SDL_SetRenderLogicalPresentation(renderer_, render::kWideWidth, render::kClassicHeight,
                                       SDL_LOGICAL_PRESENTATION_LETTERBOX);
    } else {
      SDL_SetRenderVSync(renderer_, vsync_ ? 1 : 0);
      // 320x200 shown with the 4:3 aspect of a CRT, as 320x240.
      SDL_SetRenderLogicalPresentation(renderer_, 320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }
  }

  void ReloadArt() {
    art_.Reload();
    for (int i = 0; i < art_.layers(); i++)
      std::fprintf(stderr, "art: %d manifest entries from %s\n", art_.layerEntries(i),
                   art_.directories()[i].c_str());
  }

  void DrawRemaster(Uint64 now) {
    if (!remaster_vsync_) {
      // without vsync, stay at the display's rate
      if (now - last_draw_ns_ < refresh_ns_) SDL_DelayNS(refresh_ns_ - (now - last_draw_ns_));
      now = SDL_GetTicksNS();
    }
    remaster_renderer_->Draw(frame_.data(), palette_, now);
    SDL_RenderPresent(renderer_);
    last_draw_ns_ = now;
  }

  void Pump() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_EVENT_QUIT:
          quit_ = true;
          break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
          bool down = ev.type == SDL_EVENT_KEY_DOWN;
          if (down && ev.key.scancode == SDL_SCANCODE_F11 && !ev.key.repeat) {
            if (ev.key.mod & SDL_KMOD_SHIFT) {
              // Shift+F11: switch between remastered and original sound
              audio::RemasterAudio& sound = audio::Remaster();
              sound.SetEnabled(!sound.enabled());
              std::fprintf(stderr, "audio: %s sound\n", sound.enabled() ? "remastered" : "original");
              break;
            }
            // F11: switch between the remaster and the classic renderer
            remaster_ = !remaster_;
            ApplyMode();
            break;
          }
          if (down && ev.key.scancode == SDL_SCANCODE_F12 && !ev.key.repeat) {
            if (ev.key.mod & SDL_KMOD_SHIFT) {
              // Shift+F12: show the original hitboxes over the new art
              remaster_renderer_->show_hitboxes = !remaster_renderer_->show_hitboxes;
            } else {
              // F12: reload the art manifests while the game runs
              ReloadArt();
            }
            break;
          }
          if (ev.key.scancode == SDL_SCANCODE_F11 || ev.key.scancode == SDL_SCANCODE_F12) break;
          if (down && ev.key.scancode == SDL_SCANCODE_RETURN && (ev.key.mod & SDL_KMOD_ALT)) {
            fullscreen_ = !fullscreen_;
            SDL_SetWindowFullscreen(window_, fullscreen_);
            break;
          }
          XtCode xt = ToXt(ev.key.scancode);
          if (!xt.code) break;
          if (xt.code == 0xE1) {
            if (down && !ev.key.repeat) keys_.push_back({0xE1, true, false});
            break;
          }
          keys_.push_back({xt.code, down, xt.extended});
          break;
        }
        default:
          break;
      }
    }
  }

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  int tex_w_ = 0, tex_h_ = 0;
  std::vector<std::uint32_t> rgb_;
  std::deque<KeyEvent> keys_;
  bool quit_ = false;
  bool fullscreen_ = false;
  Uint64 start_ns_ = 0;
  Uint64 last_present_ns_ = 0;
  bool vsync_ = false;
  bool remaster_ = false;
  bool remaster_vsync_ = true;
  render::ArtLibrary art_;
  std::unique_ptr<RemasterRenderer> remaster_renderer_;
  std::vector<std::uint8_t> frame_;
  std::uint32_t palette_[16] = {};
  bool have_frame_ = false;
  Uint64 last_draw_ns_ = 0;
  Uint64 refresh_ns_ = 16666667;

  bool audio_enabled_ = true;
  SDL_AudioStream* stream_ = nullptr;
  AudioCallback callback_ = nullptr;
  void* callback_user_ = nullptr;
  std::vector<float> audio_buf_;
  std::recursive_mutex audio_mutex_;
};

}  // namespace

std::unique_ptr<IPlatform> CreateSdlPlatform(const SdlOptions& options) {
  auto platform = std::make_unique<SdlPlatform>();
  if (!platform->Init(options)) return nullptr;
  return platform;
}

}  // namespace keen4::platform
