#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "engine/version.h"
#include "platform/headless/headless_platform.h"
#include "render/art.h"

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

  if (failures == 0) std::printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
