#pragma once

namespace keen4 {

// "keen4revamped 1.2.0"
const char* VersionString();
// "1.2.0", the project version in CMakeLists.txt
const char* VersionNumber();
// True for the builds the release workflow publishes (it passes -DKEEN4_VERSION).
bool IsReleaseBuild();

}  // namespace keen4
