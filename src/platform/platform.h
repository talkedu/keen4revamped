#pragma once

#include <cstdint>
#include <vector>

// Platform interface. Engine and game code reach the operating system only
// through this; the SDL3 and headless backends implement it.
namespace keen4::platform {

// One key transition, in original XT (set 1) scancodes. Extended keys such as
// the arrow keys arrive as the scancode the DOS BIOS produced for them.
struct KeyEvent {
  int scancode;
  bool down;
  bool extended;  // preceded by the 0xE0 prefix on a real keyboard
};

// Game controller buttons in SDL's Xbox-style layout. Other pads (PlayStation,
// Switch and so on) report the buttons in the same positions.
enum GamepadButton : std::uint32_t {
  kPadA = 1u << 0,  // bottom face button
  kPadB = 1u << 1,  // right
  kPadX = 1u << 2,  // left
  kPadY = 1u << 3,  // top
  kPadBack = 1u << 4,
  kPadStart = 1u << 5,
  kPadUp = 1u << 6,
  kPadDown = 1u << 7,
  kPadLeft = 1u << 8,
  kPadRight = 1u << 9,
};

struct GamepadState {
  bool connected = false;
  std::uint32_t buttons = 0;  // GamepadButton bits
  float x = 0, y = 0;         // left stick, -1..1, y pointing down
  const char* name = "";
  std::uint32_t id = 0;       // stays the same while the pad is plugged in; never 0
};

// Fills `count` stereo frames (2 floats each, -1..1) at the rate StartAudio returned.
using AudioCallback = void (*)(float* frames, int count, void* user);

// The VIDEO card's settings, saved in the config file.
struct VideoSettings {
  bool fullscreen = false;
  bool vsync = true;
  bool upscaled = true;  // the remaster renderer; false shows the original 320x200 frame
  // The size the upscaled view is drawn at before it is scaled to the window;
  // 0x0 (AUTO) draws it at the window's own size.
  int width = 0, height = 0;
};

struct Resolution {
  int width, height;
};

// The 16:9 sizes the VIDEO card offers, smallest first.
std::vector<Resolution> StandardResolutions();

class IPlatform {
 public:
  virtual ~IPlatform() = default;

  // Monotonic time in 1/70 s units (the original timer rate) and milliseconds.
  virtual std::uint64_t Ticks70() = 0;
  virtual std::uint64_t Millis() = 0;
  virtual void Delay(int ms) = 0;

  // Shows an 8-bit indexed frame. `palette` has 16 0xRRGGBB entries.
  virtual void Present(const std::uint8_t* pixels, int width, int height,
                       const std::uint32_t* palette) = 0;

  // Next pending key transition; false when there are none.
  virtual bool PollKey(KeyEvent* event) = 0;
  // The most recently connected gamepad; `connected` is false when there is none.
  virtual void ReadGamepad(GamepadState* state) { *state = GamepadState{}; }
  // Every connected gamepad, oldest first (co-op gives each player its own).
  virtual int GamepadCount() {
    GamepadState state;
    ReadGamepad(&state);
    return state.connected ? 1 : 0;
  }
  virtual void ReadGamepadAt(int index, GamepadState* state) {
    if (index == 0)
      ReadGamepad(state);
    else
      *state = GamepadState{};
  }
  // True once the user closed the window.
  virtual bool QuitRequested() = 0;

  // Starts streaming audio from `callback`. Returns the sample rate, or 0 if
  // there is no audio device.
  virtual int StartAudio(AudioCallback callback, void* user) = 0;
  virtual void LockAudio() = 0;
  virtual void UnlockAudio() = 0;

  // Opens a web page in the player's browser (the updater's GitHub sign-in).
  virtual bool OpenUrl(const char*) { return false; }
  virtual void SetClipboardText(const char*) {}

  // Screen mode, vsync, renderer and resolution. Backends without a window
  // just keep the values, so the menu still cycles through them.
  virtual VideoSettings GetVideo() { return video_; }
  virtual void SetVideo(const VideoSettings& video) { video_ = video; }
  // The resolutions that fit the display.
  virtual std::vector<Resolution> Resolutions() { return StandardResolutions(); }

 protected:
  VideoSettings video_;
};

// The active backend.
IPlatform& Get();
void Set(IPlatform* platform);

}  // namespace keen4::platform
