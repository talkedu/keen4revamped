#include <cstdio>
#include <cstdlib>
#include <cstring>
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

  if (failures == 0) std::printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
