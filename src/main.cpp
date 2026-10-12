// Entry point: finds the game data, loads KEEN4E.EXE, sets up the platform
// layer and runs the ported CK_MAIN.
//
//   keen4 [--data DIR] [--save DIR] [--fullscreen] [--scale N] [--nosound]
//         [original parameters such as /NOWAIT, /TEDLEVEL n, EASY]
//
// Headless mode renders without a window, for tests and CI:
//   keen4 --headless --frames N [--shot-every K] [--out DIR]
//         [--tics T] [--keys "tic:scancode:down,..."]
// writes frame_NNNNN.png every K presented frames and quits after N frames
// (or T tics of virtual 70 Hz time). Keys are pressed at virtual times, so
// they also reach loops that wait for input without drawing.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "engine/datafiles.h"
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

#include "platform/sdl3/sdl_platform.h"
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

  bool PollKey(keen4::platform::KeyEvent* event) override {
    while (next_ < script.size() && static_cast<std::uint64_t>(script[next_].tic) <= Ticks70()) {
      PushKey(script[next_].scancode, script[next_].down);
      next_++;
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
  std::size_t next_ = 0;

  void Save(int f) {
    char name[32];
    std::snprintf(name, sizeof name, "frame_%05d.png", f);
    keen4::WritePng((std::filesystem::path(out_dir) / name).string(), last_frame().data(), frame_width(),
                    frame_height(), last_palette());
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
    else if (a == "--tics")
      scripted->max_tics = std::strtoull(next().c_str(), nullptr, 10);
    else if (a == "--fullscreen")
      fullscreen = true;
    else if (a == "--scale")
      scale = std::atoi(next().c_str());
    else if (a == "--nosound")
      sound = false;
    else if (a == "--sim-vsync")
      scripted->vsync_hz = std::atoi(next().c_str());
    else if (a == "--vsync")
      vsync = true;
    else if (a == "--version") {
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

  std::unique_ptr<keen4::platform::IPlatform> platform;
#ifdef KEEN4_WITH_SDL3
  if (!headless) {
    keen4::platform::SdlOptions options;
    options.scale = scale;
    options.fullscreen = fullscreen;
    options.audio = sound;
    options.vsync = vsync;
    platform = keen4::platform::CreateSdlPlatform(options);
    if (!platform) return 1;
  }
#else
  (void)fullscreen;
  (void)sound;
  (void)scale;
  headless = true;
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
