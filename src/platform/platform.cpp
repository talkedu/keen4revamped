#include "platform/platform.h"

namespace keen4::platform {

namespace {
IPlatform* g_platform = nullptr;
}  // namespace

IPlatform& Get() { return *g_platform; }
void Set(IPlatform* platform) { g_platform = platform; }

std::vector<Resolution> StandardResolutions() {
  return {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
}

}  // namespace keen4::platform
