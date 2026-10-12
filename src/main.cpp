// Entry point: finds the game data, loads KEEN4E.EXE, sets up the platform
// layer and runs the ported CK_MAIN.
//
//   keen4 [--data DIR] [--save DIR] [--fullscreen] [--scale N] [--nosound] [--lang en|pt-BR]
//         [original parameters such as /NOWAIT, /TEDLEVEL n, EASY]
//
// Headless mode renders without a window, for tests and CI:
//   keen4 --headless --frames N [--shot-every K] [--out DIR]
//         [--tics T] [--keys "tic:scancode:down,..."] [--gamepad "tic:buttons,..."]
//         [--gamepad2 "tic:buttons,..."]
// writes frame_NNNNN.png every K presented frames and quits after N frames
// (or T tics of virtual 70 Hz time). Keys are pressed at virtual times, so
// they also reach loops that wait for input without drawing. --gamepad connects a
// gamepad at the first tic and holds the given GamepadButton bits from each tic on;
// --gamepad2 does the same for a second pad, and buttons -1 unplug a pad.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "audio/remaster_audio.h"
#include "engine/datafiles.h"
#include "engine/lang.h"
#include "engine/png_writer.h"
#include "engine/version.h"
#include "exe/keen4exe.h"
#include "id/id_sys.h"
#include "id/id_us.h"
#include "platform/headless/headless_platform.h"
#include "platform/platform.h"
#ifdef KEEN4_WITH_SDL3
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // provides WinMain on Windows

#include "platform/sdl3/remaster_renderer.h"
#include "platform/sdl3/sdl_platform.h"
#include "render/art.h"
#include "render/scene.h"
#endif

void CK_InitStates();
void CK_Main();

namespace {

struct ScriptedKey {
  int tic, scancode;
  bool down;
};

// Headless backend that presses keys at given times and saves frames as PNG.
class ScriptedHeadless : public keen4::platform::HeadlessPlatform {
 public:
  int max_frames = 600;
  std::uint64_t max_tics = 0;
  int vsync_hz = 0;
  int shot_every = 0;
  std::string out_dir = ".";
  std::vector<ScriptedKey> script;
  // Gamepad button states (GamepadButton bits in `scancode`, -1 unplugged)
  // from their tic on, for the first and second pad.
  std::vector<ScriptedKey> pad_script[2];
  // Also saves the remaster renderer's view of the frame (SDL builds).
  std::function<void(const std::string& path, const std::uint8_t* pixels, const std::uint32_t* palette,
                     std::uint64_t now_ns)>
      remaster_shot;

  bool PollKey(keen4::platform::KeyEvent* event) override {
    while (next_ < script.size() && static_cast<std::uint64_t>(script[next_].tic) <= Ticks70()) {
      PushKey(script[next_].scancode, script[next_].down);
      next_++;
    }
    for (int slot = 0; slot < 2; slot++) {
      const std::vector<ScriptedKey>& script = pad_script[slot];
      std::size_t& next = next_pad_[slot];
      while (next < script.size() && static_cast<std::uint64_t>(script[next].tic) <= Ticks70()) {
        keen4::platform::GamepadState pad;
        pad.connected = script[next].scancode >= 0;
        pad.name = slot ? "Headless Pad 2" : "Headless Pad";
        pad.buttons = pad.connected ? static_cast<std::uint32_t>(script[next].scancode) : 0;
        SetGamepadAt(slot, pad);
        next++;
      }
    }
    if (max_tics && Ticks70() >= max_tics) RequestQuit();
    return HeadlessPlatform::PollKey(event);
  }

  void Present(const std::uint8_t* pixels, int width, int height,
               const std::uint32_t* palette) override {
    HeadlessPlatform::Present(pixels, width, height, palette);
    if (vsync_hz > 0) {
      // wait for the next simulated vertical blank, like a vsynced window
      double period = 1000.0 / vsync_hz;
      auto next = static_cast<std::uint64_t>((static_cast<std::uint64_t>(Millis() / period) + 1) * period);
      Delay(static_cast<int>(next - Millis()));
    }
    int f = frames();
    if (shot_every > 0 && f % shot_every == 0) Save(f);
    if (f >= max_frames) {
      Save(f);
      RequestQuit();
    }
  }

 private:
  std::size_t next_ = 0, next_pad_[2] = {0, 0};

