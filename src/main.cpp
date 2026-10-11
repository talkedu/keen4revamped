#include <cstdio>

#include "engine/version.h"

int main() {
  std::printf("%s\n", keen4::VersionString());
  return 0;
}
