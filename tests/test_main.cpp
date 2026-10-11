#include <cstdio>
#include <cstring>

#include "engine/version.h"
#include "platform/headless/headless_platform.h"

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

  if (failures == 0) std::printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