  void Save(int f) {
    char name[32];
    std::snprintf(name, sizeof name, "frame_%05d.png", f);
    keen4::WritePng((std::filesystem::path(out_dir) / name).string(), last_frame().data(), frame_width(),
                    frame_height(), last_palette());
    if (remaster_shot) {
      std::snprintf(name, sizeof name, "hd_%05d.png", f);
      remaster_shot((std::filesystem::path(out_dir) / name).string(), last_frame().data(), last_palette(),
                    Millis() * 1000000ull);
    }
  }
};

std::vector<ScriptedKey> ParseKeys(const std::string& text) {
  std::vector<ScriptedKey> keys;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) {
    ScriptedKey k{};
    int down = 1;
    if (std::sscanf(item.c_str(), "%d:%i:%d", &k.tic, &k.scancode, &down) >= 2) {
      k.down = down != 0;
      keys.push_back(k);
    }
  }
  std::stable_sort(keys.begin(), keys.end(), [](const ScriptedKey& a, const ScriptedKey& b) { return a.tic < b.tic; });
  return keys;
}

// Per-user folder for config, saves and scores: %APPDATA%/keen4revamped on
// Windows, $XDG_DATA_HOME/keen4revamped or ~/.local/share/keen4revamped elsewhere.
std::string DefaultSaveDir() {
  namespace fs = std::filesystem;
#ifdef _WIN32
  if (const char* appdata = std::getenv("APPDATA")) return (fs::path(appdata) / "keen4revamped").string();
#else
  if (const char* xdg = std::getenv("XDG_DATA_HOME")) return (fs::path(xdg) / "keen4revamped").string();
  if (const char* home = std::getenv("HOME")) return (fs::path(home) / ".local/share/keen4revamped").string();
#endif
  return "keen4-save";
}

bool HasExe(const std::string& dir) {
  keen4::files::SetDataDir(dir);
  return !keen4::files::Find("KEEN4E.EXE").empty();
}

std::string FindDataDir(const std::string& requested) {
  if (!requested.empty()) return requested;
  std::vector<std::string> candidates = {".", "gamedata", "base4"};
#ifdef _WIN32
  candidates.push_back("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Commander Keen\\base4");
#endif
  for (const std::string& dir : candidates) {
    if (HasExe(dir)) return dir;
  }
  return ".";
}

}  // namespace

int main(int argc, char** argv) {
  std::string datadir, savedir;
  bool headless = false, fullscreen = false, sound = true, vsync = false;
  bool classic = false, nearest = false, interpolate = true, hdshots = false;
  std::vector<std::string> artdirs;
  bool curated = false, remastered_audio = false;
  bool hitboxes = false;
  int scale = 3;
  auto scripted = std::make_unique<ScriptedHeadless>();
  std::vector<char*> gameargs = {argv[0]};

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--data")
      datadir = next();
    else if (a == "--save")
      savedir = next();
    else if (a == "--headless")
      headless = true;
    else if (a == "--frames")
      scripted->max_frames = std::atoi(next().c_str());
    else if (a == "--shot-every")
      scripted->shot_every = std::atoi(next().c_str());
    else if (a == "--out")
      scripted->out_dir = next();
    else if (a == "--keys")
      scripted->script = ParseKeys(next());
    else if (a == "--gamepad")
      scripted->pad_script[0] = ParseKeys(next());
    else if (a == "--gamepad2")
      scripted->pad_script[1] = ParseKeys(next());
    else if (a == "--tics")
      scripted->max_tics = std::strtoull(next().c_str(), nullptr, 10);
    else if (a == "--fullscreen")
      fullscreen = true;
    else if (a == "--scale")
      scale = std::atoi(next().c_str());
    else if (a == "--nosound")
      sound = false;
    else if (a == "--remastered-audio")
      remastered_audio = true;
    else if (a == "--audio") {
      keen4::audio::AudioOptions().dir = next();
      remastered_audio = true;
    } else if (a == "--curated")
      curated = true;
    else if (a == "--music-volume")
      keen4::audio::AudioOptions().music_volume = std::atoi(next().c_str());
    else if (a == "--sfx-volume")
      keen4::audio::AudioOptions().sfx_volume = std::atoi(next().c_str());
    else if (a == "--sim-vsync")
      scripted->vsync_hz = std::atoi(next().c_str());
    else if (a == "--vsync")
      vsync = true;
    else if (a == "--classic")
      classic = true;
    else if (a == "--art")
      artdirs.push_back(next());
    else if (a == "--nearest")
      nearest = true;
    else if (a == "--no-interpolation")
      interpolate = false;
    else if (a == "--hitboxes")
      hitboxes = true;
    else if (a == "--hd-shots")
      hdshots = true;
    else if (a == "--lang") {
      std::string code = next();
      keen4::lang::Language language;
      if (!keen4::lang::Parse(code, &language)) {
        std::fprintf(stderr, "Unknown language '%s' (use en or pt-BR)\n", code.c_str());
        return 2;
      }
      keen4::lang::SetFromCommandLine(language);
    } else if (a == "--version") {
      std::printf("%s\n", keen4::VersionString());
      return 0;
    } else
      gameargs.push_back(argv[i]);
  }

  datadir = FindDataDir(datadir);
  keen4::files::SetDataDir(datadir);
  // Never write into the game folder: config, saves and scores go elsewhere.
  if (savedir.empty() && std::getenv("KEEN4_SAVE_DIR")) savedir = std::getenv("KEEN4_SAVE_DIR");
  if (savedir.empty()) savedir = DefaultSaveDir();
  std::error_code ec;
  std::filesystem::create_directories(savedir, ec);
  keen4::files::SetWriteDir(savedir);

  std::string exe = keen4::files::Find("KEEN4E.EXE");
  std::string error;
  if (exe.empty() || !keen4::exe::g_exe.Load(exe, &error)) {
    std::fprintf(stderr,
                 "Could not load KEEN4E.EXE (v1.4 EGA) from '%s'%s%s\n"
                 "Copy your Keen 4 files (KEEN4E.EXE, *.CK4) there or pass --data DIR.\n",
                 datadir.c_str(), error.empty() ? "" : ": ", error.c_str());
#ifdef KEEN4_WITH_SDL3
    if (!headless) {
      std::string text = "Could not load KEEN4E.EXE (v1.4 EGA) from '" + datadir +
                         "'.\nCopy your Keen 4 files there, or pass --data DIR.";
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Commander Keen 4", text.c_str(), nullptr);
    }
#endif
    return 1;
  }

  // By default only the upscaled set in art/; --curated puts the hand-curated
  // art in art-curated/ in front of it (or of the --art folders).
  if (artdirs.empty() && std::filesystem::exists("art/manifest.txt")) artdirs.push_back("art");
  if (curated && std::find(artdirs.begin(), artdirs.end(), "art-curated") == artdirs.end())
    artdirs.insert(artdirs.begin(), "art-curated");

  // By default the original sound; --remastered-audio (or --audio DIR) plays the
  // recordings in audio/ instead, and the original for anything not listed.
  if (remastered_audio && keen4::audio::AudioOptions().dir.empty()) keen4::audio::AudioOptions().dir = "audio";

  std::unique_ptr<keen4::platform::IPlatform> platform;
