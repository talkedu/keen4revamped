#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "engine/datafiles.h"
#include "engine/gamedata_search.h"
#include "engine/lang.h"
#include "engine/version.h"
#include "game/coop_split.h"
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

  // Mouse: clicks type Return and Escape outside the play loop, or are left
  // for the screen to read; a key press hides the menu hand until the mouse
  // moves again; a button held through a clear counts once pressed again.
  {
    using namespace keen4::platform;
    HeadlessPlatform mp;
    Set(&mp);
    IN_ClearKeysDown();
    mp.SetMouse(100, 50, 0);
    IN_PumpEvents();
    MouseInfo m;
    IN_ReadMouse(&m);
    CHECK(m.inside && m.x == 100 && m.y == 50 && m.shown && m.held == 0);
    mp.SetMouse(100, 50, kMouseLeft);
    IN_PumpEvents();
    CHECK(LastScan == sc_Return && Keyboard[sc_Return]);
    mp.SetMouse(100, 50, 0);
    IN_PumpEvents();
    CHECK(!Keyboard[sc_Return] && IN_TakeMouseClicks() == kMouseLeft && IN_TakeMouseClicks() == 0);

    mp.PushKey(sc_Space, true);
    mp.PushKey(sc_Space, false);
    IN_PumpEvents();
    IN_ReadMouse(&m);
    CHECK(!m.shown);
    mp.SetMouse(101, 50, 0);
    IN_PumpEvents();
    IN_ReadMouse(&m);
    CHECK(m.shown);

    IN_ClearKeysDown();
    IN_SetMouseKeys(false);
    mp.SetMouse(101, 50, kMouseRight);
    IN_PumpEvents();
    CHECK(LastScan == sc_None && IN_TakeMouseClicks() == kMouseRight);
    IN_ClearKeysDown();
    IN_PumpEvents();
    IN_ReadMouse(&m);
    CHECK(m.held == 0 && IN_TakeMouseClicks() == 0);
    mp.SetMouse(101, 50, 0);
    IN_PumpEvents();
    mp.SetMouse(101, 50, kMouseRight);
    IN_PumpEvents();
    IN_ReadMouse(&m);
    CHECK(m.held == kMouseRight && IN_TakeMouseClicks() == kMouseRight);
    mp.SetMouse(101, 50, 0);
    IN_SetMouseKeys(true);

    // The hand is drawn over the shown frame, never into the framebuffer, and
    // follows the mouse without the screen being drawn again.
    mp.SetMouse(100, 50, 0);
    IN_PumpEvents();
    VW_ClearVideo(8);
    VW_ShowHand(true);
    VW_UpdateScreen();
    auto at = [&](int x, int y) { return mp.last_frame()[y * SCREENWIDTH + x]; };
    CHECK(vw_framebuffer[50 * SCREENWIDTH + 100] == 8);
    CHECK(at(100, 50) == 0 && at(101, 51) == 15 && at(99, 50) == 8);  // the fingertip
    int frames = mp.frames();
    mp.SetMouse(200, 120, 0);
    IN_PumpEvents();
    VW_UpdateHand();
    CHECK(mp.frames() == frames + 1 && at(100, 50) == 8 && at(200, 120) == 0);
    VW_UpdateHand();
    CHECK(mp.frames() == frames + 1);  // nothing moved
    VW_ShowHand(false);
    VW_UpdateScreen();
    CHECK(at(200, 120) == 8);
    IN_ClearKeysDown();
    Set(nullptr);
  }

  // Two gamepads: each keeps its id and can be read on its own, oldest first;
  // the newest still drives Controls[0].
  {
    using namespace keen4::platform;
    HeadlessPlatform two;
    Set(&two);
    Controls[0] = ctrl_Keyboard1;
    GamepadState a, b;
    a.connected = b.connected = true;
    two.SetGamepadAt(0, a);
    two.SetGamepadAt(1, b);
    IN_PumpEvents();
    CHECK(IN_GamepadCount() == 2 && Controls[0] == ctrl_Joystick1);
    std::uint32_t ida = IN_GamepadId(0), idb = IN_GamepadId(1);
    CHECK(ida && idb && ida != idb && !IN_GamepadId(2));
    b.buttons = kPadA | kPadLeft | kPadStart;
    two.SetGamepadAt(1, b);
    ControlInfo info;
    word buttons;
    CHECK(IN_ReadGamepad(idb, &info, &buttons));
    CHECK(info.button0 && !info.button1 && info.xaxis == -1 && info.yaxis == 0);
    CHECK(buttons == (1 | kJoyStart));
    CHECK(IN_ReadGamepad(ida, &info, &buttons) && !info.button0 && info.xaxis == 0 && buttons == 0);
    a.y = -0.9f;  // stick up
    two.SetGamepadAt(0, a);
    CHECK(IN_ReadGamepad(ida, &info, &buttons) && info.yaxis == -1);
    // unplugging the first keeps the second's id
    two.SetGamepadAt(0, GamepadState{});
    CHECK(IN_GamepadCount() == 1 && IN_GamepadId(0) == idb);
    CHECK(!IN_ReadGamepad(ida, &info, &buttons) && buttons == 0);
    two.SetGamepadAt(1, GamepadState{});
    IN_PumpEvents();
    CHECK(Controls[0] == ctrl_Keyboard1);
    IN_ClearKeysDown();
    Set(nullptr);
  }

  // Co-op split screen: one view until the Keens are too far apart for it,
  // split side by side or stacked by where they are further apart, back to
  // one view after they fit again for half a second and the divider slid away.
  {
    CoopSplit s{};
    auto box = [](int x, int y) { return CoopBox{x, y, x + 16, y + 32}; };
    CoopSplitStep(&s, box(100, 100), box(300, 100), 3);  // 216 apart: fits in 256
    CHECK(s.mode == coop_shared);
    CoopSplitStep(&s, box(100, 100), box(360, 120), 3);  // 276 across
    CHECK(s.mode == coop_sidebyside && CoopSplitSize(s) == 160);
    CoopSplitStep(&s, box(100, 100), box(330, 100), 3);  // closer, but not by the margin
    CHECK(s.mode == coop_sidebyside && s.fittics == 0);
    CoopSplit t{};
    CoopSplitStep(&t, box(100, 0), box(110, 170), 3);  // 202 up and down
    CHECK(t.mode == coop_stacked && CoopSplitSize(t) == 100);
    int tics = 0;
    while (s.mode != coop_shared && tics < 200) {
      CoopSplitStep(&s, box(100, 100), box(200, 100), 3);
      tics += 3;
      if (s.slidetics) CHECK(CoopSplitSize(s) >= 160 && CoopSplitSize(s) <= 320);
    }
    CHECK(s.mode == coop_shared && tics >= COOP_MERGETICS + COOP_SLIDETICS);
    CHECK(tics < COOP_MERGETICS + COOP_SLIDETICS + 9);
    // moving apart again while the divider slides keeps the split
    CoopSplitStep(&s, box(0, 100), box(400, 100), 3);
    for (int i = 0; i < 14; i++) CoopSplitStep(&s, box(100, 100), box(200, 100), 3);
    CHECK(s.slidetics > 0);
    CoopSplitStep(&s, box(0, 100), box(400, 100), 3);
    CHECK(s.mode == coop_sidebyside && s.slidetics == 0 && CoopSplitSize(s) == 160);
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

  // Smooth motion follows the object, not the image corner: an inchworm
  // stepping 8 pixels right swaps its flat frame (art at +0) for its hump
  // (art at +8), and the hump must not start 8 pixels behind and slide.
  {
    using keen4::render::Scene;
    using keen4::render::SceneSprite;
    using keen4::render::SpriteMotion;
    int worm = 0, other = 0;
    Scene a, b;
    a.map = b.map = 1;
    a.sprites.push_back(SceneSprite{&worm, 333, 100, 50, 0, 8, 2, false});
    b.sprites.push_back(SceneSprite{&worm, 334, 116, 46, 8, 4, 2, false});
    SpriteMotion motion;
    float x = -1, y = -1;
    motion.Update(a, 0.0);
    CHECK(motion.Position(a.sprites[0], 0.0, &x, &y) && x == 100 && y == 50);
    // a step half a second after the last one glides over half a second,
    // and the hump's head (right edge) starts where the flat frame's was
    motion.Update(a, 0.125);
    motion.Update(b, 0.5);
    CHECK(motion.Position(b.sprites[0], 0.5, &x, &y) && x == 108 && y == 46);
    CHECK(motion.Position(b.sprites[0], 0.75, &x, &y) && x == 112);
    CHECK(motion.Position(b.sprites[0], 1.0, &x, &y) && x == 116);
    CHECK(motion.Position(b.sprites[0], 2.0, &x, &y) && x == 116);
    // after a pause the next step glides at the usual pace
    motion.Update(b, 2.0);
    motion.Update(a, 2.125);
    CHECK(motion.Position(a.sprites[0], 2.375, &x, &y) && x == 104);
    CHECK(motion.Position(a.sprites[0], 2.625, &x, &y) && x == 100);
    // an actor that speeds up is followed at once
    SpriteMotion quick;
    quick.Update(a, 0.0);
    quick.Update(b, 0.5);
    quick.Update(a, 0.5625);
    CHECK(quick.Position(a.sprites[0], 0.625, &x, &y) && x == 100);
    // a jump bigger than kMaxStep is not smoothed
    Scene far = b;
    far.sprites[0].x = 400;
    motion.Update(far, 3.0);
    CHECK(motion.Position(far.sprites[0], 3.0, &x, &y) && x == 400);
    x = -1;
    SceneSprite stranger{&other, 334, 116, 46, 8, 4, 2, false};
    CHECK(!motion.Position(stranger, 3.0, &x, &y) && x == -1);
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
                                                   "sprite2 124 tom124.png\n"
                                                   "tile16 7 ai7.png\r\n";
    keen4::render::ArtLibrary art;
    art.SetDirectories({(root / "curated").string(), (root / "ai").string()});
    art.Reload();
    CHECK(art.layers() == 2 && art.layerEntries(0) == 2 && art.layerEntries(1) == 4);
    keen4::render::ArtLibrary::Source src;
    using keen4::render::AssetKind;
    CHECK(art.Find(AssetKind::Sprite, 124, &src) && src.layer == 0 && src.ax == 8 && src.ay == 4 &&
          fs::path(src.path).filename() == "keen.png");
    CHECK(art.Find(AssetKind::Sprite, 124, &src, 1) && src.layer == 1);
    CHECK(art.Find(AssetKind::Sprite, 125, &src) && src.layer == 1);
    CHECK(art.Find(AssetKind::Tile16, 7, &src) && src.layer == 1 && fs::path(src.path).filename() == "ai7.png");
    CHECK(!art.Find(AssetKind::Tile16M, 7, &src));
    CHECK(!art.Find(AssetKind::Sprite, 126, &src));
    // Tom's copy of a sprite (co-op) is an asset of its own
    CHECK(art.Find(AssetKind::Sprite2, 124, &src) && src.layer == 1 && fs::path(src.path).filename() == "tom124.png");
    CHECK(!art.Find(AssetKind::Sprite2, 125, &src));
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
    CHECK(std::strcmp(Name(Language::Portuguese), "PORTUGU\xCAS (BRASIL)") == 0);
    CHECK(std::strcmp(Name(Language::English), "ENGLISH") == 0);
    Set(Language::English);
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

  // Steam data search: libraryfolders.vdf lists extra libraries (escaped
  // Windows paths), and the install is found whatever its folder-name case.
  {
    using namespace keen4::files;
    std::vector<std::string> paths = ParseLibraryFolders(
        "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n"
        "\t\t\"apps\"\n\t\t{\n\t\t\t\"7\"\t\t\"123\"\n\t\t}\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"/run/media/games\"\n\t}\n}\n");
    CHECK(paths.size() == 2 && paths[0] == "C:\\Program Files (x86)\\Steam" && paths[1] == "/run/media/games");

    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "keen4_steam_test";
    fs::remove_all(tmp);
    fs::path root = tmp / "Steam", library = tmp / "Library2";
    fs::create_directories(root / "steamapps");
    fs::path game = library / "SteamApps" / "common" / "Commander Keen" / "BASE4";
    fs::create_directories(game);
    std::ofstream(game / "Keen4e.exe") << "x";
    std::string escaped;
    for (char c : library.string()) escaped += c == '\\' ? std::string("\\\\") : std::string(1, c);
    std::ofstream(root / "steamapps" / "libraryfolders.vdf")
        << "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" << escaped << "\"\n\t}\n}\n";
    std::vector<std::string> libraries = SteamLibraries({root.string(), root.string(), (tmp / "missing").string()});
    CHECK(libraries.size() == 2);
    CHECK(KeenInLibrary(root.string()).empty());
    std::string found = libraries.size() == 2 ? KeenInLibrary(libraries[1]) : "";
    CHECK(!found.empty() && fs::equivalent(found, game) && !FindIn(found, "KEEN4E.EXE").empty());
    fs::remove_all(tmp);
  }

  if (failures == 0) std::printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
