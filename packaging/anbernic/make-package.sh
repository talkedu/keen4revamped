#!/bin/sh
# Packs an anbernic-preset build as a PortMaster-style port zip:
#
#   keen4revamped.sh            launcher (goes in the ports folder)
#   keen4revamped/keen4         the game, Linux aarch64
#   keen4revamped/gamedata/     where players copy their Keen 4 files
#   keen4revamped/port.json     PortMaster metadata
#   keen4revamped/README.md, docs/, licenses/
#
# Usage: make-package.sh BUILD_DIR OUTPUT.zip [UPDATE.tar.gz]   (run from the source tree)
set -eu

build=$1
out=$2
tarball=${3:-}
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

port="$stage/keen4revamped"
mkdir -p "$port/gamedata" "$port/docs" "$port/licenses"
cp "$build/keen4" "$port/keen4"
cp packaging/anbernic/keen4revamped.sh "$stage/keen4revamped.sh"
chmod +x "$port/keen4" "$stage/keen4revamped.sh"
cp packaging/anbernic/port.json README.md "$port"
cp packaging/anbernic/gamedata-README.txt "$port/gamedata/README.txt"
cp docs/ART.md docs/AUDIO.md docs/LANGUAGES.md "$port/docs"
sh packaging/copy-licenses.sh "$build" "$port/licenses"

rm -f "$out"
case $out in /*) dest=$out ;; *) dest="$PWD/$out" ;; esac
(cd "$stage" && zip -qr -X "$dest" keen4revamped.sh keen4revamped)
echo "wrote $out"

# The port folder alone, as a .tar.gz, for the in-game updater of older
# handheld releases (it unpacks with tar, which on these devices may not read
# .zip). Current handheld builds have no updater (KEEN4_UPDATER=OFF).
if [ -n "$tarball" ]; then
  case $tarball in /*) dest=$tarball ;; *) dest="$PWD/$tarball" ;; esac
  tar -czf "$dest" -C "$stage" keen4revamped
  echo "wrote $tarball"
fi