#ifdef KEEN4_WITH_SDL3
  if (!headless) {
    keen4::platform::SdlOptions options;
    options.scale = scale;
    options.fullscreen = fullscreen;
    options.audio = sound;
    options.vsync = vsync;
    options.remaster = !classic;
    options.interpolate = interpolate;
    options.nearest = nearest;
    options.art_dirs = artdirs;
    options.hitboxes = hitboxes;
    platform = keen4::platform::CreateSdlPlatform(options);
    if (!platform) return 1;
  }
#else
  (void)fullscreen;
  (void)sound;
  (void)scale;
  headless = true;
#endif
#ifdef KEEN4_WITH_SDL3
  // Headless remaster frames: the same renderer on SDL's software renderer.
  static keen4::render::ArtLibrary hdart;
  static SDL_Surface* hdsurface = nullptr;
  static SDL_Renderer* hdrenderer = nullptr;
  static std::unique_ptr<keen4::platform::RemasterRenderer> hd;
  if (headless && hdshots) {
    hdart.SetDirectories(artdirs);
    hdart.SetFallback(nearest ? keen4::render::Fallback::Nearest : keen4::render::Fallback::Scale4x);
    hdart.Reload();
    hdsurface = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_ARGB8888);
    hdrenderer = SDL_CreateSoftwareRenderer(hdsurface);
    SDL_SetRenderLogicalPresentation(hdrenderer, keen4::render::kWideWidth, keen4::render::kClassicHeight,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    hd = std::make_unique<keen4::platform::RemasterRenderer>(hdrenderer, &hdart);
    hd->interpolate = interpolate;
    hd->show_hitboxes = hitboxes;
    scripted->remaster_shot = [](const std::string& path, const std::uint8_t* pixels,
                                 const std::uint32_t* palette, std::uint64_t now) {
      hd->Update(keen4::render::LatestScene(), now);
      hd->Draw(pixels, palette, now);
      SDL_RenderPresent(hdrenderer);
      std::vector<std::uint32_t> rgb(static_cast<std::size_t>(hdsurface->w) * hdsurface->h);
      for (int y = 0; y < hdsurface->h; y++)
        std::memcpy(&rgb[static_cast<std::size_t>(y) * hdsurface->w],
                    static_cast<const std::uint8_t*>(hdsurface->pixels) + y * hdsurface->pitch, hdsurface->w * 4);
      keen4::WritePngRgb(path, rgb.data(), hdsurface->w, hdsurface->h);
    };
  }
#else
  (void)hdshots;
  (void)hitboxes;
  (void)nearest;
  (void)interpolate;
  (void)classic;
#endif
  if (headless) platform = std::move(scripted);
  keen4::platform::Set(platform.get());

  gameargs.push_back(nullptr);
  US_SetArgs(static_cast<int>(gameargs.size()) - 1, gameargs.data());
  SYS_Startup();
  CK_InitStates();
  CK_Main();
  return 0;
}
