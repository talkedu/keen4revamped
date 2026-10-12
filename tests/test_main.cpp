#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "engine/lang.h"
#include "engine/version.h"
#include "id/id_ca.h"
#include "id/id_in.h"
#include "id/id_vw.h"
#include "platform/headless/headless_platform.h"
#include "render/art.h"
#include "render/motion.h"

// The engine reports fatal errors through Quit, normally from CK_MAIN.
[[noreturn]] void Quit(const char* error) {
  std::printf("Quit: %s\n", error ? error : "");
  std::exit(1);
}

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

int main() {
  CHECK(std::strlen(keen4::VersionString()) > 0);

  // The headless clock only moves when the program waits.
  keen4::platform::HeadlessPlatform platform;
  CHECK(platform.Ticks70() == 0);
  platform.Delay(1000);
  CHECK(platform.Ticks70() == 70);

  keen4::platform::KeyEvent event;
  platform.PushKey(0x39, true);
  CHECK(platform.PollKey(&event) && event.scancode == 0x39 && event.down);
  CHECK(!platform.PollKey(&event));

  // Gamepad: plugging one in switches control to it, its buttons type menu
  // keys outside the play loop, a key press or unplugging goes back to the
  // keyboard.
  {
    using namespace keen4::platform;
    Set(&platform);
    Controls[0] = ctrl_Keyboard1;
    GamepadState pad;
    pad.connected = true;
    pad.name = "Test Pad";
    platform.SetGamepad(pad);
    IN_PumpEvents();
    CHECK(JoysPresent[0] && Controls[0] == ctrl_Joystick1 && IN_GamepadActive());

    IN_ClearKeysDown();
    pad.buttons = kPadA;
    platform.SetGamepad(pad);
    IN_PumpEvents();
    CHECK(LastScan == sc_Return && Keyboard[sc_Return] && IN_JoyButtons() == 1);
    pad.buttons = 0;
    pad.y = 0.9f;  // stick down
    platform.SetGamepad(pad);
    IN_PumpEvents();
    CHECK(!Keyboard[sc_Return] && Keyboard[sc_DownArrow] && LastScan == sc_DownArrow);

    // held through a clear, a button does nothing until pressed again
    pad.y = 0;
    pad.buttons = kPadB;
    platform.SetGamepad(pad);
    IN_PumpEvents();
    CHECK(Keyboard[sc_Escape]);
    IN_ClearKeysDown();
    IN_PumpEvents();
    CHECK(!Keyboard[sc_Escape] && IN_JoyButtons() == 0);

    platform.PushKey(sc_Space, true);
    IN_PumpEvents();
    CHECK(Controls[0] == ctrl_Keyboard1 && !IN_GamepadActive());
    platform.PushKey(sc_Space, false);
    pad.buttons = kPadX;
    platform.SetGamepad(pad);
    IN_PumpEvents();
    CHECK(Controls[0] == ctrl_Joystick1);

    platform.SetGamepad(GamepadState{});
    IN_PumpEvents();
    CHECK(!JoysPresent[0] && Controls[0] == ctrl_Keyboard1 && IN_JoyButtons() == 0);
    IN_ClearKeysDown();
    Set(nullptr);
  }

  // Scale4x: 4x the size, no new colours, and a flat image stays flat.
  {
    std::vector<std::uint8_t> src = {1, 2, 3, 4};
    std::vector<std::uint8_t> big = keen4::render::Scale4x(src, 2, 2);
    CHECK(big.size() == 64);
    for (std::uint8_t v : big) CHECK(v >= 1 && v <= 4);
    std::vector<std::uint8_t> flat(9, 7);
    for (std::uint8_t v : keen4::render::Scale4x(flat, 3, 3)) CHECK(v == 7);
    // a diagonal edge gets smoothed: the corner of the upscaled pixel takes the neighbour
    std::vector<std::uint8_t> diag = {0, 1, 1, 1};
    std::vector<std::uint8_t> d = keen4::render::Scale4x(diag, 2, 2);
    CHECK(d[0] == 0 && d[7 * 8 + 7] == 1);
  }

  // Dithers: a grey/white checkerboard becomes one light grey tone, edge to
  // edge; a lone dot and a diagonal line stay as they are.
  {
    using keen4::render::DitherTones;
    std::vector<std::uint8_t> checker(36);
    for (int i = 0; i < 36; i++) checker[i] = ((i % 6) + (i / 6)) % 2 ? 7 : 15;
    std::vector<std::uint32_t> tones = DitherTones(checker, 6, 6);
    const std::uint32_t tone = tones[0];
    // gamma-correct mean of AAAAAA and FFFFFF, lighter than the plain average
    CHECK(tone == 0xFFDADADAu);
    for (std::uint32_t t : tones) CHECK(t == tone);
    std::vector<std::uint8_t> dot(25, 0);
    dot[12] = 15;
    for (std::uint32_t t : DitherTones(dot, 5, 5)) CHECK(t == 0);
    std::vector<std::uint8_t> line(25, 0);
    for (int i = 0; i < 5; i++) line[i * 6] = 15;
    for (std::uint32_t t : DitherTones(line, 5, 5)) CHECK(t == 0);
  }

  // Smooth motion follows the object, not the image corner: an inchworm
  // stepping 8 pixels right swaps its flat frame (art at +0) for its hump
  // (art at +8), and the hump must not start 8 pixels behind and slide.
  // Scenes arrive right on the game clock here, so the view shows game time
  // `t` at wall time t + kLag.
  {
    using keen4::render::Scene;
    using keen4::render::SceneSprite;
    using keen4::render::SpriteMotion;
    auto at = [](double t) { return t + SpriteMotion::kLag; };
    auto near = [](float v, float want) { return std::abs(v - want) < 0.001f; };
    int worm = 0, other = 0;
    Scene a, b;
    a.map = b.map = 1;
    a.sprites.push_back(SceneSprite{&worm, 333, 100, 50, 0, 8, 2, false});
    b.sprites.push_back(SceneSprite{&worm, 334, 116, 46, 8, 4, 2, false});
    auto update = [](SpriteMotion& m, Scene& s, double t) {
      s.time = t;
      m.Update(s, t);
    };
    SpriteMotion motion;
    float x = -1, y = -1;
    update(motion, a, 0.0);
    CHECK(motion.Position(a.sprites[0], at(0.0), &x, &y) && near(x, 100) && near(y, 50));
    // a step half a second after the last one glides over half a second from
    // the frame before it, and the hump's head (right edge) starts where the
    // flat frame's was
    update(motion, a, 0.125);
    update(motion, b, 0.5);
    CHECK(motion.Position(b.sprites[0], at(0.125), &x, &y) && near(x, 108) && near(y, 46));
    CHECK(motion.Position(b.sprites[0], at(0.375), &x, &y) && near(x, 112));
    CHECK(motion.Position(b.sprites[0], at(0.625), &x, &y) && near(x, 116));
    CHECK(motion.Position(b.sprites[0], at(2.0), &x, &y) && near(x, 116));
    // after a pause the next step glides at the usual pace
    update(motion, b, 2.0);
    update(motion, a, 2.125);
    CHECK(motion.Position(a.sprites[0], at(2.25), &x, &y) && near(x, 104));
    CHECK(motion.Position(a.sprites[0], at(2.5), &x, &y) && near(x, 100));
    // an actor that speeds up is followed at once
    SpriteMotion quick;
    update(quick, a, 0.0);
    update(quick, b, 0.5);
    update(quick, a, 0.5625);
    CHECK(quick.Position(a.sprites[0], at(0.5625), &x, &y) && near(x, 100));
    // a jump bigger than kMaxStep is not smoothed
    Scene far = b;
    far.sprites[0].x = 400;
    update(motion, far, 3.0);
    CHECK(motion.Position(far.sprites[0], at(3.0), &x, &y) && near(x, 400));
    x = -1;
    SceneSprite stranger{&other, 334, 116, 46, 8, 4, 2, false};
    CHECK(!motion.Position(stranger, at(3.0), &x, &y) && x == -1);
  }

  // Keen runs at 3 pixels a tic with the camera locked on him. The game draws
  // 2 or 3 tics apart and each frame reaches the screen a little late, by a
  // different amount each time. Drawn on the game's clock, the camera still
  // moves at one steady speed and Keen stays put on screen.
  {
    using keen4::render::Scene;
    using keen4::render::SceneSprite;
    using keen4::render::SpriteMotion;
    int keen = 0;
    SpriteMotion motion;
    Scene s;
    s.map = 1;
    s.sprites.push_back(SceneSprite{&keen, 100, 0, 80, 0, 0, 1, false});
    const int gaps[] = {2, 3, 2, 2, 3, 3, 2};                   // tics between frames
    const double late[] = {0, 0.012, 0.004, 0.009, 0, 0.013, 0.002};  // arrival after the game clock
    int tic = 0;
    double worst_speed = 0, worst_lock = 0;
    for (int i = 0; i < 40; i++) {
      s.time = tic / 70.0;
      s.viewx = tic * 3;
      s.sprites[0].x = tic * 3 + 150;
      const double arrival = s.time + late[i % 7];
      motion.Update(s, arrival);
      tic += gaps[i % 7];
      const double next = tic / 70.0 + late[(i + 1) % 7];
      for (int k = 0; k < 8; k++) {
        const double now = arrival + (next - arrival) * k / 8;
        float cx = 0, cy = 0, kx = 0, ky = 0;
        CHECK(motion.Camera(now, &cx, &cy) && motion.Position(s.sprites[0], now, &kx, &ky));
        worst_lock = std::max(worst_lock, static_cast<double>(std::abs(kx - cx - 150)));
        if (i >= 3) worst_speed = std::max(worst_speed, std::abs(cx - (now - SpriteMotion::kLag) * 210));
      }
    }
    CHECK(worst_lock < 0.01);
    CHECK(worst_speed < 0.01);
  }

  // Art layers: curated art wins over the AI set, `bad` falls through to the
  // next folder, and assets no folder lists use the built-in upscale.
  {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "keen4-art-layers";
    fs::remove_all(root);
    fs::create_directories(root / "curated");
    fs::create_directories(root / "ai");
    std::ofstream(root / "curated" / "manifest.txt") << "# curated\n"
                                                        "sprite 124 keen.png anchor 8 4\n"
                                                        "tile16 7 seven.png bad\n";
    std::ofstream(root / "ai" / "manifest.txt") << "sprite 124 ai124.png\n"
                                                   "sprite 125 ai125.png\n"
                                                   "tile16 7 ai7.png\r\n";
    keen4::render::ArtLibrary art;
    art.SetDirectories({(root / "curated").string(), (root / "ai").string()});
    art.Reload();
    CHECK(art.layers() == 2 && art.layerEntries(0) == 2 && art.layerEntries(1) == 3);
    keen4::render::ArtLibrary::Source src;
    using keen4::render::AssetKind;
    CHECK(art.Find(AssetKind::Sprite, 124, &src) && src.layer == 0 && src.ax == 8 && src.ay == 4 &&
          fs::path(src.path).filename() == "keen.png");
    CHECK(art.Find(AssetKind::Sprite, 124, &src, 1) && src.layer == 1);
    CHECK(art.Find(AssetKind::Sprite, 125, &src) && src.layer == 1);
    CHECK(art.Find(AssetKind::Tile16, 7, &src) && src.layer == 1 && fs::path(src.path).filename() == "ai7.png");
    CHECK(!art.Find(AssetKind::Tile16M, 7, &src));
    CHECK(!art.Find(AssetKind::Sprite, 126, &src));
    fs::remove_all(root);
  }

  // Translations: UTF-8 becomes ISO-8859-1, unknown text passes through, and
  // --lang wins over the config.
  {
    using namespace keen4::lang;
    CHECK(ToLatin1("ação É") == "a\xE7\xE3o \xC9");
    CHECK(ToLatin1("€") == "?");
    Language l;
    CHECK(Parse("PT-br", &l) && l == Language::Portuguese);
    CHECK(Parse("en", &l) && l == Language::English);
    CHECK(!Parse("xx", &l));

    const char* paused = "PAUSED";
    CHECK(Tr(paused) == paused);
    CHECK(Article(4739) == nullptr);
    Set(Language::Portuguese);
    CHECK(std::strcmp(Tr(paused), "PAUSADO") == 0);
    CHECK(std::strcmp(Tr("LEVEL"), "N\xCDVEL") == 0);
    const char* unknown = "no such text";
    CHECK(Tr(unknown) == unknown);
    CHECK(Article(4739) && std::strncmp(Article(4739), "^P", 2) == 0);
    CHECK(std::strcmp(MenuLabel(), "IDIOMA: PORTUGU\xCAS") == 0);
    Next();
    CHECK(Current() == Language::English);

    SetFromCommandLine(Language::English);
    SetFromConfig(Language::Portuguese);
    CHECK(Current() == Language::English);
  }

  // Accented letters are drawn from the plain letter of the font plus a mark,
  // which may rise above the font's first row.
  {
    static byte font[0x302 + 6] = {};
    font[0] = 6;  // height
    font[2 + 'A' * 2] = 0x02;
    font[3 + 'A' * 2] = 0x03;  // glyph at 0x302
    font[0x202 + 'A'] = 5;     // width
    const byte rows[6] = {0x70, 0x88, 0xF8, 0x88, 0x88, 0x00};
    std::memcpy(font + 0x302, rows, sizeof rows);
    byte* saved = grsegs[STARTFONT];
    grsegs[STARTFONT] = font;
    fontnumber = 0;

    word w, h;
    VW_MeasurePropString("\xC1", &w, &h);  // Á
    CHECK(w == 5 && h == 6);
    VW_MeasurePropString("\xC6", &w, &h);  // Æ has no plain letter
    CHECK(w == 0);

    auto ink = [](int y) {
      int n = 0;
      for (int x = 16; x < 32; x++) n += vw_screen[y * SCREENWIDTH + x] != 0;
      return n;
    };
    VW_ClearVideo(0);
    px = 16;
    py = 20;
    VW_DrawPropString("A");
    CHECK(ink(18) == 0 && ink(19) == 0 && ink(20) == 3);
    VW_ClearVideo(0);
    px = 16;
    py = 20;
    VW_DrawPropString("\xC1");
    CHECK(ink(17) == 0 && ink(18) == 2 && ink(19) == 2 && ink(20) == 3 && ink(22) == 5);
    grsegs[STARTFONT] = saved;
  }

  if (failures == 0) std::printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
