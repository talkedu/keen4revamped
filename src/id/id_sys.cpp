#include "id/id_sys.h"

#include "id/id_in.h"
#include "id/id_sd.h"
#include "id/id_vw.h"
#include "platform/platform.h"

void SD_ServiceWithoutAudio();

namespace {
std::uint64_t lastTicks;
bool started;
}  // namespace

void SYS_Startup() {
  lastTicks = keen4::platform::Get().Ticks70();
  started = true;
}

void SYS_Poll() {
  auto& plat = keen4::platform::Get();
  if (started) {
    std::uint64_t now = plat.Ticks70();
    TimeCount += static_cast<longword>(now - lastTicks);
    lastTicks = now;
  }
  IN_PumpEvents();
  VW_UpdateHand();
  SD_ServiceWithoutAudio();
  if (plat.QuitRequested()) Quit(nullptr);
}

void SYS_Idle() {
  SYS_Poll();
  keen4::platform::Get().Delay(1);
}

void SYS_WaitTics(int tics) {
  std::uint64_t target = keen4::platform::Get().Ticks70() + static_cast<std::uint64_t>(tics > 0 ? tics : 0);
  do {
    SYS_Idle();
  } while (keen4::platform::Get().Ticks70() < target);
}
