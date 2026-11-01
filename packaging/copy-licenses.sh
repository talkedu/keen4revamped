#!/bin/sh
# Copies the licences that go with a package into DEST (a licenses/ folder):
# the game's own (GPL-2.0-or-later), the vendored libraries' and SDL3's.
#
# Usage: copy-licenses.sh BUILD_DIR DEST   (run from the source tree)
set -eu

build=$1
dest=$2
mkdir -p "$dest"
cp LICENSE "$dest/keen4revamped.txt"
cp third_party/README.md "$dest/third_party.md"
cp third_party/nuked-opl3/LICENSE "$dest/nuked-opl3.txt"
if [ -f "$build/_deps/sdl3-src/LICENSE.txt" ]; then
  cp "$build/_deps/sdl3-src/LICENSE.txt" "$dest/SDL3.txt"
fi
