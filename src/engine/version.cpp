#include "engine/version.h"

// Set by CMakeLists.txt from project(... VERSION).
#ifndef KEEN4_VERSION
#define KEEN4_VERSION "0.0.0"
#endif

namespace keen4 {

const char* VersionString() { return "keen4revamped " KEEN4_VERSION; }
const char* VersionNumber() { return KEEN4_VERSION; }

bool IsReleaseBuild() {
#ifdef KEEN4_RELEASE_BUILD
  return true;
#else
  return false;
#endif
}

}  // namespace keen4
