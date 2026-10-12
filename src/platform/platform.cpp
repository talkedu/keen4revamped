#include "platform/platform.h"

namespace keen4::platform {

namespace {
IPlatform* g_platform = nullptr;
}  // namespace

IPlatform& Get() { return *g_platform; }
void Set(IPlatform* platform) { g_platform = platform; }

}  // namespace keen4::platform
