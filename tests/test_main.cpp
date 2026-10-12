#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "engine/lang.h"
#include "engine/version.h"
#include "id/id_ca.h"
#include "id/id_vw.h"
#include "platform/headless/headless_platform.h"
#include "render/art.h"
#include "render/scene.h"

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
    int worm = 0, other = 0;
    Scene prev, cur;
    prev.sprites.push_back(SceneSprite{&worm, 333, 100, 50, 0, 8, 2, false});
    cur.sprites.push_back(SceneSprite{&worm, 334, 116, 46, 8, 4, 2, false});
    float x = -1, y = -1;
    CHECK(keen4::render::InterpolateSprite(prev, cur.sprites[0], 0, 48, &x, &y) && x == 108 && y == 46);
    CHECK(keen4::render::InterpolateSprite(prev, cur.sprites[0], 1, 48, &x, &y) && x == 116 && y == 46);
    CHECK(keen4::render::InterpolateSprite(prev, cur.sprites[0], 0.5f, 48, &x, &y) && x == 112);
    x = -1;
    CHECK(!keen4::render::InterpolateSprite(prev, cur.sprites[0], 0.5f, 4, &x, &y) && x == -1);
    SceneSprite stranger{&other, 334, 116, 46, 8, 4, 2, false};
    CHECK(!keen4::render::InterpolateSprite(prev, stranger, 0.5f, 48, &x, &y) && x == -1);
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
