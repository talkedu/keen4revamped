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

  keen4::platform::HeadlessTimer timer;
  timer.Advance(70);
  CHECK(timer.Tics() == 70);

  keen4::platform::HeadlessInput input;
  input.SetKey(0x39, true);
  CHECK(input.KeyDown(0x39));
  CHECK(!input.KeyDown(0x1C));
  CHECK(!input.KeyDown(200));

  if (failures == 0) std::printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
